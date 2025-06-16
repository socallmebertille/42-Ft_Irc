#include "Server.hpp"
#include "Replies.hpp"
#include <sstream>  // Pour ostringstream

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
		// std::cout << "[DEBUG] Commande ignorée (client marqué pour suppression): " << _clientFd << std::endl;
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
		// std::cout << "[DEBUG] setPasswordOk(true) → " << _client->isPasswordOk() << std::endl;
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
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		if (it->second->getNickname() == newNick && it->first != _clientFd) {
			sendReply(ERR_NICKNAMEINUSE, _client, "*", newNick, "Nickname is already in use");
			return;
		}
	}
	std::string oldNick = _client->getNickname();
	_client->setNickname(newNick);
	// std::cout << "[DEBUG] setNickname(" << newNick << ") → " << _client->getNickname() << std::endl;
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
	checkRegistration();
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
    checkRegistration();
    // std::cout << "[DEBUG] setUsername(" << username << ") → " << _client->getUsername() << std::endl;
    // std::cout << "[DEBUG] setRealname(" << realname << ") → " << _client->getRealname() << std::endl;
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
    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }
    std::string args = _client->getArg();
    if (args.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "PRIVMSG", "", "Not enough parameters");
        return;
    }
    std::string target, message;
    size_t colonPos = args.find(" :");
    if (colonPos != std::string::npos) {
        target = args.substr(0, colonPos);
        message = args.substr(colonPos + 2); // +2 pour passer " :"
    } else {
        // Cas où il n'y a pas de ":" - erreur
        std::istringstream iss(args);
        if (!(iss >> target)) {
            sendReply(ERR_NEEDMOREPARAMS, _client, "PRIVMSG", "", "Not enough parameters");
            return;
        }
        sendReply(ERR_NEEDMOREPARAMS, _client, "PRIVMSG", "", "No text to send");
        return;
    }
    target = trim(target);
    if (target.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "PRIVMSG", "", "Not enough parameters");
        return;
    }
    if (message.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "PRIVMSG", "", "No text to send");
        return;
    }
    // std::cout << "[DEBUG] PRIVMSG - Target: '" << target << "', Message: '" << message << "'" << std::endl;
    if (target[0] == '#') {
        handleChannelMessage(target, message);
    } else {
        handlePrivateMessage(target, message);
    }
}

void Server::handleChannelMessage(const std::string& channelName, const std::string& message) {
    std::map<std::string, Channel>::iterator it = _channels.find(channelName);
    if (it == _channels.end()) {
        sendReply(ERR_NOSUCHCHANNEL, _client, channelName, "", "No such channel");
        return;
    }
    Channel& channel = it->second;
    if (!channel.isMember(_client)) {
        sendReply(ERR_CANNOTSENDTOCHAN, _client, channelName, "", "Cannot send to channel");
        return;
    }
    std::string fullMsg = ":" + _client->getPrefix() + " PRIVMSG " + channelName + " :" + message;
    const std::set<Client*>& members = channel.getMembers();
    int sentCount = 0;
    for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
        if (*it != _client) {
            sendToClient((*it)->getFd(), fullMsg);
            sentCount++;
        }
    }
    // std::cout << "[DEBUG] PRIVMSG canal '" << channelName << "' envoyé à " << sentCount << " membres" << std::endl;
}

