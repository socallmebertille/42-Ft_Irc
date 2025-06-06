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
    typedef void (Server::*CommandFunc)();

private:
    int _port;
    int _serverSocket;
    int _epollFd;
    int _clientFd;
    std::string _password;

    std::map<std::string, Channel> _channels;
    std::map<int, Client*> _clients;
    Client* _client;
    static const std::string _type[16];
    static CommandFunc _function[16];

    void initServerSocket();
    void handleNewConnection();
    void setNonBlocking(int fd);

    Client* getClientByNick(const std::string& nickname);
    void closeAndRemoveClient(int fd);
    void handleCommand(int clientFd);

    void sendToClient(int fd, const std::string& msg);
	void sendReply(int code, Client* client, const std::string& arg1 = "", const std::string& arg2 = "", const std::string& trailing = "");

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
