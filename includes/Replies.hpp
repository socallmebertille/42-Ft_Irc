#ifndef REPLIES_HPP
#define REPLIES_HPP

#include "Replies.hpp"
#include "Client.hpp"
#include "Server.hpp"
#include <sstream>
#include <string>

class Client;
class Server;

// === Numeric Replies ===
#define RPL_WELCOME        001
#define ERR_NOSUCHNICK     401
#define ERR_NICKNAMEINUSE  433
#define ERR_NONICKNAMEGIVEN 431
#define ERR_NEEDMOREPARAMS 461
#define RPL_NAMREPLY       353
#define RPL_ENDOFNAMES     366



#endif
