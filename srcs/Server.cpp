#include "Server.hpp"

Server::Server(int port, const std::string& password):
_port(port), _serverSocket(-1), _epollFd(-1), _clientFd(-1), _password(password),
_channels(), _clients(), _client(NULL), _clientsToRemove()
{
	// std::cout << "[DEBUG]" << "Serveur IRC créé sur le port " << _port
	// 		  << " avec mot de passe : " << _password << std::endl << std::endl;
	initServerSocket();
}

Server::~Server()
{
	close(_serverSocket);
	close(_epollFd);
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		close(it->first);
		delete it->second;
	}
}

// ========== STATIC MEMBERS INITIALIZATION ==========
const std::string Server::_type[18] = {
    "CAP", "PASS", "NICK", "USER", "PRIVMSG", "JOIN", "PART", "QUIT",
    "MODE", "TOPIC", "LIST", "INVITE", "KICK", "NOTICE", "PING", "PONG",
    "USERHOST", "WHOIS"
};

Server::CommandFunc Server::_function[18] = {
    &Server::cap, &Server::pass, &Server::nick, &Server::user, &Server::privmsg,
    &Server::join, &Server::part, &Server::quit, &Server::mode, &Server::topic,
    &Server::list, &Server::invite, &Server::kick, &Server::notice, &Server::ping, &Server::pong,
    &Server::userhost, &Server::whois
};

// ========== SOCKET CONFIGURATION ==========
void Server::setNonBlocking(int fd) {
    // non-blocking mode = allow multiple connections without blocking
    // modif_opt_socket (socker_id, modif_flag, active_non-blockant_mode)
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        throw std::runtime_error("Erreur fcntl() pour mettre en non-bloquant");
    }
}

void Server::initServerSocket() {
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);// socket_init (IPv4, TCP, default protocol)
    if (_serverSocket < 0)
        throw std::runtime_error("socket() failed");
    setNonBlocking(_serverSocket);
    // SO_REUSEADDR = reuse address/port even after shutdown
    int yes = 1;
    if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        throw std::runtime_error("setsockopt() failed");
    }
    sockaddr_in addr;// adress struct for IPv4
    addr.sin_family = AF_INET;// address family (IPv4)
    addr.sin_addr.s_addr = INADDR_ANY;// bind to any address (for listenning ALL clients)
    addr.sin_port = htons(_port);// set port to sever & convert to network format (= big-endian)
    if (bind(_serverSocket, (sockaddr*)&addr, sizeof(addr)) < 0) // bind the socket to the address
        throw std::runtime_error("bind() failed");
    if (listen(_serverSocket, SOMAXCONN) < 0)// put the socket in listening mode & SOMAXCONN = max number of pending connections
        throw std::runtime_error("listen() failed");
    _epollFd = epoll_create1(0);// create the epoll file descriptor
    if (_epollFd < 0)
        throw std::runtime_error("epoll_create1() failed");
    struct epoll_event ev; // struct to monitor events on fd
    ev.events = EPOLLIN | EPOLLET; // EPOLLIN (on server socket) = notif when new client, EPOLLET = edge-triggered mode
    ev.data.fd = _serverSocket;
    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, _serverSocket, &ev) < 0) // add the server socket to epoll with EPOLLIN and EPOLLET
        throw std::runtime_error("epoll_ctl() failed");
}

// ========== CONNECTION MANAGEMENT ==========
void Server::handleNewConnection() {
    // EDGE-TRIGGERED MODE: loop until accept() returns EAGAIN or EWOULDBLOCK
    while (true) {
        sockaddr_in clientAddr; // adress struct to get ip & port of the client
        socklen_t len = sizeof(clientAddr);
        int clientFd = accept(_serverSocket, (sockaddr*)&clientAddr, &len);// get new client connection in a fd
        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)// no more clients to accept
                break;
            else
                throw std::runtime_error("accept() failed");
        }
        setNonBlocking(clientFd);
        struct epoll_event ev;// struct to monitor events on the new client fd
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = clientFd;
        if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, clientFd, &ev) < 0)// add the new client fd to epoll with EPOLLIN and EPOLLET
            throw std::runtime_error("epoll_ctl() failed on new client");

        std::string ip = inet_ntoa(clientAddr.sin_addr);// convert the client IP address from binary to string format
        Client* newClient = new Client(clientFd, ip);
        _clients.insert(std::make_pair(clientFd, newClient));

        std::cout << GREEN << "ENTER of client : " << RESET;
        std::cout << "fd[" << clientFd << "], ip[" << ip << "]" << std::endl;
    }
}

void Server::disconnectClient(int fd) {
	if (std::find(_clientsToRemove.begin(), _clientsToRemove.end(), fd) == _clientsToRemove.end())
		_clientsToRemove.push_back(fd);
}

void Server::cleanupClients() {
	for (size_t i = 0; i < _clientsToRemove.size(); ++i) {
		int fd = _clientsToRemove[i];
		// std::cout << "[DEBUG] CLEANUP client fd[" << fd << "]" << std::endl;
		std::map<int, Client*>::iterator it = _clients.find(fd);
		if (it != _clients.end()) {
			std::cout << RED << "CLEANUP client fd[" << fd << "] nickname[" << it->second->getNickname() << "]" << RESET << std::endl;
			std::map<std::string, Channel>::iterator chanIt = _channels.begin();
			while (chanIt != _channels.end()) {
				Channel& chan = chanIt->second;
				if (chan.isMember(it->second)) {
					chan.part(it->second);
					if (chan.getMemberCount() == 0) {
						std::map<std::string, Channel>::iterator toErase = chanIt;
						++chanIt;
						_channels.erase(toErase);
						continue;
					}
				}
				++chanIt;
			}
			delete it->second;
			_clients.erase(it);
		}
		epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL);
		close(fd);
	}
	_clientsToRemove.clear();
}

