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
        sendToClient(_clientFd, ":localhost CAP * LS :\r\n");
        sendToClient(_clientFd, ":localhost CAP * END\r\n");
    }
    else if (subcmd == "REQ") {
        sendToClient(_clientFd, ":localhost CAP * NAK :\r\n");
        sendToClient(_clientFd, ":localhost CAP * END\r\n");
    }
    else if (subcmd == "LIST") {
        sendToClient(_clientFd, ":localhost CAP * LIST :\r\n");
        sendToClient(_clientFd, ":localhost CAP * END\r\n");
    }
    else if (subcmd == "END") {
        sendToClient(_clientFd, ":localhost CAP * END\r\n");
    }
}


void Server::ping() {
	// if (_client->getArg().empty()) {
	// 	sendToClient(_clientFd, "409 :No origin specified\r\n");
	// 	return;
	// }
	// std::string response = "PONG :" + _client->getArg() + "\r\n";
	// sendToClient(_clientFd, response);
	return;
}

void Server::pong() {
	return;
}

void Server::pass() {
	if (_client->isRegistered()) {
		sendToClient(_clientFd, "462 :You may not reregister\r\n");
		return;
	}
	if (_client->getArg().empty()) {
		sendToClient(_clientFd, "461 PASS :Not enough parameters\r\n");
		return;
	}
	std::string inputPass = _client->getArg();
	inputPass.erase(inputPass.find_last_not_of(" \r\n") + 1);
	if (inputPass == _password) {
		_client->setPasswordOk(true);
		_client->setPassErrorSent(false);
	} else {
		sendToClient(_clientFd, "464 :Password incorrect\r\n");
		_client->setPasswordOk(false);
	}
}

void Server::nick() {
	if (_client->getArg().empty()) {
		sendToClient(_clientFd, "431 * :No nickname given\r\n");
		return;
	}
	std::string newNick = _client->getArg();
	size_t firstSpace = newNick.find_first_of(" \t\r\n");
	if (firstSpace != std::string::npos)
		newNick = newNick.substr(0, firstSpace);
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		if (it->second->getNickname() == newNick && it->first != _clientFd) {
			sendToClient(_clientFd, "433 * " + newNick + " :Nickname is already in use\r\n");
			return;
		}
	}
	std::string oldNick = _client->getNickname();
	_client->setNickname(newNick);
	std::string prefix = oldNick.empty() ? "*" : oldNick;
	std::string nickMsg = ":" + prefix + "!" + (_client->getUsername().empty() ? "*" : _client->getUsername()) + "@localhost NICK :" + newNick + "\r\n";
	sendToClient(_clientFd, nickMsg);
}

void Server::user() {
	if (_client->isRegistered()) {
		sendToClient(_clientFd, "462 :You may not reregister\r\n");
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
		sendToClient(_clientFd, "461 USER :Not enough parameters\r\n");
		return;
	}
	if (realNamePos != std::string::npos) {
		realname = args.substr(realNamePos + 2);
	} else {
		sendToClient(_clientFd, "461 USER :Not enough parameters\r\n");
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
			sendToClient(_clientFd, "412 :No text to send\r\n");
			_client->eraseBuf();
			return;
		}
		message.erase(0, 1);
	}
	Client* target = getClientByNick(_client->getArg());
	if (!target) {
		sendToClient(_clientFd, "401 " + _client->getArg() + " :No such nick/channel\r\n");
		_client->eraseBuf();
		return;
	}
	if (message[0] == ' ')
		message.erase(0, 1);
	std::string fullMsg = ":" + _client->getPrefix() + " PRIVMSG " + target->getNickname() + " :" + message + "\r\n";
	sendToClient(target->getFd(), fullMsg);
	_client->eraseBuf();
}

void Server::join() {
	if (_client->getArg().empty() || _client->getArg()[0] != '#') {
		sendToClient(_clientFd, "ERROR :Invalid channel name\r\n");
		return;
	}
	std::pair<std::map<std::string, Channel>::iterator, bool> result = _channels.insert(std::make_pair(_client->getArg(), Channel(_client->getArg())));
	Channel& chan = result.first->second;
	if (!chan.isMember(_client)) {
		chan.join(_client);
		std::string joinMsg = ":" + _client->getPrefix() + " JOIN :" + _client->getArg() + "\r\n";
		const std::set<Client*>& members = chan.getMembers();
		for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
			sendToClient((*it)->getFd(), joinMsg);
		std::string nickList;
		for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
			nickList += (*it)->getNickname() + " ";
		sendReply(353, _client, "=", _client->getArg(), nickList);
		sendReply(366, _client, _client->getArg(), "", "End of /NAMES list.\r\n");
	}
}

void Server::part() {
	if (_client->getArg().empty() || _client->getArg()[0] != '#') {
		sendToClient(_clientFd, "ERROR :Invalid channel name\r\n");
		return;
	}
	std::map<std::string, Channel>::iterator it = _channels.find(_client->getArg());
	if (it == _channels.end()) {
		sendToClient(_clientFd, "ERROR :No such channel\r\n");
		return;
	}
	Channel& chan = it->second;
	if (!chan.isMember(_client)) {
		sendToClient(_clientFd, "ERROR :You're not in that channel\r\n");
		return;
	}
	std::string partMsg = ":" + _client->getPrefix() + " PART " + _client->getArg() + "\r\n";
	const std::set<Client*>& members = chan.getMembers();
	for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
		sendToClient((*it)->getFd(), partMsg);
	chan.part(_client);
	if (chan.getMemberCount() == 0) {
		_channels.erase(_client->getArg());
	}
}

void Server::quit() {
	return;
}

void Server::mode() {
	if (_client->getArg().empty()) {
		sendToClient(_clientFd, "461 MODE :Not enough parameters\r\n");
		return;
	}
	sendToClient(_clientFd, "324 " + _client->getNickname() + " +\r\n");
}

void Server::topic() {
	return;
}

void Server::list() {
	return;
}

void Server::invite() {
	return;
}

void Server::kick() {
	return;
}

void Server::notice() {
	return;
}
