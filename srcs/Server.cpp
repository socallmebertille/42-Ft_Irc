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
        delete it->second;
    }
}

void Server::setNonBlocking(int fd) {
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        throw std::runtime_error("Erreur fcntl() pour mettre en non-bloquant");
    }
}

void Server::initServerSocket() {
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverSocket < 0)
        throw std::runtime_error("socket() failed");
    setNonBlocking(_serverSocket);
    int yes = 1;
    if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        throw std::runtime_error("setsockopt() failed");
    }
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(_port);
    if (bind(_serverSocket, (sockaddr*)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("bind() failed");
    if (listen(_serverSocket, SOMAXCONN) < 0)
        throw std::runtime_error("listen() failed");
    _epollFd = epoll_create1(0);
    if (_epollFd < 0)
        throw std::runtime_error("epoll_create1() failed");
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = _serverSocket;
    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, _serverSocket, &ev) < 0)
        throw std::runtime_error("epoll_ctl() failed");
}

void Server::handleNewConnection() {
    while (true) {
        sockaddr_in clientAddr;
        socklen_t len = sizeof(clientAddr);
        int clientFd = accept(_serverSocket, (sockaddr*)&clientAddr, &len);
        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else
                throw std::runtime_error("accept() failed");
        }
        setNonBlocking(clientFd);
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = clientFd;
        if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, clientFd, &ev) < 0)
            throw std::runtime_error("epoll_ctl() failed on new client");

        std::string ip = inet_ntoa(clientAddr.sin_addr);
        Client* newClient = new Client(clientFd, ip);
        _clients.insert(std::make_pair(clientFd, newClient));

        std::cout << GREEN << "ENTER of client : " << RESET;
        std::cout << "fd[" << clientFd << "], ip[" << ip << "]" << std::endl;
    }
}

void Server::run() {
    struct epoll_event events[MAX_EVENTS];
    while (true) {
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
                    if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                    if (errno == EINTR) continue;
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
    _clientFd = clientFd;
    while (!_client->getBuffer().empty()) {
        _client->parseLine();
        if (_client->getCmd().empty()) {
            break;
        }
        execCommand();
    }
}

void Server::parseAndExecuteCommand(const std::string& line) {
    if (line.empty()) {
        return;
    }

    // La commande a déjà été parsée dans Client::parseLine(), donc on peut juste exécuter
    execCommand();
}

void Server::execCommand() {
    if (_client->getCmd().empty()) {
        sendToClient(_clientFd, "421 * :Empty command\r\n");
        return;
    }

    std::string const& cmd = _client->getCmd();

    // Toujours autoriser CAP
    if (cmd == "CAP") {
        cap();
        return;
    }

    // Vérifier le mot de passe pour toutes les autres commandes
    if (!_client->isPasswordOk() && cmd != "PASS") {
        sendToClient(_clientFd, "464 * :Password required\r\n");
        return;
    }

    // Exécuter la commande si elle existe
    for (int i = 0; i < 16; i++) {
        if (cmd == _type[i]) {
            (this->*_function[i])();

            // Vérifier si on peut enregistrer l'utilisateur
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