// ========== CLIENT UTILITIES ==========
Client* Server::getClientByNick(const std::string& nickname) {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second->getNickname() == nickname)
            return it->second;
    }
    return NULL;
}

void Server::run() {
    struct epoll_event events[MAX_EVENTS];
    while (true) {
        // nb_events = wait_for_events (fd_to_monitor, events, max_events, infinite timeout)
        int nfds = epoll_wait(_epollFd, events, MAX_EVENTS, -1);
        if (nfds < 0)
            throw std::runtime_error("epoll_wait() failed");
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            if (fd == _serverSocket) {
                handleNewConnection();
                continue;
            }
            if (events[i].events & EPOLLIN) {
                char buffer[1024];
                _client = _clients[fd];
                int bytesRead = recv(fd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);

                if (bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    // std::cout << "[RECV FD " << fd << "] >>> [" << buffer << "]" << std::endl;
                    _clients[fd]->appendToBuffer(buffer);
                    handleCommand(fd);
                }
                else if (bytesRead == 0 || (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
                    // std::cout << "[DEBUG] Client fd[" << fd << "] marked for removal (recv error or disconnect)" << std::endl;
                    // évite de continuer avec ce fd
                    if (std::find(_clientsToRemove.begin(), _clientsToRemove.end(), fd) == _clientsToRemove.end())
                        _clientsToRemove.push_back(fd);
                    break;
                }
            }
        }
        cleanupClients();
    }
}

void Server::handleCommand(int clientFd) {
	_clientFd = clientFd;
	_client = _clients[clientFd];
	if (!_client)
		return;

	while (true) {
		std::string& buf = _client->getBuffer();
		size_t pos = buf.find("\r\n");
		size_t posNc = buf.find("\n");
		std::string fullLine;
		if (pos != std::string::npos)
		{
			fullLine = buf.substr(0, pos);
			buf.erase(0, pos + 2);
			_client->setClientType(false); // Client normal (avec \r\n)
		}
		else if (posNc != std::string::npos)
		{
			fullLine = buf.substr(0, posNc);
			buf.erase(0, posNc + 1);
			_client->setClientType(true); // Client spécial (e.g. netcat sans \r)
		}
		else {
            if (!buf.empty()) { //if CTRL+D was pressed, buf might still contain data
                // std::cout << "[DEBUG] Partial command in buffer due to disconnection: [" << buf << "]" << std::endl;
            }
            break;
        }
        if (fullLine.empty() || fullLine == "\r") {
            // std::cerr << "[DEBUG] Ligne vide ignorée pour fd " << clientFd << std::endl;
            return;
        }
		std::cout << "[PARSE FD " << clientFd << "] >>> [" << fullLine << "]" << std::endl;
		_client->parseLine(fullLine);
		if (_client->getCmd().empty())
			continue;
		execCommand();
		checkRegistration();
	}
}

void Server::execCommand() {
    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;

    const std::string& cmd = _client->getCmd();
    if (cmd.empty()) {
        sendReply(ERR_UNKNOWNCOMMAND, _client, "*", "", "Empty command");
        return;
    }
    if (!_client->isPasswordOk() && cmd != "PASS" && cmd != "CAP" && cmd != "JOIN") {
        if (!_client->hasSentPassError()) {
            sendReply(ERR_PASSWDMISMATCH, _client, "*", "", "Password required");
            _client->setPassErrorSent(true);
        }
		_clientsToRemove.push_back(_clientFd);
        return;
    }
    if (cmd == "USER" && !_client->hasNick()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "USER", "", "You must set a nickname first");
        return;
    }
    if (cmd == "CAP") {
        cap();
        return;
    }
    for (int i = 0; i < 18; i++) {
        if (cmd == _type[i]) {
            try {
                (this->*_function[i])();
            } catch (const std::exception& e) {
                std::cerr << "[CRASH] Exception during command " << cmd << ": " << e.what() << std::endl;
            }
            checkRegistration();
            return;
        }
    }
    sendReply(ERR_UNKNOWNCOMMAND, _client, cmd, "", "Unknown command");
}

// ========== USER REGISTRATION ==========
void Server::checkRegistration() {
    if (!_client->isRegistered()
        && _client->isPasswordOk()
        && _client->hasNick()
        && !_client->getNickname().empty()
        && _client->hasUser()
        && !_client->getUsername().empty()) {
        // std::cout << "[DEBUG] → registerUser called" << std::endl;
        _client->registerUser(_client->getNickname(), _client->getUsername(), _client->getRealname());
        sendReply(RPL_WELCOME, _client, "", "", "Welcome to the IRC server!");
        // std::cout << "[DEBUG] Client enregistré : " << _client->getNickname() << std::endl;
    }
    else if (!_client->isPasswordOk() && _client->hasNick() && _client->hasUser()) {
        if (!_client->hasSentPassError()) {
            sendReply(ERR_PASSWDMISMATCH, _client, "*", "", "Password required");
            _client->setPassErrorSent(true);
        }
    }
}
