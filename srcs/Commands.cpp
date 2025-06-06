#include "Server.hpp"
#include "Replies.hpp"

void Server::cap() {
    std::cout << "Executing CAP command." << std::endl;
    // Implementation for CAP command
}

void Server::pass() {
	if (_client->getArg().empty()) {
		sendToClient(_clientFd, "461 PASS :Not enough parameters\r\n");
		return;
	}
	if (_client->isRegistered()) {
		sendToClient(_clientFd, "462 :You may not reregister\r\n");
		return;
	}
	if (_client->getArg() == _password) {
		std::cout << "[DEBUG] mot de passe correct, on setPasswordOk(true)\n";
		_client->setPasswordOk(true);
	} else {
		sendToClient(_clientFd, "464 :Password incorrect");
	}
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
    if (_client->getArg().empty()) {
        sendToClient(_clientFd, "411 :No recipient given");
        _client->eraseBuf();
        return;
    }
    std::string message;
    size_t pos = _client->getArg().find(":");
    if (pos != std::string::npos) {
        std::string argCpy(_client->getArg());
        std::string part1(argCpy.substr(pos + 1)), part2(_client->getBuffer());
        if (_client->getSpace() == 1)
            part1 += " ";
        message = part1 + part2;
        argCpy = argCpy.substr(0, pos);
        _client->setArg(argCpy);
    }
    else {
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
    std::string fullMsg = ":" + _client->getPrefix() + " PRIVMSG " + _client->getNickname() + " :" + PINK + message + RESET;
    sendToClient(target->getFd(), fullMsg);
    // sendToClient(_clientFd, ":" + client->getPrefix() + " PRIVMSG " + target->getNickname() + " :" + message);
    _client->eraseBuf();
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
        std::string joinMsg = ":" + _client->getPrefix() + " JOIN :" + _client->getArg();
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
    std::string partMsg = ":" + _client->getPrefix() + " PART " + _client->getArg();
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
    // Implementation for MODE command
}

void Server::topic() {
    std::cout << "Executing TOPIC command." << std::endl;
    // Implementation for TOPIC command
}

void Server::list() {
    std::cout << "Executing LIST command." << std::endl;
    // Implementation for LIST command
}

void Server::invite() {
    std::cout << "Executing INVITE command." << std::endl;
    // Implementation for INVITE command
}

void Server::kick() {
    std::cout << "Executing KICK command." << std::endl;
    // Implementation for KICK command
}

void Server::notice() {
    std::cout << "Executing NOTICE command." << std::endl;
    // Implementation for NOTICE command
}

void Server::ping() {
    std::cout << "Executing PING command." << std::endl;
    // Implementation for PING command
}

void Server::pong() {
    std::cout << "Executing PONG command." << std::endl;
    // Implementation for PONG command
}