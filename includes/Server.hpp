#ifndef SERVER_HPP
# define SERVER_HPP

# include <iostream>
# include <unistd.h>
# include <map>
# include <string>
# include <fcntl.h>
# include <cstring>
# include <stdexcept>
# include <netinet/in.h>
# include <sys/epoll.h>
# include <arpa/inet.h>
# include <cerrno>
# include "Client.hpp"

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
	std::map<int, Client> _clients;

    void initServerSocket();
    void handleNewConnection();
    void setNonBlocking(int fd);
};

#endif
