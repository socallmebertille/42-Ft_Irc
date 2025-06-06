#include"Replies.hpp"
//Centraliser tous les codes de réponse IRC (001, 433, 461, etc.)

void Server::sendToClient(int fd, const std::string& msg) {
    std::string formattedMsg = msg;
    if (_client->getClientType() == false)
        formattedMsg += "\r\n";
    else
        formattedMsg += "\n";
    send(fd, formattedMsg.c_str(), formattedMsg.length(), 0);
}

void Server::sendReply(int code, Client* client, const std::string& arg1, const std::string& arg2, const std::string& trailing) {
	std::ostringstream oss;
	std::string nickname = client->getNickname();
	std::string serverName = "ircserv";

	oss << ":" << serverName << " " << code << " " << nickname;

	if (!arg1.empty())
		oss << " " << arg1;
	if (!arg2.empty())
		oss << " " << arg2;
	if (!trailing.empty())
		oss << " :" << trailing;
	sendToClient(client->getFd(), oss.str());
}

