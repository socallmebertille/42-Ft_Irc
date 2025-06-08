#include "Server.hpp"

const std::string Server::_type[16] = {
    "CAP", "PASS", "NICK", "USER", "PRIVMSG", "JOIN", "PART", "QUIT",
    "MODE", "TOPIC", "LIST", "INVITE", "KICK", "NOTICE", "PING", "PONG"
};

Server::CommandFunc Server::_function[16] = {
    &Server::cap, &Server::pass, &Server::nick, &Server::user, &Server::privmsg,
    &Server::join, &Server::part, &Server::quit, &Server::mode, &Server::topic,
    &Server::list, &Server::invite, &Server::kick, &Server::notice, &Server::ping, &Server::pong
};

Server::Server(int port, const std::string& password):
_port(port), _serverSocket(-1), _epollFd(-1), _clientFd(-1), _password(password)
{
    std::cout << "Serveur IRC créé sur le port " << _port
              << " avec mot de passe : " << _password << std::endl << std::endl;
    initServerSocket();
}

Server::~Server()
{
    close(_serverSocket);
    close(_epollFd);
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        close(it->first);
        delete it->second; // Free the Client object
    }
}

void Server::setNonBlocking(int fd) {
    // non-blocking mode = allow multiple connections without blocking
    // modif_opt_socket (socker_id, modif_flag, active_non-blockant_mode)
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        throw std::runtime_error("Erreur fcntl() pour mettre en non-bloquant");
    }
}

void Server::initServerSocket() {
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0); // socket_init (IPv4, TCP, default protocol)
    if (_serverSocket < 0)
        throw std::runtime_error("socket() failed");
    setNonBlocking(_serverSocket);
    // SO_REUSEADDR = reuse address/port even after shutdown
    int yes = 1;
    if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        throw std::runtime_error("setsockopt() failed");
    }
    sockaddr_in addr; // adress struct for IPv4
    addr.sin_family = AF_INET; // address family (IPv4)
    addr.sin_addr.s_addr = INADDR_ANY; // bind to any address (for listenning ALL clients)
    addr.sin_port = htons(_port); // set port to sever & convert to network format (= big-endian)
    if (bind(_serverSocket, (sockaddr*)&addr, sizeof(addr)) < 0) // bind the socket to the address
        throw std::runtime_error("bind() failed");
    if (listen(_serverSocket, SOMAXCONN) < 0) // put the socket in listening mode & SOMAXCONN = max number of pending connections
        throw std::runtime_error("listen() failed");
    _epollFd = epoll_create1(0); // create the epoll file descriptor
    if (_epollFd < 0)
        throw std::runtime_error("epoll_create1() failed");
    struct epoll_event ev; // struct to monitor events on fd
    ev.events = EPOLLIN | EPOLLET; // EPOLLIN (on server socket) = notif when new client, EPOLLET = edge-triggered mode
    ev.data.fd = _serverSocket;
    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, _serverSocket, &ev) < 0) // add the server socket to epoll with EPOLLIN and EPOLLET
        throw std::runtime_error("epoll_ctl() failed");

    std::cout << "Server listening on port " << _port << std::endl;
}

void Server::handleNewConnection() {
    // EDGE-TRIGGERED MODE: loop until accept() returns EAGAIN or EWOULDBLOCK
    while (true) {
        sockaddr_in clientAddr; // adress struct to get ip & port of the client
        socklen_t len = sizeof(clientAddr);
        int clientFd = accept(_serverSocket, (sockaddr*)&clientAddr, &len); // get new client connection in a fd
        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) // no more clients to accept
                break;
            else
                throw std::runtime_error("accept() failed");
        }
        setNonBlocking(clientFd);
        struct epoll_event ev; // struct to monitor events on the new client fd
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = clientFd;
        if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, clientFd, &ev) < 0) // add the new client fd to epoll with EPOLLIN and EPOLLET
            throw std::runtime_error("epoll_ctl() failed on new client");
        std::string ip = inet_ntoa(clientAddr.sin_addr); // convert the client IP address from binary to string format
        Client* newClient = new Client(clientFd, ip);
        _clients.insert(std::make_pair(clientFd, newClient));

        // std::cout << "New client connected: " << ip << " [fd: " << clientFd << "]" << std::endl;
        std::cout << GREEN << "ENTER of client : " << RESET;
        std::cout << "fd[" << clientFd << "], nickname[" << newClient->getNickname() << "]" << std::endl;
    }
}

