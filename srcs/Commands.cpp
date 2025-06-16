#include "Server.hpp"
#include "Replies.hpp"

void Server::cap() {
    std::string arg = _client->getArg();
    if (arg.empty())
        return;

    std::istringstream iss(arg);
    std::string subcmd;
    iss >> subcmd;

    if (subcmd == "LS") {
        sendToClient(_clientFd, ":localhost CAP * LS :");
        sendToClient(_clientFd, ":localhost CAP * END");
    }
    else if (subcmd == "REQ") {
        sendToClient(_clientFd, ":localhost CAP * NAK :");
        sendToClient(_clientFd, ":localhost CAP * END");
    }
    else if (subcmd == "LIST") {
        sendToClient(_clientFd, ":localhost CAP * LIST :");
        sendToClient(_clientFd, ":localhost CAP * END");
    }
    else if (subcmd == "END") {
        sendToClient(_clientFd, ":localhost CAP * END");
    }
}

void Server::ping() {
	// if (_client->getArg().empty()) {
	// 	sendToClient(_clientFd, "409 :No origin specified");
	// 	return;
	// }
	// std::string response = "PONG :" + _client->getArg();
	// sendToClient(_clientFd, response);
	return;
}

void Server::pong() {
	return;
}

void Server::pass() {
	if (std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end()) {
		std::cout << "[DEBUG] Commande ignorée (client marqué pour suppression): " << _clientFd << std::endl;
		return;
	}
	if (_client->isRegistered()) {
		sendReply(ERR_ALREADYREGISTRED, _client, "PASS", "", "You may not reregister");
		return;
	}
	if (_client->getArg().empty()) {
		sendReply(ERR_NEEDMOREPARAMS, _client, "PASS", "", "Not enough parameters");
		return;
	}
	std::string inputPass = _client->getArg();
	inputPass.erase(inputPass.find_last_not_of(" \r\n") + 1);
	if (inputPass == _password) {
		_client->setPasswordOk(true);
		_client->setPassErrorSent(false);
		std::cout << "[DEBUG] setPasswordOk(true) → " << _client->isPasswordOk() << std::endl;
	} else {
		sendReply(ERR_PASSWDMISMATCH, _client, "", "", "Password incorrect");
		_client->setPasswordOk(false);
	}
}

void Server::nick() {
	if (std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
		return;

	if (_client->getArg().empty()) {
		sendReply(ERR_NONICKNAMEGIVEN, _client, "*", "", "No nickname given");
		return;
	}

	std::string newNick = _client->getArg();
	size_t firstSpace = newNick.find_first_of(" \t\r\n");
	if (firstSpace != std::string::npos)
		newNick = newNick.substr(0, firstSpace);

	// Vérifie si le nickname est déjà utilisé
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		if (it->second->getNickname() == newNick && it->first != _clientFd) {
			sendReply(ERR_NICKNAMEINUSE, _client, "*", newNick, "Nickname is already in use");
			return;
		}
	}

	std::string oldNick = _client->getNickname();
	_client->setNickname(newNick);

	std::cout << "[DEBUG] setNickname(" << newNick << ") → " << _client->getNickname() << std::endl;

	std::string prefix;
	if (oldNick.empty())
		prefix = "*";
	else
		prefix = oldNick;

	std::string username = _client->getUsername();
	if (username.empty())
		username = "*";

	std::string nickMsg = ":" + prefix + "!" + username + "@localhost NICK :" + newNick;
	sendToClient(_clientFd, nickMsg);

	// Si l'utilisateur a aussi complété USER et PASS, on enregistre
	if (_client->isPasswordOk() && !_client->getUsername().empty() && !_client->isRegistered()) {
		_client->registerUser(newNick, username, _client->getRealname());
		sendReply(RPL_WELCOME, _client, newNick, "", "Welcome to the IRC server!");
	}
}


