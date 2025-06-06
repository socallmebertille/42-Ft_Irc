#include "Server.hpp"
#include "Replies.hpp"

void Server::cap() {
    std::cout << "Executing CAP command." << std::endl;
    // Implementation for CAP command
}

void Server::pass() {
    if (_client->getArg().empty()) {
        sendToClient(_clientFd, "461 PASS :Not enough parameters");
        return;
    }
    _client->setPassword(_client->getArg());
    _client->markPassword();
    sendToClient(_clientFd, MAGENTA "NOTICE * :Password accepted" RESET);
}

void Server::nick() {
    if (_client->getArg().empty()) {
        sendToClient(_clientFd, "431 :No nickname given");
        return;
    }
    // Check nickname uniqueness
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second->getNickname() == _client->getArg() && it->first != _clientFd) {
            sendToClient(_clientFd, "433 * " + _client->getArg() + " :Nickname is already in use");
            return;
        }
    }
    _client->setNickname(_client->getArg());
    _client->markNick();
    std::string message;
    message = MAGENTA "NOTICE * :Nickname " + _client->getArg() + " save" RESET;
    sendToClient(_clientFd, message);
}

void Server::user() {
    if (_client->getArg().empty()) {
        sendToClient(_clientFd, "461 USER :Not enough parameters");
        return;
    }
    _client->setUsername(_client->getArg());
    _client->markUser();
    std::string message;
    message = MAGENTA "NOTICE * :User " + _client->getArg() + " save" RESET;
    sendToClient(_clientFd, message);
}

void Server::privmsg() {
	if (_arg.empty()) {
		sendToClient(_clientFd, "411 :No recipient given\r\n");
		_commandLine.clear();
		return;
	}
	std::string message;
	size_t pos = _arg.find(":");
	if (pos != std::string::npos) {
		std::string part1(_arg.substr(pos + 1));
		std::string part2(_commandLine);
		if (_space == 1)
			part1 += " ";
		message = part1 + part2;
		_arg = _arg.substr(0, pos);
	} 
	else {
		std::istringstream iss(_commandLine);
		std::getline(iss, message);
		if (message.empty() || message[0] != ':') {
			sendToClient(_clientFd, "412 :No text to send\r\n");
			_commandLine.clear();
			return;
		}
		message.erase(0, 1);
	}
	if (message.size() < 2 || message.substr(message.size() - 2) != "\r\n")
		message += "\r\n";
	if (message[0] == ' ')
		message.erase(0, 1);
	std::string fullMsg = ":" + _client->getPrefix() + " PRIVMSG " + _arg + " :" + BOLD + message;

	// channel
	if (_arg[0] == '#') {
		std::map<std::string, Channel>::iterator it = _channels.find(_arg);
		if (it == _channels.end()) {
			sendToClient(_clientFd, "401 " + _arg + " :No such nick/channel\r\n");
			_commandLine.clear();
			return;
		}
		Channel& chan = it->second;
		if (!chan.isMember(_client)) {
			sendToClient(_clientFd, "404 " + _arg + " :Cannot send to channel\r\n");
			_commandLine.clear();
			return;
		}
		const std::set<Client*>& members = chan.getMembers();
		for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
			if ((*it)->getFd() != _clientFd)
				sendToClient((*it)->getFd(), fullMsg);
		}
	} 
	else {
		Client* target = getClientByNick(_arg);
		if (!target) {
			sendToClient(_clientFd, "401 " + _arg + " :No such nick/channel\r\n");
		} 
		else {
			sendToClient(target->getFd(), fullMsg);
		}
	}
	_commandLine.clear();
}

void Server::join() {
    if (_client->getArg().empty() || _client->getArg()[0] != '#') {
        sendToClient(_clientFd, "ERROR :Invalid channel name");
        return;
    }
    std::pair<std::map<std::string, Channel>::iterator, bool> result = _channels.insert(std::make_pair(_client->getArg(), Channel(_client->getArg())));
    Channel& chan = result.first->second;
    if (!chan.isMember(_client)) {
        chan.join(_client);
        std::string joinMsg = ":" + _client->getPrefix() + " JOIN :" + _client->getArg() + "\r\n";
        const std::set<Client*>& members = chan.getMembers();
        for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
            sendToClient((*it)->getFd(), joinMsg);
        }
        std::string nickList;
        for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
            nickList += (*it)->getNickname() + " ";
        }
		sendReply(353, _client, "=", _client->getArg(), nickList);
		sendReply(366, _client, _client->getArg(), "", "End of /NAMES list.");
    }	//Ces codes ne sont pas des erreurs, mais des codes de réponse standards IRC.
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
    std::string partMsg = ":" + _client->getPrefix() + " PART " + _client->getArg() + "\r\n";
    // Notifier tous les membres AVANT de retirer le client
    const std::set<Client*>& members = chan.getMembers();
    for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
        sendToClient((*it)->getFd(), partMsg);
    }
    chan.part(_client);
    if (chan.getMemberCount() == 0) {
        _channels.erase(_client->getArg());
        std::cout << RED << "Channel supprimé car vide : " << _client->getArg() << RESET << std::endl;
    }
}

void Server::quit() {
    std::cout << "Executing QUIT command." << std::endl;
    // Implementation for QUIT command
}

void Server::mode() {
    std::cout << "Executing MODE command." << std::endl;
}

void Server::topic() {
    std::cout << "Executing TOPIC command." << std::endl;
}

void Server::list() {
    std::cout << "Executing LIST command." << std::endl;
}

void Server::invite() {
    std::cout << "Executing INVITE command." << std::endl;
}

void Server::kick() {
    std::cout << "Executing KICK command." << std::endl;
}

void Server::notice() {
	if (_arg.empty()) {
		_commandLine.clear();
		return;
	}
	std::string message;
	size_t pos = _arg.find(":");
	if (pos != std::string::npos) {
		std::string part1(_arg.substr(pos + 1));
		std::string part2(_commandLine);
		if (_space == 1)
			part1 += " ";
		message = part1 + part2;
		_arg = _arg.substr(0, pos);
	} 
	else {
		std::istringstream iss(_commandLine);
		std::getline(iss, message);
		if (!message.empty() && message[0] == ':')
			message.erase(0, 1);
		else {
			_commandLine.clear();
			return;
		}
	}
	if (message.size() < 2 || message.substr(message.size() - 2) != "\r\n")
		message += "\r\n";
	if (!message.empty() && message[0] == ' ')
		message.erase(0, 1);
	std::string fullMsg = ":" + _client->getPrefix() + " NOTICE " + _arg + " :" + BOLD + message;
	if (_arg[0] == '#') {
		std::map<std::string, Channel>::iterator it = _channels.find(_arg);
		if (it != _channels.end()) {
			Channel& chan = it->second;
			const std::set<Client*>& members = chan.getMembers();
			for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
				if ((*it)->getFd() != _clientFd)
					sendToClient((*it)->getFd(), fullMsg);
			}
		}
	} 
	else {
		Client* target = getClientByNick(_arg);
		if (target)
			sendToClient(target->getFd(), fullMsg);
	}
	_commandLine.clear();
}


void Server::ping() {
    std::cout << "Executing PING command." << std::endl;

}

void Server::pong() {
    std::cout << "Executing PONG command." << std::endl;

}