void Server::handlePrivateMessage(const std::string& targetNick, const std::string& message) {
    Client* target = getClientByNick(targetNick);
    if (!target) {
        sendReply(ERR_NOSUCHNICK, _client, targetNick, "", "No such nick/channel");
        return;
    }
    if (!target->isRegistered()) {
        // std::cout << "[DEBUG] Target client not registered, skipping message" << std::endl;
        return;
    }
    std::string fullMsg = ":" + _client->getPrefix() + " PRIVMSG " + targetNick + " :" + message;
    sendToClient(target->getFd(), fullMsg);

    // std::cout << "[DEBUG] PRIVMSG privé envoyé à '" << targetNick << "' (fd=" << target->getFd() << "): " << fullMsg << std::endl;
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

		// Envoyer le topic du canal au nouveau membre (si un topic existe)
		if (!chan.getTopic().empty()) {
			// RPL_TOPIC (332) : Le topic lui-même
			sendReply(RPL_TOPIC, _client, channelName, "", chan.getTopic());

			// RPL_TOPICWHOTIME (333) : Qui a défini le topic et quand
			std::ostringstream timeStr;
			timeStr << chan.getTopicSetTime();
			sendReply(RPL_TOPICWHOTIME, _client, channelName, chan.getTopicSetBy(), timeStr.str());

			// std::cout << "[DEBUG] Topic + métadonnées envoyés à " << _client->getNickname() << " lors du JOIN" << std::endl;
		} else {
			// std::cout << "[DEBUG] Aucun topic à envoyer pour " << channelName << std::endl;
		}

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
	// std::cout << "[DEBUG] QUIT reçu → ajout à _clientsToRemove: fd[" << _clientFd << "]" << std::endl;
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

	if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
		return;

	if (!_client->isRegistered()) {
		sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
		return;
	}

	std::string args = _client->getArg();
	if (args.empty()) {
		sendReply(ERR_NEEDMOREPARAMS, _client, "TOPIC", "", "Not enough parameters");
		return;
	}
	std::istringstream iss(args);
	std::string channelName;
	iss >> channelName;

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

	size_t colonPos = args.find(" :");

	if (colonPos != std::string::npos) {
		std::string newTopic = args.substr(colonPos + 2); // +2 pour passer " :"

		if (!chan.isOperator(_client)) {
			sendReply(ERR_CHANOPRIVSNEEDED, _client, channelName, "", "You're not channel operator");
			return;
		}

		chan.setTopic(newTopic, _client->getNickname());  // Utiliser la nouvelle version avec métadonnées

		std::string topicMsg = ":" + _client->getPrefix() + " TOPIC " + channelName + " :" + newTopic;
		const std::set<Client*>& members = chan.getMembers();
		for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
			sendToClient((*it)->getFd(), topicMsg);
		}
		sendReply(RPL_TOPIC, _client, channelName, "", chan.getTopic());

		std::ostringstream timeStr;
		timeStr << chan.getTopicSetTime();
		sendReply(RPL_TOPICWHOTIME, _client, channelName, chan.getTopicSetBy(), timeStr.str());

		// std::cout << "[DEBUG] TOPIC modifié pour " << channelName << " par " << _client->getNickname() << " : " << newTopic << std::endl;
	} else {
		if (chan.getTopic().empty()) {
			sendReply(331, _client, channelName, "", "No topic is set");
		} else {
			sendReply(332, _client, channelName, "", chan.getTopic());
		}

		// std::cout << "[DEBUG] TOPIC consulté pour " << channelName << " par " << _client->getNickname() << std::endl;
	}
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

void Server::userhost() {
	// La commande USERHOST retourne des informations sur un ou plusieurs utilisateurs
	// Format: USERHOST nick1 [nick2 ...]

	if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
		return;

	if (!_client->isRegistered()) {
		sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
		return;
	}

	std::string args = _client->getArg();
	if (args.empty()) {
		sendReply(ERR_NEEDMOREPARAMS, _client, "USERHOST", "", "Not enough parameters");
		return;
	}

	// Parser les nicknames demandés
	std::istringstream iss(args);
	std::string nickname;
	std::string response;

	while (iss >> nickname) {
		Client* target = getClientByNick(nickname);
		if (target && target->isRegistered()) {
			if (!response.empty())
				response += " ";
			// Format: nick=+user@host (+ indique que l'utilisateur est disponible)
			response += target->getNickname() + "=+" + target->getUsername() + "@localhost";
		}
	}

	// Réponse USERHOST (code 302)
	sendReply(302, _client, "", "", response);
}

void Server::whois() {
	// La commande WHOIS retourne des informations détaillées sur un utilisateur
	// Format: WHOIS nick

	if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
		return;

	if (!_client->isRegistered()) {
		sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
		return;
	}

	std::string args = _client->getArg();
	if (args.empty()) {
		sendReply(ERR_NEEDMOREPARAMS, _client, "WHOIS", "", "Not enough parameters");
		return;
	}

	// Prendre le premier nickname
	std::istringstream iss(args);
	std::string nickname;
	iss >> nickname;

	Client* target = getClientByNick(nickname);
	if (!target) {
		sendReply(ERR_NOSUCHNICK, _client, nickname, "", "No such nick/channel");
		return;
	}

	if (!target->isRegistered()) {
		sendReply(ERR_NOSUCHNICK, _client, nickname, "", "No such nick/channel");
		return;
	}

	// Envoyer les informations WHOIS
	// RPL_WHOISUSER (311): nick user host * :realname
	sendReply(311, _client, nickname, target->getUsername() + " localhost *", target->getRealname());

	// RPL_WHOISSERVER (312): nick server :server info
	sendReply(312, _client, nickname, SERVER_NAME, "IRC Server");

	// RPL_ENDOFWHOIS (318): nick :End of /WHOIS list
	sendReply(318, _client, nickname, "", "End of /WHOIS list");

	// std::cout << "[DEBUG] WHOIS for " << nickname << " completed" << std::endl;
}