void Server::user() {
	if (std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
		return;
	if (_client->isRegistered()) {
		sendReply(462, _client, "", "", "You may not reregister");
		return;
	}
	std::string args = _client->getArg();
	std::string& buffer = _client->getBuffer();

	size_t realNamePos = args.find(" :");
	if (realNamePos == std::string::npos) {
		realNamePos = buffer.find(":");
		if (realNamePos != std::string::npos)
			args += buffer.substr(realNamePos);
	}
	std::istringstream iss(args);
	std::string username, hostname, servername, realname;
	if (!(iss >> username >> hostname >> servername)) {
		sendReply(ERR_NEEDMOREPARAMS, _client, "USER", "", "Not enough parameters");
		return;
	}
	if (realNamePos != std::string::npos)
		realname = args.substr(realNamePos + 2);
	else {
		sendReply(ERR_NEEDMOREPARAMS, _client, "USER", "", "Not enough parameters");
		return;
	}
	_client->setUsername(username);
	_client->setRealname(realname);

	// Vérifie que tout est prêt pour l'enregistrement
	if (_client->isPasswordOk() && _client->hasNick()) {
		_client->registerUser(_client->getNickname(), username, realname);

		// Réponses de bienvenue (RFC)
		sendReply(RPL_WELCOME, _client, _client->getNickname(), "", "Welcome to the IRC server!");
		sendReply(RPL_YOURHOST, _client, _client->getNickname(), "", "Your host is irc.ft_irc, running version 1.0");
		sendReply(RPL_CREATED, _client, _client->getNickname(), "", "This server was created in June 2025");
		sendReply(RPL_MYINFO, _client, _client->getNickname(), "", "irc.ft_irc 1.0 o o");

		std::cout << "[DEBUG] Client enregistré : " << _client->getNickname() << std::endl;
	}

	std::cout << "[DEBUG] setUsername(" << username << ") → " << _client->getUsername() << std::endl;
	std::cout << "[DEBUG] setRealname(" << realname << ") → " << _client->getRealname() << std::endl;
}

std::string trim(const std::string& str) {
	size_t first = str.find_first_not_of(" \r\n\t");
	size_t last = str.find_last_not_of(" \r\n\t");
	if (first == std::string::npos || last == std::string::npos)
		return "";
	return str.substr(first, last - first + 1);
}


void Server::privmsg() {
	if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
		return;

	const std::string& rawArg = _client->getArg();
	if (rawArg.empty()) {
		sendReply(411, _client, "", "", "No recipient given (PRIVMSG)");
		_client->eraseBuf();
		return;
	}

	std::string targetNick, message;
	size_t colonPos = rawArg.find(':');

	if (colonPos != std::string::npos) {
		targetNick = trim(rawArg.substr(0, colonPos));
		message = rawArg.substr(colonPos + 1) + _client->getBuffer();
	} else {
		targetNick = trim(rawArg);
		message = _client->getBuffer();
		if (message.empty() || message[0] != ':') {
			sendReply(412, _client, "", "", "No text to send");
			_client->eraseBuf();
			return;
		}
		message.erase(0, 1);
	}

	if (message.empty()) {
		sendReply(412, _client, "", "", "No text to send");
		_client->eraseBuf();
		return;
	}
	if (message[0] == ' ')
		message.erase(0, 1);

	std::cout << "[DEBUG] Recherche du destinataire : '" << targetNick << "'\n";

	Client* target = getClientByNick(targetNick);

	if (!target) {
		// 🔁 Cas channel
		if (targetNick[0] == '#') {
			std::map<std::string, Channel>::iterator it = _channels.find(targetNick);
			if (it == _channels.end()) {
				sendReply(ERR_NOSUCHNICK, _client, targetNick, "", "No such nick/channel");
				_client->eraseBuf();
				return;
			}
			Channel& chan = it->second;

			// Vérifie que l'utilisateur est bien dans le channel
			if (!chan.isMember(_client)) {
				sendReply(ERR_CANNOTSENDTOCHAN, _client, targetNick, "", "Cannot send to channel");
				_client->eraseBuf();
				return;
			}

			// Envoie à tous les membres sauf l'émetteur
			std::string fullMsg = ":" + _client->getPrefix() + " PRIVMSG " + targetNick + " :" + message + "\r\n";
			const std::set<Client*>& members = chan.getMembers();
			for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
				if (*it != _client)
					send((*it)->getFd(), fullMsg.c_str(), fullMsg.length(), 0);
			}
			std::cout << "[DEBUG] PRIVMSG envoyé à tous les membres de " << targetNick << " : " << fullMsg;
			_client->eraseBuf();
			return;
		} else {
			sendReply(ERR_NOSUCHNICK, _client, targetNick, "", "No such nick/channel");
			_client->eraseBuf();
			return;
		}
	}

	// 🔁 Cas message privé entre deux nicks
	std::string fullMsg = ":" + _client->getPrefix() + " PRIVMSG " + target->getNickname() + " :" + message + "\r\n";
	if (send(target->getFd(), fullMsg.c_str(), fullMsg.length(), 0) < 0)
		std::cerr << "Erreur d'envoi à " << target->getFd() << std::endl;

	std::cout << "[DEBUG] PRIVMSG envoyé à " << target->getNickname() << " (fd=" << target->getFd() << ") : " << fullMsg;
	_client->eraseBuf();
}


