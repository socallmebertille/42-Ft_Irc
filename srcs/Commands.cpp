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
	if (_client->isRegistered()) {
		sendToClient(_clientFd, "462 :You may not reregister");
		return;
	}
	if (_client->getArg().empty()) {
		sendToClient(_clientFd, "461 PASS :Not enough parameters");
		return;
	}
	std::string inputPass = _client->getArg();
	inputPass.erase(inputPass.find_last_not_of(" \r\n") + 1);
	if (inputPass == _password) {
		_client->setPasswordOk(true);
		_client->setPassErrorSent(false);
	} else {
		sendToClient(_clientFd, "464 :Password incorrect");
		_client->setPasswordOk(false);
	}
}

void Server::nick() {
	if (_client->getArg().empty()) {
		sendToClient(_clientFd, "431 * :No nickname given");
		return;
	}
	std::string newNick = _client->getArg();
	size_t firstSpace = newNick.find_first_of(" \t\r\n");
	if (firstSpace != std::string::npos)
		newNick = newNick.substr(0, firstSpace);
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		if (it->second->getNickname() == newNick && it->first != _clientFd) {
			sendToClient(_clientFd, "433 * " + newNick + " :Nickname is already in use");
			return;
		}
	}
	std::string oldNick = _client->getNickname();
	_client->setNickname(newNick);
	std::string prefix = oldNick.empty() ? "*" : oldNick;
	std::string nickMsg = ":" + prefix + "!" + (_client->getUsername().empty() ? "*" : _client->getUsername()) + "@localhost NICK :" + newNick;
	sendToClient(_clientFd, nickMsg);
}

void Server::user() {
	if (_client->isRegistered()) {
		sendToClient(_clientFd, "462 :You may not reregister");
		return;
	}
	std::string args = _client->getArg();
	std::string& buffer = _client->getBuffer();
	size_t realNamePos = args.find(" :");
	if (realNamePos == std::string::npos) {
		realNamePos = buffer.find(":");
		if (realNamePos != std::string::npos) {
			args += buffer.substr(realNamePos);
		}
	}
	std::istringstream iss(args);
	std::string username, hostname, servername, realname;
	if (!(iss >> username >> hostname >> servername)) {
		sendToClient(_clientFd, "461 USER :Not enough parameters");
		return;
	}
	if (realNamePos != std::string::npos) {
		realname = args.substr(realNamePos + 2);
	} else {
		sendToClient(_clientFd, "461 USER :Not enough parameters");
		return;
	}
	_client->setUsername(username);
	_client->setRealname(realname);
}

void Server::privmsg() {
	if (_client->getArg().empty()) {
		sendToClient(_clientFd, "411 :No recipient given\r\n");
		_client->eraseBuf();
		return;
	}
	std::string message;
	if (_client->getArg().find(":") == std::string::npos) {
		sendToClient(_clientFd, "412 :No text to send\r\n");
		_client->eraseBuf();
		return;
	}
	size_t pos = _client->getArg().find(":");
	if (pos != std::string::npos) {
		std::string argCpy(_client->getArg());
		std::string& buffer = _client->getBuffer();
		message = argCpy.substr(pos + 1) + buffer;
		argCpy = argCpy.substr(0, pos);
		_client->setArg(argCpy);
	} else {
		std::istringstream iss(_client->getBuffer());
		std::getline(iss, message);
		if (message.empty() || message[0] != ':') {
			sendToClient(_clientFd, "412 :No text to send");
			_client->eraseBuf();
			return;
		}
		message.erase(0, 1);
	}
	Client* target = getClientByNick(_client->getArg());
	if (!target) {
		sendToClient(_clientFd, "401 " + _client->getArg() + " :No such nick/channel");
		_client->eraseBuf();
		return;
	}
	if (message[0] == ' ')
		message.erase(0, 1);
	std::string fullMsg = ":" + _client->getPrefix() + " PRIVMSG " + target->getNickname() + " :" + message;
	sendToClient(target->getFd(), fullMsg);
	_client->eraseBuf();
}

void Server::join() {
	if (_client->getArg().empty() || _client->getArg()[0] != '#') {
		sendToClient(_clientFd, "ERROR :Invalid channel name");
		return;
	}
	std::pair<std::map<std::string, Channel>::iterator, bool> result = _channels.insert(std::make_pair(_client->getArg(), Channel(_client->getArg())));
	Channel& chan = result.first->second;
	if (result.second) {
		chan.addOperator(_client);
		std::string modeMsg = ":" + _client->getPrefix() + " MODE " + _client->getArg() + " +o " + _client->getNickname() + "\r\n";
		sendToClient(_clientFd, modeMsg);
	}
	if (!chan.isMember(_client)) {
		chan.join(_client);
		std::string joinMsg = ":" + _client->getPrefix() + " JOIN :" + _client->getArg();
		const std::set<Client*>& members = chan.getMembers();
		for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
			sendToClient((*it)->getFd(), joinMsg);
		std::string nickList;
		for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
			nickList += (*it)->getNickname() + " ";
		sendReply(353, _client, "=", _client->getArg(), nickList);
		sendReply(366, _client, _client->getArg(), "", "End of /NAMES list.");
	}
}