void Server::run() {
    struct epoll_event events[MAX_EVENTS];
    while (true) {
        // nb_events = wait_for_events (fd_to_monitor, events, max_events, infinite timeout)
        int nfds = epoll_wait(_epollFd, events, MAX_EVENTS, -1);
        if (nfds < 0) throw std::runtime_error("epoll_wait() failed");
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            if (fd == _serverSocket) {
                handleNewConnection();
                continue;
            }
            char buffer[512];
            _client = _clients[fd];
            while (true) {
                ssize_t bytesRead = recv(fd, buffer, sizeof(buffer) - 1, 0);
                if (bytesRead < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) break; // nothing to read & still non-blockant mode
                    if (errno == EINTR) continue; // signal => retry
                    closeAndRemoveClient(fd);
                    break;
                }
                else if (bytesRead == 0) {
                    closeAndRemoveClient(fd);
                    break;
                }
                buffer[bytesRead] = '\0';
                _client->setBuf(std::string(buffer, bytesRead));
                handleCommand(fd);
            }
        }
    }
}

Client* Server::getClientByNick(const std::string& nickname) {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second->getNickname() == nickname)
            return it->second;
    }
    return NULL;
}

void Server::closeAndRemoveClient(int fd)
{
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it != _clients.end())
    {
        std::cout << RED << "QUITTING of client : " << RESET;
        std::cout << "fd[" << it->first << "], nickname[" << it->second->getNickname() << "]" << std::endl;
        delete it->second;
        _clients.erase(it);
    }
    ::close(fd);
}

void Server::handleCommand(int clientFd) {
    // std::cout << "Commande brute reçue de fd " << clientFd << " : [" << line << "]" << std::endl;
    _clientFd = clientFd;
    while (! _client->getBuffer().empty() && _client->getBuffer()[0] != '\n') {
        _client->parseLine();
        execCommand();
        // std::cout << "Command executed: " << _client->getCmd() << " " << _arg << std::endl;
    }
    if (!_client->getBuffer().empty()) {
        _client->eraseBuf();
    }
}

void Server::execCommand() {
	if (_client->getCmd().empty()) {
		sendToClient(_clientFd, "421 * :Empty command\r\n");
		return;
	}

	// cmd autorisées avant validation du PASS
	const std::string allowedBeforePass[] = { "CAP", "PASS", "NICK", "USER", "PING" };
	bool isAllowedBeforePass = false;
	for (int i = 0; i < 5; ++i) {
		if (_client->getCmd() == allowedBeforePass[i]) {
			isAllowedBeforePass = true;
			break;
		}
	}
	if (!_client->isPasswordOk() && (_client->getCmd() == "CAP" || _client->getCmd() == "PING")) {
		return;
	}
	if (!_client->isPasswordOk() && !isAllowedBeforePass) {
		if (!_client->hasSentPassError()) {
			sendToClient(_clientFd, "464 * :Password required\r\n");
			_client->setPassErrorSent(true);
		}
		return;
	}
	const std::string allowedBeforeRegister[] = { "CAP", "PASS", "NICK", "USER", "PING" };
	bool isAllowedPreReg = false;
	for (int i = 0; i < 5; ++i) {
		if (_client->getCmd() == allowedBeforeRegister[i]) {
			isAllowedPreReg = true;
			break;
		}
	}
	if (!_client->isRegistered() && !isAllowedPreReg) {
		sendToClient(_clientFd, "451 * :You have not registered\r\n");
		return;
	}
	for (int i = 0; i < 16; i++) {
		if (_client->getCmd() == _type[i]) {
			(this->*_function[i])();

			if (_client->isPasswordOk() && _client->hasNick() && _client->hasUser() && !_client->isRegistered()) {
				_client->registerUser(
					_client->getNickname(),
					_client->getUsername(),
					_client->getRealname()
				);
				sendToClient(_clientFd, "001 " + _client->getNickname() + " :Welcome to the IRC server!\r\n");
			}
			return;
		}
	}
	sendToClient(_clientFd, "421 " + _client->getCmd() + " :Unknown command\r\n");
}