void Server::join() {
	if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
		return;
	std::string channelName = _client->getArg();
	if (channelName.empty() || channelName[0] != '#') {
		sendToClient(_clientFd, "ERROR :Invalid channel name");
		return;
	}
	std::pair<std::map<std::string, Channel>::iterator, bool> result =
		_channels.insert(std::make_pair(channelName, Channel(channelName)));
	Channel& chan = result.first->second;
	if (result.second) {
		chan.addOperator(_client);
		std::string modeMsg = ":" + _client->getPrefix() + " MODE " + channelName + " +o " + _client->getNickname();
		sendToClient(_clientFd, modeMsg);
	}
	if (!chan.isMember(_client)) {
		chan.join(_client);
		std::string joinMsg = ":" + _client->getPrefix() + " JOIN :" + channelName;
		const std::set<Client*>& members = chan.getMembers();
		for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
			sendToClient((*it)->getFd(), joinMsg);

		std::string nickList;
		for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
			nickList += (*it)->getNickname() + " ";
		sendReply(RPL_NAMREPLY, _client, "=", channelName, nickList);
		sendReply(RPL_ENDOFNAMES, _client, channelName, "", "End of /NAMES list.");
	}
}

void Server::part() {
	if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
		return;
	std::string channelName = _client->getArg();
	if (channelName.empty() || channelName[0] != '#') {
		sendToClient(_clientFd, "ERROR :Invalid channel name");
		return;
	}
	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end()) {
		sendReply(ERR_NOSUCHCHANNEL, _client, channelName, "", "No such channel");
		return;
	}
	Channel& chan = it->second;
	if (!chan.isMember(_client)) {
		sendReply(ERR_NOTONCHANNEL, _client, channelName, "", "You're not on that channel");
		return;
	}
	std::string partMsg = ":" + _client->getPrefix() + " PART " + channelName;
	const std::set<Client*>& members = chan.getMembers();
	for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
		sendToClient((*it)->getFd(), partMsg);
	chan.part(_client);
	if (chan.getMemberCount() == 0) {
		_channels.erase(channelName);
	}
}

void Server::quit() {
	if (!_client)
		return;
	std::string quitMessage = _client->getArg().empty() ? "Client Quit" : _client->getArg();
	std::string quitMsg = ":" + _client->getPrefix() + " QUIT :" + quitMessage;
	for (std::map<std::string, Channel>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
		Channel& chan = it->second;
		if (!chan.isMember(_client))
			continue;
		const std::set<Client*>& members = chan.getMembers();
		for (std::set<Client*>::const_iterator mit = members.begin(); mit != members.end(); ++mit) {
			if (*mit != _client)
				sendToClient((*mit)->getFd(), quitMsg);
		}
		chan.part(_client);
	}
	std::cout << "[DEBUG] QUIT reçu → ajout à _clientsToRemove: fd[" << _clientFd << "]" << std::endl;
	_clientsToRemove.push_back(_clientFd);
}