void Server::part() {
	if (_client->getArg().empty() || _client->getArg()[0] != '#') {
		sendToClient(_clientFd, "ERROR :Invalid channel name");
		return;
	}
	std::map<std::string, Channel>::iterator it = _channels.find(_client->getArg());
	if (it == _channels.end()) {
		sendToClient(_clientFd, "ERROR :No such channel");
		return;
	}
	Channel& chan = it->second;
	if (!chan.isMember(_client)) {
		sendToClient(_clientFd, "ERROR :You're not in that channel");
		return;
	}
	std::string partMsg = ":" + _client->getPrefix() + " PART " + _client->getArg();
	const std::set<Client*>& members = chan.getMembers();
	for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
		sendToClient((*it)->getFd(), partMsg);
	chan.part(_client);
	if (chan.getMemberCount() == 0) {
		_channels.erase(_client->getArg());
	}
}

void Server::quit() {
	// std::string quitMessage = _client->getArg().empty() ? "Client Quit" : _client->getArg();
	// std::string quitMsg = ":" + _client->getPrefix() + " QUIT :" + quitMessage + "\r\n";

	// for (std::map<std::string, Channel>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
	// 	Channel& chan = it->second;
	// 	if (chan.isMember(_client)) {
	// 		const std::set<Client*>& members = chan.getMembers();
	// 		for (std::set<Client*>::const_iterator mit = members.begin(); mit != members.end(); ++mit) {
	// 			if (*mit != _client)
	// 				sendToClient((*mit)->getFd(), quitMsg);
	// 		}
	// 		chan.part(_client);
	// 	}
	// }

	// // Fermer la connexion et supprimer le client
	// disconnectClient(_clientFd);
	return;
}


void Server::mode() {
	if (_client->getArg().empty()) {
		sendToClient(_clientFd, "461 MODE :Not enough parameters");
		return;
	}
	sendToClient(_clientFd, "324 " + _client->getNickname());
}

void Server::topic() {
	return;
}

void Server::list() {
	return;
}

void Server::invite() {
	std::vector<std::string> args = ft_split(_client->getArg(), ' ');
	if (args.size() < 2) {
		sendToClient(_clientFd, "ERROR :Not enough parameters for INVITE\r\n");
		return;
	}
	std::string targetNick = args[0];
	std::string channelName = args[1];

	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end()) {
		sendToClient(_clientFd, "ERROR :No such channel\r\n");
		return;
	}
	Channel& chan = it->second;
	if (!chan.isOperator(_client)) {
		sendToClient(_clientFd, "ERROR :You're not channel operator\r\n");
		return;
	}
	Client* target = getClientByNick(targetNick);
	if (!target) {
		sendToClient(_clientFd, "ERROR :No such user\r\n");
		return;
	}
	if (chan.isMember(target)) {
		sendToClient(_clientFd, "ERROR :User is already in the channel\r\n");
		return;
	}
	chan.invite(target);
	std::string inviteMsg = ":" + _client->getPrefix() + " INVITE " + targetNick + " :" + channelName + "\r\n";
	sendToClient(target->getFd(), inviteMsg);
	sendToClient(_clientFd, "INVITE sent\r\n");
	if (!chan.isOperator(_client)) {
		std::string errMsg = std::string(":") + SERVER_NAME + " 482 " + _client->getNickname()
			+ " " + channelName + " :You're not channel operator\r\n";
		sendToClient(_clientFd, errMsg);
		return;
	}


	// if (chan.isInviteOnly() && !chan.isInvited(_client)) {
	// 	sendToClient(_clientFd, "ERROR :Channel is invite-only\r\n");
	// 	return;
	// }

}



void Server::kick() //expulse un utilisateur d’un channel
{
	if (!_client->isRegistered()) {
		sendToClient(_clientFd, "451 " + _client->getNickname() + " :You have not registered\r\n");
		return;
	}

	std::vector<std::string> args = ft_split(_client->getArg(), ' ');
	if (args.size() < 2) {
		sendToClient(_clientFd, "461 " + _client->getNickname() + " KICK :Not enough parameters\r\n");
		return;
	}

	std::string channelName = args[0];
	std::string targetNick = args[1];

	std::string comment = "Kicked";
	size_t colon = _client->getArg().find(':');
	if (colon != std::string::npos)
		comment = _client->getArg().substr(colon + 1);

	// Channel existe ?
	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end()) {
		sendToClient(_clientFd, "403 " + channelName + " :No such channel\r\n");
		return;
	}

	Channel& channel = it->second;

	// Client est-il opérateur ?
	if (!channel.isOperator(_client)) {
		sendToClient(_clientFd, "482 " + _client->getNickname() + " " + channelName + " :You're not channel operator\r\n");
		return;
	}

	// Trouver le client cible
	Client* target = getClientByNick(targetNick);
	if (!target || !channel.isMember(target)) {
		sendToClient(_clientFd, "441 " + _client->getNickname() + " " + targetNick + " " + channelName + " :They aren't on that channel\r\n");
		return;
	}

	// Message de kick
	std::string kickMsg = ":" + _client->getPrefix() + " KICK " + channelName + " " + targetNick + " :" + comment + "\r\n";

	// Notifier tout le monde
	channel.sendToAll(kickMsg);

	// Retirer la cible du channel
	channel.part(target);
}


void Server::notice() {
	return;
}
