#include "Server.hpp"
#include "Replies.hpp"

void Server::cap() {
    std::string arg = _client->getArg();
    if (arg.empty())
        return;
    std::istringstream iss(arg);
    std::string subcmd;
    iss >> subcmd;
    if (subcmd == "LS") // capacities list -> none in IRC
        sendToClient(_clientFd, ":localhost CAP * LS :");
    else if (subcmd == "REQ") { // request form client -> always answer none accepted
        std::string caps;
        std::getline(iss, caps);
        if (!caps.empty() && caps[0] == ':')
            caps = caps.substr(1);
        sendToClient(_clientFd, ":localhost CAP * NAK :" + caps);
    }
    else if (subcmd == "LIST") // capacities list of those activated
        sendToClient(_clientFd, ":localhost CAP * LIST :");
    else if (subcmd == "END") // end of negociation client
        _client->setCapNegotiationDone(true);
}


void Server::ping() {
    // if (_client->getArg().empty()) {
    // 	sendToClient(_clientFd, "409 " + _client->getNickname() + " :No origin specified");
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
    if (newNick.empty()) {
        sendToClient(_clientFd, "431 * :No nickname given");
        return;
    }
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
    if (_client->isRegistered() || _client->hasUser()) {
        sendToClient(_clientFd, "462 :You may not reregister");
        return;
    }
    std::string args = _client->getArg();
    if (args.empty()) {
        sendToClient(_clientFd, "461 USER :Not enough parameters");
        return;
    }
    std::istringstream iss(args);
    std::string username, hostname, servername, realname;
    if (!(iss >> username >> hostname >> servername)) { // Parser: USER <username> <hostname> <servername> :<realname>
        sendToClient(_clientFd, "461 USER :Not enough parameters");
        return;
    }
    std::getline(iss, realname);
    if (!realname.empty() && realname[0] == ' ') {
        realname = realname.substr(1);
    }
    if (!realname.empty() && realname[0] == ':') {
        realname = realname.substr(1);
    }
    if (realname.empty()) {
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
    if (message[0] == ' ')
        message.erase(0, 1);
    std::istringstream iss(_client->getArg());
    std::string target;
    iss >> target;
    if (!target.empty() && target[0] == '#')
    {
        std::map<std::string, Channel>::iterator it = _channels.find(target);
        if (it == _channels.end()) {
            sendToClient(_clientFd, "403 " + _client->getNickname() + " " + target + " :No such channel");
            return;
        }
        Channel& chan = it->second;
        if (!chan.isMember(_client)) {
            sendToClient(_clientFd, "404 " + _client->getNickname() + " " + target + " :Cannot send to channel");
            return;
        }
        std::string fullMsg = ":" + _client->getPrefix() + " PRIVMSG " + target + " :" + message;
        const std::set<Client*>& members = chan.getMembers();
        for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
            if ((*it)->getFd() != _clientFd)
                sendToClient((*it)->getFd(), fullMsg); 
        }
    }
    else
    {
        Client* targetNick = getClientByNick(target);
        if (!targetNick) {
            sendToClient(_clientFd, "401 " + _client->getArg() + " :No such nick/channel");
            _client->eraseBuf();
            return;
        }
        std::string fullMsg = ":" + _client->getPrefix() + " PRIVMSG " + targetNick->getNickname() + " :" + message;
        sendToClient(targetNick->getFd(), fullMsg);
    }
    _client->eraseBuf();
}

void Server::join() {
    if (_client->getArg().empty() || _client->getArg()[0] != '#') {
        sendToClient(_clientFd, "403 " + _client->getNickname() + " " + _client->getArg() + " :No such channel");
        return;
    }
    std::pair<std::map<std::string, Channel>::iterator, bool> result = _channels.insert(std::make_pair(_client->getArg(), Channel(_client->getArg())));
    Channel& chan = result.first->second;
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
        sendToClient(_clientFd, "403 " + _client->getNickname() + " " + _client->getArg() + " :No such channel");
        return;
    }
    std::map<std::string, Channel>::iterator it = _channels.find(_client->getArg());
    if (it == _channels.end()) {
        sendToClient(_clientFd, "403 " + _client->getNickname() + " " + _client->getArg() + " :No such channel");
        return;
    }
    Channel& chan = it->second;
    if (!chan.isMember(_client)) {
        sendToClient(_clientFd, "442 " + _client->getNickname() + " " + _client->getArg() + " :You're not on that channel");
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
    return;
}

void Server::mode() {
    if (_client->getArg().empty()) {
        sendToClient(_clientFd, "461 " + _client->getNickname() + " MODE :Not enough parameters");
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
    return;
}

void Server::kick() {
    return;
}

void Server::notice() {
    return;
}