void Server::mode() {
	if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
		return;

	const std::string& args = _client->getArg();
	if (args.empty()) {
		sendReply(ERR_NEEDMOREPARAMS, _client, "MODE", "", "Not enough parameters");
		return;
	}

	std::istringstream iss(args);
	std::string channelName, modeStr, param;
	iss >> channelName >> modeStr;
	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end()) {
		sendReply(ERR_NOSUCHCHANNEL, _client, channelName, "", "No such channel");
		return;
	}

	Channel& chan = it->second;

	// Si aucun mode à appliquer → envoyer les modes actuels
	if (modeStr.empty()) {
		std::string currentModes = "+";
		if (chan.isInviteOnly()) currentModes += "i";
		if (chan.isTopicProtected()) currentModes += "t";
		if (chan.hasKey()) currentModes += "k";
		if (chan.hasUserLimit()) currentModes += "l";
		sendToClient(_clientFd, std::string(":") + SERVER_NAME + " 001 " + _client->getNickname() + " :Welcome to the IRC server\r\n");
		return;
	}
	if (!chan.isOperator(_client)) {
		sendReply(ERR_CHANOPRIVSNEEDED, _client, channelName, "", "You're not channel operator");
		return;
	}
	bool adding = true;
	while (!modeStr.empty()) {
		char flag = modeStr[0];
		modeStr.erase(0, 1);
		switch (flag) {
			case '+':
				adding = true;
				break;
			case '-':
				adding = false;
				break;
			case 'i':
				chan.setInviteOnly(adding);
				break;
			case 't':
				chan.setTopicProtected(adding);
				break;
			case 'k':
				if (adding) {
					iss >> param;
					if (param.empty()) {
						sendReply(ERR_NEEDMOREPARAMS, _client, "MODE", "", "Need key parameter for +k");
						return;
					}
					chan.setKey(param);
				} else {
					chan.removeKey();
				}
				break;
			case 'l':
				if (adding) {
					iss >> param;
					if (param.empty()) {
						sendReply(ERR_NEEDMOREPARAMS, _client, "MODE", "", "Need limit parameter for +l");
						return;
					}
					int limit = std::atoi(param.c_str());
					chan.setUserLimit(limit);
				} else {
					chan.removeUserLimit();
				}
				break;
			default:
				sendReply(ERR_UNKNOWNMODE, _client, std::string(1, flag), "", "is unknown mode char to me");
				return;
		}
	}
	sendToClient(_clientFd, ":" + _client->getPrefix() + " MODE " + channelName + " " + args);
}


void Server::topic() {
	return;
}

void Server::list() {
	return;
}

void Server::invite() {
	if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
		return;
	std::vector<std::string> args = ft_split(_client->getArg(), ' ');
	if (args.size() < 2) {
		sendReply(ERR_NEEDMOREPARAMS, _client, "INVITE", "", "Not enough parameters");
		return;
	}
	const std::string& targetNick = args[0];
	const std::string& channelName = args[1];
	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end()) {
		sendReply(ERR_NOSUCHCHANNEL, _client, channelName, "", "No such channel");
		return;
	}
	Channel& chan = it->second;
	if (!chan.isOperator(_client)) {
		sendReply(ERR_CHANOPRIVSNEEDED, _client, channelName, "", "You're not channel operator");
		return;
	}
	Client* target = getClientByNick(targetNick);
	if (!target) {
		sendReply(ERR_NOSUCHNICK, _client, targetNick, "", "No such nick/channel");
		return;
	}
	if (chan.isMember(target)) {
		sendReply(ERR_USERONCHANNEL, _client, targetNick, channelName, "is already on channel");
		return;
	}
	chan.invite(target);
	std::string inviteMsg = ":" + _client->getPrefix() + " INVITE " + targetNick + " :" + channelName;
	sendToClient(target->getFd(), inviteMsg);
	sendReply(RPL_INVITING, _client, targetNick, channelName, "");
}


void Server::kick() {
	if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
		return;
	if (!_client->isRegistered()) {
		sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
		return;
	}
	std::vector<std::string> args = ft_split(_client->getArg(), ' ');
	if (args.size() < 2) {
		sendReply(ERR_NEEDMOREPARAMS, _client, "KICK", "", "Not enough parameters");
		return;
	}
	std::string channelName = args[0];
	std::string targetNick = args[1];
	std::string comment = "Kicked";
	size_t colon = _client->getArg().find(':');
	if (colon != std::string::npos)
		comment = _client->getArg().substr(colon + 1);
	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end()) {
		sendReply(ERR_NOSUCHCHANNEL, _client, channelName, "", "No such channel");
		return;
	}
	Channel& channel = it->second;
	if (!channel.isOperator(_client)) {
		sendReply(ERR_CHANOPRIVSNEEDED, _client, channelName, "", "You're not channel operator");
		return;
	}
	Client* target = getClientByNick(targetNick);
	if (!target || !channel.isMember(target)) {
		sendReply(ERR_USERNOTINCHANNEL, _client, targetNick, channelName, "They aren't on that channel");
		return;
	}
	std::string kickMsg = ":" + _client->getPrefix() + " KICK " + channelName + " " + targetNick + " :" + comment;
	channel.sendToAll(kickMsg);
	channel.part(target);
}

void Server::notice() {
	return;
}
