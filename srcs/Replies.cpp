#include"Replies.hpp"
//Centraliser tous les codes de réponse IRC (001, 433, 461, etc.)

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

	oss << "\r\n";
	sendToClient(client->getFd(), oss.str());
}

