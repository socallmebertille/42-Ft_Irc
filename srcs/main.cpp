#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include <cstdlib>
#include <sstream>
#include <colors.hpp>

bool isValidPort(const char* str, int& port) {
	std::istringstream iss(str);
	iss >> port;
	return !(iss.fail() || !iss.eof());
}

int main(int ac, char** av)
{
	if (ac != 3) {
		std::cerr << RED << "Error: ./ircserv <port> <password>" << RESET << std::endl;
		return 1;
	}
    int port;
    if (!isValidPort(av[1], port) || port <= 0 || port > 65535) {
        std::cerr << RED << "Error: port should be a valid integer between 1 and 65535." << RESET << std::endl;
        return 1;
    }
    std::string password(av[2]);
    try {
        Server server(port, password);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << RED << "Error: " << e.what() << RESET << std::endl;
        return 1;
    }
	
		// Test temporaire de la classe Client
	Client testClient(5); // fd fictif
	if (!testClient.setNickname("helloWorld"))
		std::cerr << RED << "Nickname invalid" << RESET << std::endl;
	else
		std::cout << GREEN << "Nickname set: " << testClient.getNickname() << RESET << std::endl;

	testClient.setUsername("melina");
	testClient.setRealname("Melina Motylewski");

	testClient.authenticate();

	if (testClient.isAuthenticated())
		std::cout << CYAN << "Client authenticated!" << RESET << std::endl;

    return 0;
}
