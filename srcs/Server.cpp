#include "Server.hpp"

Server::Server(int port, const std::string& password): _port(port), _password(password), _serverSocket(-1), _epollFd(-1)
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
        if (nfds < 0) {
            throw std::runtime_error("epoll_wait() failed");
        }
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            if (fd == _serverSocket) {
                handleNewConnection();
            } else {
                char buffer[512];
                ssize_t bytesRead = recv(fd, buffer, sizeof(buffer) - 1, 0);
                if (bytesRead <= 0) {
                    // Client disconnected or error
                    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
                    {
                        if (fd == it->first) {
                            std::cout << RED << "QUITTING of client : " << RESET;
                            std::cout << "fd[" << it->first << "], nickname[" << it->second->getNickname() << "]" << std::endl;
                            break;
                        }
                    }
                    close(fd);
                    _clients.erase(fd);
                    continue;
                }
                buffer[bytesRead] = '\0';
                _commandLine = buffer;
                while (_commandLine[0] != '\n' && !_commandLine.empty()) {
                    handleCommand(fd, _commandLine);
                }
                if (!_commandLine.empty()) {
                    _commandLine.erase(0, _commandLine.size());
                }
            }
        }
    }
}

void Server::sendToClient(int fd, const std::string& msg) {
    send(fd, msg.c_str(), msg.length(), 0);
}

Client* Server::getClientByNick(const std::string& nickname) {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second->getNickname() == nickname)
            return it->second;
    }
    return NULL;
}

void Server::handleCommand(int clientFd, const std::string& line) {
    // Client& client = _clients[clientFd];

	std::cout << "Commande brute reçue de fd " << clientFd << " : [" << line << "]" << std::endl;

    Client* client = _clients[clientFd];
    std::istringstream iss(line);
    std::string command, arg;
    int space(0);
    iss >> command >> arg;
    _commandLine.erase(0, command.size());
    if (_commandLine[0] == ' ') {
        _commandLine.erase(0, 1);
    }
    if (!arg.empty()) {
        if (arg[arg.size() - 1] != ' ' || arg[arg.size() - 1] != ':')
            space = 1;
        _commandLine.erase(0, arg.size());
        if (_commandLine[0] == ' ')
            _commandLine.erase(0, 1);
        if (_commandLine.empty())
            space = 0;
    }
	if (command == "CAP") {
        // Ignorer CAP pour ne pas fermer la connexion trop tôt
        return;
	}
    else if (command == "PASS") {
        if (arg.empty()) {
            sendToClient(clientFd, "461 PASS :Not enough parameters\n");
            return;
        }
        client->setPassword(arg);
        client->markPassword();
        sendToClient(clientFd, MAGENTA "NOTICE * :Password accepted\n" RESET);
    }
    else if (command == "NICK") {
        if (arg.empty()) {
            sendToClient(clientFd, "431 :No nickname given\n");
            return;
        }
        // Check nickname uniqueness
        for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
            if (it->second->getNickname() == arg && it->first != clientFd) {
                sendToClient(clientFd, "433 * " + arg + " :Nickname is already in use\n");
                return;
            }
        }
        client->setNickname(arg);
        client->markNick();
        sendToClient(clientFd, MAGENTA "NOTICE * :Nickname ");
        sendToClient(clientFd, arg);
        sendToClient(clientFd, " save\n" RESET);
    }
    else if (command == "USER") {
        if (arg.empty()) {
            sendToClient(clientFd, "461 USER :Not enough parameters\n");
            return;
        }
        client->setUsername(arg);
        client->markUser();
        sendToClient(clientFd, MAGENTA "NOTICE * :User ");
        sendToClient(clientFd, arg);
        sendToClient(clientFd, " save\n" RESET);
    }
    else if (command == "PRIVMSG") {
        if (arg.empty()) {
            sendToClient(clientFd, "411 :No recipient given\n");
            _commandLine.erase(0, _commandLine.size());
            return;
        }
        std::string message;
        size_t pos = arg.find(":");
        if (pos != std::string::npos) {
            std::string part1(arg.substr(pos + 1)), part2(_commandLine);
            if (space == 1)
                part1 += " ";
            message = part1 + part2;
            arg = arg.substr(0, pos);
        }
        else {
            std::istringstream iss(_commandLine);
            std::getline(iss, message);
            if (message.empty() || message[0] != ':') {
                sendToClient(clientFd, "412 :No text to send\n");
                _commandLine.erase(0, _commandLine.size());
                return;
            }
            message.erase(0, 1);
        }
        Client* target = getClientByNick(arg);
        if (!target) {
            sendToClient(clientFd, "401 " + arg + " :No such nick/channel\n");
            _commandLine.erase(0, _commandLine.size());
            return;
        }
        if (message[message.size() - 1] != '\n')
            message += "\n";
        if (message[0] == ' ')
            message.erase(0, 1);
        std::string fullMsg = ":" + client->getPrefix() + " PRIVMSG " + client->getNickname() + " :" + PINK + message + RESET;
        sendToClient(target->getFd(), fullMsg);
        // sendToClient(clientFd, ":" + client->getPrefix() + " PRIVMSG " + target->getNickname() + " :" + message);
        _commandLine.erase(0, _commandLine.size());
    }
    else if (command == "JOIN") {
		if (arg.empty() || arg[0] != '#') {
			sendToClient(clientFd, "ERROR :Invalid channel name\r\n");
			return;
		}
		std::pair<std::map<std::string, Channel>::iterator, bool> result = _channels.insert(std::make_pair(arg, Channel(arg)));
		Channel& chan = result.first->second;
		if (!chan.isMember(client)) {
			chan.join(client);
			sendToClient(clientFd, ":" + client->getNickname() + " JOIN " + arg + "\n");
		}
	}
	else if (command == "PART") {
        if (arg.empty() || arg[0] != '#') {
            sendToClient(clientFd, "ERROR :Invalid channel name\r\n");
            return;
        }
        std::map<std::string, Channel>::iterator it = _channels.find(arg);
        if (it == _channels.end()) {
            sendToClient(clientFd, "ERROR :No such channel\r\n");
            return;
        }

        Channel& chan = it->second;
        if (!chan.isMember(client)) {
            sendToClient(clientFd, "ERROR :You're not in that channel\r\n");
            return;
        }

        chan.part(client); // ta fonction dans Channel
        sendToClient(clientFd, ":" + client->getNickname() + " PART " + arg + "\r\n");

        // (Optionnel) Supprimer le channel s’il est vide
        if (chan.getMemberCount() == 0) {
            _channels.erase(arg);
            std::cout << RED << "Channel supprimé car vide : " << arg << RESET << std::endl;
        }
    }
    else {
        sendToClient(clientFd, "421 " + command + " :Unknown command\n");
        _commandLine.erase(0, _commandLine.size());
    }
    if (client->hasPassword() && client->hasNick() && client->hasUser() && !client->isAuthenticated()) {
        client->authenticate();
        sendToClient(clientFd, "001 " + client->getNickname() + " :Welcome to the IRC server!\n");
    }
}
