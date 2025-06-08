#include "Server.hpp"
#include "Replies.hpp"


void Server::cap() {
    std::string arg = _client->getArg();
    if (arg.empty()) {
        return;
    }

    if (arg == "LS") {
        sendToClient(_clientFd, "CAP * LS :\r\n");
        sendToClient(_clientFd, "CAP * END\r\n");
    }
    else if (arg == "REQ") {
        sendToClient(_clientFd, "CAP * NAK :\r\n");
        sendToClient(_clientFd, "CAP * END\r\n");
    }
    else if (arg == "LIST") {
        sendToClient(_clientFd, "CAP * LIST :\r\n");
        sendToClient(_clientFd, "CAP * END\r\n");
    }
    else if (arg == "END") {
        sendToClient(_clientFd, "CAP * END\r\n");
    }
}

void Server::ping() {
    if (_client->getArg().empty()) {
        sendToClient(_clientFd, "409 :No origin specified\r\n");
        return;
    }
    std::string response = "PONG :" + _client->getArg() + "\r\n";
    sendToClient(_clientFd, response);
}

void Server::pong() {
    // No need to handle PONG responses from client
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
        std::cout << "[DEBUG] mot de passe incorrect : [" << inputPass << "] vs [" << _password << "]\n";
        sendToClient(_clientFd, "464 :Password incorrect\r\n");
        _client->setPasswordOk(false);
    }
}


void Server::nick() {
    if (_client->getArg().empty()) {
        sendToClient(_clientFd, "431 " + _client->getNickname() + " :No nickname given\r\n");
        return;
    }

    // Nettoyer le nouveau nickname
    std::string newNick = _client->getArg();
    size_t firstSpace = newNick.find_first_of(" \t\r\n");
    if (firstSpace != std::string::npos) {
        newNick = newNick.substr(0, firstSpace);
    }

    // Vérifier si le nickname est déjà utilisé
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second->getNickname() == newNick && it->first != _clientFd) {
            sendToClient(_clientFd, "433 * " + newNick + " :Nickname is already in use\r\n");
            return;
        }
    }

    std::string oldNick = _client->getNickname();
    if (!_client->setNickname(newNick)) {
        sendToClient(_clientFd, "432 * " + newNick + " :Erroneous nickname\r\n");
        return;
    }
    _client->markNick();

    // Envoyer le message de confirmation du changement de pseudo
    std::string prefix = oldNick.empty() ? "*" : oldNick;
    std::string nickMsg = ":" + prefix + "!" + (_client->getUsername().empty() ? "*" : _client->getUsername()) + "@localhost NICK :" + newNick + "\r\n";
    sendToClient(_clientFd, nickMsg);

    // Si l'utilisateur a déjà fourni son nom d'utilisateur et le mot de passe est OK, on peut le marquer comme enregistré
    if (_client->hasUser() && _client->isPasswordOk() && !_client->isRegistered()) {
        _client->registerUser(newNick, _client->getUsername(), _client->getRealname());
        sendReply(RPL_WELCOME, _client, "", "", "Welcome to the Internet Relay Network " + _client->getPrefix());
    }
}

void Server::user() {
    if (_client->isRegistered()) {
        sendToClient(_clientFd, "462 :You may not reregister");
        return;
    }

    std::string args = _client->getArg();
    std::string& buffer = _client->getBuffer();

    // Trouver la position du realname (commence par :)
    size_t realNamePos = args.find(" :");
    if (realNamePos == std::string::npos) {
        realNamePos = buffer.find(":");
        if (realNamePos != std::string::npos) {
            args += buffer.substr(realNamePos);
        }
    }

    std::istringstream iss(args);
    std::string username, hostname, servername, realname;

    // Extraire username, hostname, servername
    if (!(iss >> username >> hostname >> servername)) {
        sendToClient(_clientFd, "461 USER :Not enough parameters");
        return;
    }

    // Extraire realname (peut contenir des espaces)
    if (realNamePos != std::string::npos) {
        realname = args.substr(realNamePos + 2); // +2 pour sauter " :"
    } else {
        sendToClient(_clientFd, "461 USER :Not enough parameters");
        return;
    }

    _client->setUsername(username);
    _client->setRealname(realname);
    _client->markUser();

    // Si le client a déjà un nickname et le mot de passe est OK, on peut le marquer comme enregistré
    if (_client->hasNick() && _client->isPasswordOk()) {
        _client->registerUser(_client->getNickname(), username, realname);
        sendReply(RPL_WELCOME, _client, "", "", "Welcome to the Internet Relay Network " + _client->getPrefix());
    }
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
        std::string& buffer = _client->getBuffer();
        std::string part1(argCpy.substr(pos + 1)), part2(buffer);
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
	if (_client->getArg().empty()) {
		sendToClient(_clientFd, "461 MODE :Not enough parameters\r\n");
		return;
	}
	// Pour l'instant on ignore les modes, mais on évite l'erreur
	sendToClient(_clientFd, "324 " + _client->getNickname() + " +\r\n");
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
