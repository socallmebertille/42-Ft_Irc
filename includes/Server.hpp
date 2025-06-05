#ifndef SERVER_HPP
# define SERVER_HPP

# include <iostream>
# include <sstream>
# include <unistd.h>
# include <map>
# include <string>
# include <fcntl.h>
# include <cstring>
# include <stdexcept>
# include <netinet/in.h>
# include <sys/epoll.h>
# include <arpa/inet.h>
# include <algorithm>
# include <unistd.h>
# include <cerrno>
# include "Client.hpp"
# include "Channel.hpp"
# include "colors.hpp"

# define MAX_EVENTS 64

class Server {
public:
    Server(int port, const std::string& password);
    ~Server();

    void run();

private:
    int _port;
    std::string _password;
    int _serverSocket;
    int _epollFd;

    std::string _commandLine, _command, _arg;
    std::map<std::string, Channel> _channels;
    std::map<int, Client*> _clients;
    int _space, _clientFd;
    Client* _client;

    void initServerSocket();
    void handleNewConnection();
    void setNonBlocking(int fd);

    void sendToClient(int fd, const std::string& msg);
    Client* getClientByNick(const std::string& nickname);
    void handleCommand(int clientFd);

    void parseLine();
    void execCommand();
    void cap();
    void pass();
    void nick();
    void user();
    void privmsg();
    void join();
    void part();
    void quit();
    void mode();
    void topic();
    void list();
    void invite();
    void kick();
    void notice();
    void ping();
    void pong();
};

#endif
