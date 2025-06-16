#ifndef REPLIES_HPP
#define REPLIES_HPP

#include "Replies.hpp"
#include "Client.hpp"
#include "Server.hpp"
#include <sstream>
#include <string>

class Client;
class Server;

# define SERVER_NAME "irc.ft_irc"

// === Numeric Replies ===

// 001–004 : Réponses de bienvenue
#define RPL_WELCOME           001
#define RPL_YOURHOST          002
#define RPL_CREATED           003
#define RPL_MYINFO            004

// 3xx : Réponses positives diverses
#define RPL_INVITING          341
#define RPL_NAMREPLY          353
#define RPL_ENDOFNAMES        366

// 4xx : Réponses d'erreurs client/serveur
#define ERR_NOSUCHNICK        401
#define ERR_NOSUCHCHANNEL     403
#define ERR_CANNOTSENDTOCHAN  404

#define ERR_UNKNOWNCOMMAND    421

#define ERR_NONICKNAMEGIVEN   431
#define ERR_ERRONEUSNICKNAME  432
#define ERR_NICKNAMEINUSE     433

#define ERR_USERNOTINCHANNEL  441
#define ERR_NOTONCHANNEL      442
#define ERR_USERONCHANNEL     443

#define ERR_NOTREGISTERED     451

#define ERR_NEEDMOREPARAMS    461
#define ERR_ALREADYREGISTRED  462
#define ERR_PASSWDMISMATCH    464

#define ERR_CHANNELISFULL     471
#define ERR_UNKNOWNMODE       472
#define ERR_INVITEONLYCHAN    473
#define ERR_BADCHANNELKEY     475
#define ERR_BADCHANMASK       476

#define ERR_CHANOPRIVSNEEDED  482

// === Message Macros ===
#define MSG_WELCOME(nick)              (RPL_WELCOME + std::string(" ") + (nick) + " :Welcome to the IRC server!")
#define MSG_ERR_NICKNAMEINUSE(nick)    (ERR_NICKNAMEINUSE + std::string(" ") + (nick) + " :Nickname is already in use")
#define MSG_ERR_NONICKNAMEGIVEN        (ERR_NONICKNAMEGIVEN + std::string(" * :No nickname given"))
#define MSG_ERR_NEEDMOREPARAMS(cmd)    (ERR_NEEDMOREPARAMS + std::string(" ") + (cmd) + " :Not enough parameters")
#define MSG_ERR_UNKNOWNCOMMAND(cmd)    (ERR_UNKNOWNCOMMAND + std::string(" ") + (cmd) + " :Unknown command")
#define MSG_ERR_PASSWDMISMATCH         (ERR_PASSWDMISMATCH + std::string(" :Password required"))


#endif
