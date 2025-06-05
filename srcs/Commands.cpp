#include "Server.hpp"

void Server::parseLine() {
    std::istringstream iss(_commandLine);
    if (_commandLine.find("\r\n", _commandLine.size() - 3) != std::string::npos) {
        _commandLine.erase(_commandLine.find("\r\n", _commandLine.size() - 3), 2);
        _client->setClientType(false);
    }
    else if (_commandLine.find("\n", _commandLine.size() - 2) != std::string::npos) {
        _commandLine.erase(_commandLine.find("\n", _commandLine.size() - 2), 1);
        _client->setClientType(true);
    }
    iss >> _command >> _arg;
    _commandLine.erase(0, _command.size());
    if (_commandLine[0] == ' ') {
        _commandLine.erase(0, 1);
    }
    if (!_arg.empty()) {
        if (_arg[_arg.size() - 1] != ' ' || _arg[_arg.size() - 1] != ':')
            _space = 1;
        _commandLine.erase(0, _arg.size());
        if (_commandLine[0] == ' ')
            _commandLine.erase(0, 1);
        if (_commandLine.empty())
            _space = 0;
    }
}

void Server::execCommand()
{
	std::string type[16] = {"CAP", "PASS", "NICK", "USER", "PRIVMSG", "JOIN", "PART", "QUIT", "MODE", "TOPIC", "LIST", "INVITE", "KICK", "NOTICE", "PING", "PONG"};
    void (Server::*function[16])() = {&Server::cap, &Server::pass, &Server::nick, &Server::user, &Server::privmsg, &Server::join, &Server::part, &Server::quit, &Server::mode, &Server::topic, &Server::list, &Server::invite, &Server::kick, &Server::notice, &Server::ping, &Server::pong};
	for (int i(0); i < 16; i++)
	{
		if (_command == type[i])
		{
			(this->*function[i])();
            if (_client->hasPassword() && _client->hasNick() && _client->hasUser() && !_client->isAuthenticated()) {
                _client->authenticate();
                sendToClient(_clientFd, "001 " + _client->getNickname() + " :Welcome to the IRC server!");
            }
            return ;
		}
	}
	sendToClient(_clientFd, "421 " + _command + " :Unknown command");
    _commandLine.erase(0, _commandLine.size());
}

void Server::cap() {
    std::cout << "Executing CAP command." << std::endl;
    // Implementation for CAP command
}

void Server::pass() {
    if (_arg.empty()) {
        sendToClient(_clientFd, "461 PASS :Not enough parameters");
        return;
    }
    _client->setPassword(_arg);
    _client->markPassword();
    sendToClient(_clientFd, MAGENTA "NOTICE * :Password accepted" RESET);
}

void Server::nick() {
    if (_arg.empty()) {
        sendToClient(_clientFd, "431 :No nickname given");
        return;
    }
    // Check nickname uniqueness
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second->getNickname() == _arg && it->first != _clientFd) {
            sendToClient(_clientFd, "433 * " + _arg + " :Nickname is already in use");
            return;
        }
    }
    _client->setNickname(_arg);
    _client->markNick();
    std::string message;
    message = MAGENTA "NOTICE * :Nickname " + _arg + " save" RESET;
    sendToClient(_clientFd, message);
}

void Server::user() {
    if (_arg.empty()) {
        sendToClient(_clientFd, "461 USER :Not enough parameters");
        return;
    }
    _client->setUsername(_arg);
    _client->markUser();
    std::string message;
    message = MAGENTA "NOTICE * :User " + _arg + " save" RESET;
    sendToClient(_clientFd, message);
}

void Server::privmsg() {
    if (_arg.empty()) {
        sendToClient(_clientFd, "411 :No recipient given");
        _commandLine.erase(0, _commandLine.size());
        return;
    }
    std::string message;
    size_t pos = _arg.find(":");
    if (pos != std::string::npos) {
        std::string part1(_arg.substr(pos + 1)), part2(_commandLine);
        if (_space == 1)
            part1 += " ";
        message = part1 + part2;
        _arg = _arg.substr(0, pos);
    }
    else {
        std::istringstream iss(_commandLine);
        std::getline(iss, message);
        if (message.empty() || message[0] != ':') {
            sendToClient(_clientFd, "412 :No text to send");
            _commandLine.erase(0, _commandLine.size());
            return;
        }
        message.erase(0, 1);
    }
    Client* target = getClientByNick(_arg);
    if (!target) {
        sendToClient(_clientFd, "401 " + _arg + " :No such nick/channel");
        _commandLine.erase(0, _commandLine.size());
        return;
    }
    if (message[0] == ' ')
        message.erase(0, 1);
    std::string fullMsg = ":" + _client->getPrefix() + " PRIVMSG " + _client->getNickname() + " :" + PINK + message + RESET;
    sendToClient(target->getFd(), fullMsg);
    // sendToClient(_clientFd, ":" + client->getPrefix() + " PRIVMSG " + target->getNickname() + " :" + message);
    _commandLine.erase(0, _commandLine.size());
}

void Server::join() {
    if (_arg.empty() || _arg[0] != '#') {
        sendToClient(_clientFd, "ERROR :Invalid channel name");
        return;
    }
    std::pair<std::map<std::string, Channel>::iterator, bool> result = _channels.insert(std::make_pair(_arg, Channel(_arg)));
    Channel& chan = result.first->second;
    if (!chan.isMember(_client)) {
        chan.join(_client);
        sendToClient(_clientFd, ":" + _client->getNickname() + " JOIN " + _arg);
    }
}

void Server::part() {
    if (_arg.empty() || _arg[0] != '#') {
        sendToClient(_clientFd, "ERROR :Invalid channel name");
        return;
    }
    std::map<std::string, Channel>::iterator it = _channels.find(_arg);
    if (it == _channels.end()) {
        sendToClient(_clientFd, "ERROR :No such channel");
        return;
    }

    Channel& chan = it->second;
    if (!chan.isMember(_client)) {
        sendToClient(_clientFd, "ERROR :You're not in that channel");
        return;
    }

    chan.part(_client); // ta fonction dans Channel
    sendToClient(_clientFd, ":" + _client->getNickname() + " PART " + _arg);

    // (Optionnel) Supprimer le channel s’il est vide
    if (chan.getMemberCount() == 0) {
        _channels.erase(_arg);
        std::cout << RED << "Channel supprimé car vide : " << _arg << RESET << std::endl;
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