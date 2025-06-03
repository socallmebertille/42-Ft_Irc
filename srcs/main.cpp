#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"

#define WHITE   "\033[0;37m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[38;5;220m"
#define CYAN    "\033[38;5;45m"
#define RED     "\033[38;5;196m"
#define RESET   "\033[0m"

int main(int ac, char** av)
{
    if (ac != 3) {
        std::cerr << RED << "./ircserv <port> <password>" << RESET << std::endl;
        return 1;
    }
    (void)av;
    return 0;
}