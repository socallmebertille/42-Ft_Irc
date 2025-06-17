#include "Server.hpp"
#include "Replies.hpp"
#include <sstream>
#include <cstdlib>

void Server::cap() {
    std::string arg = _client->getArg();
    if (arg.empty())
        return;
    std::istringstream iss(arg);
    std::string subcmd;
    iss >> subcmd;
    if (subcmd == "LS") // capabilities list -> none in IRC
        sendToClient(_clientFd, ":localhost CAP * LS :");
    else if (subcmd == "REQ") { // request from client -> always answer none accepted
        std::string caps;
        std::getline(iss, caps);
        if (!caps.empty() && caps[0] == ':')
            caps = caps.substr(1);
        sendToClient(_clientFd, ":localhost CAP * NAK :" + caps);
    }
    else if (subcmd == "LIST") // capabilities list of those activated
        sendToClient(_clientFd, ":localhost CAP * LIST :");
    else if (subcmd == "END") // end of negotiation from client
        _client->setCapNegotiationDone(true);
}

void Server::ping() {
    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;

    const std::string& args = _client->getArg();
    if (args.empty()) {
        sendReply(ERR_NOORIGIN, _client, "", "", "No origin specified");
        return;
    }
    // Reply with PONG followed by the server and the original message
    std::string response = ":localhost PONG localhost :" + args;
    sendToClient(_clientFd, response);
}

void Server::pong() {
    // The PONG command is usually sent by the client in response to a PING from the server
    // In most cases, it can be silently ignored
    // since it's the server that initiates pings to test the connection

    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;
    return;
}

void Server::pass() {
    if (std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end()) {
        // std::cout << "[DEBUG] Command ignored (client marked for removal): " << _clientFd << std::endl;
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
    }
    else {
        sendReply(ERR_PASSWDMISMATCH, _client, "", "", "Password incorrect");
        _client->setPasswordOk(false);
        _clientsToRemove.push_back(_clientFd);
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
        message = args.substr(colonPos + 2); // +2 to skip " :"
    } else {
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

void Server::join() {
    std::cout << RED << "[DEBUG] JOIN reçu avec argument : [" << _client->getArg() << "]" << RESET << std::endl;

    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;
    std::string args = _client->getArg();
    if (args.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "JOIN", "", "Not enough parameters");
        return;
    }
    std::istringstream iss(args);
    std::string channelName, key;
    iss >> channelName >> key;
    if (!channelName.empty() && channelName[0] == ':')
    channelName = channelName.substr(1);
    if (channelName == ":") { // case clients sending "JOIN :"
            return;
    }
    if (!_client->isPasswordOk()) { // verification if password is ok
        if (!_client->hasSentPassError()) {
            sendReply(ERR_PASSWDMISMATCH, _client, "*", "", "Password required");
            _client->setPassErrorSent(true);
        }
        return;
    }
    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }

    if (channelName.empty() || channelName[0] != '#') {
        sendReply(ERR_NOSUCHCHANNEL, _client, channelName.empty() ? "*" : channelName, "", "No such channel");
        return;
    }
    std::pair<std::map<std::string, Channel>::iterator, bool> result =
        _channels.insert(std::make_pair(channelName, Channel(channelName)));
    Channel& chan = result.first->second;
    if (result.second) {
        chan.setCreator(_client->getNickname());
        chan.addOperator(_client);
        std::string modeMsg = ":" + _client->getPrefix() + " MODE " + channelName + " +o " + _client->getNickname();
        sendToClient(_clientFd, modeMsg);
    }
    else if (chan.isCreator(_client) && !chan.isOperator(_client)) {
        chan.addOperator(_client);
        std::string modeMsg = ":" + _client->getPrefix() + " MODE " + channelName + " +o " + _client->getNickname();
        sendToClient(_clientFd, modeMsg);
    }
    if (!result.second && chan.isInviteOnly() && !chan.isInvited(_client) && !chan.isOperator(_client)) {
        sendReply(ERR_INVITEONLYCHAN, _client, channelName, "", "Cannot join channel (+i)");
        return;
    }
    if (!result.second && chan.hasKey() && !chan.isOperator(_client)) {
        if (key.empty() || key != chan.getKey()) {
            sendReply(ERR_BADCHANNELKEY, _client, channelName, "", "Cannot join channel (+k)");
            return;
        }
    }
    if (!result.second && chan.hasUserLimit() && !chan.isOperator(_client)) {
        if ((int)chan.getMemberCount() >= chan.getUserLimit()) {
            sendReply(ERR_CHANNELISFULL, _client, channelName, "", "Cannot join channel (+l)");
            return;
        }
    }
    if (!chan.isMember(_client)) {
        chan.join(_client);

        // ========== DEBUG LOGS JOIN ==========
        std::cout << RED << "[DEBUG JOIN] Canal: " << channelName << RESET << std::endl;
        std::cout << RED << "[DEBUG JOIN] Nouveau membre: " << _client->getNickname() << RESET << std::endl;
        std::cout << RED << "[DEBUG JOIN] Nombre total de membres: " << chan.getMemberCount() << RESET << std::endl;
        std::cout << RED << "[DEBUG JOIN] Nombre d'opérateurs: " << chan.getOperators().size() << RESET << std::endl;
        std::cout << RED << "[DEBUG JOIN] Est-ce que ce client est opérateur? " << (chan.isOperator(_client) ? "OUI" : "NON") << RESET << std::endl;

        // Afficher la liste des opérateurs
        const std::set<Client*>& ops = chan.getOperators();
        std::cout << RED << "[DEBUG JOIN] Opérateurs actuels: ";
        for (std::set<Client*>::const_iterator it = ops.begin(); it != ops.end(); ++it) {
            std::cout << (*it)->getNickname() << " ";
        }
        std::cout << RESET << std::endl;
        // ====================================

        std::string joinMsg = ":" + _client->getPrefix() + " JOIN :" + channelName;
        const std::set<Client*>& members = chan.getMembers();
        for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
            sendToClient((*it)->getFd(), joinMsg);
        if (!chan.getTopic().empty()) {
            sendReply(RPL_TOPIC, _client, channelName, "", chan.getTopic());
            std::ostringstream timeStr;
            timeStr << chan.getTopicSetTime();
            sendReply(RPL_TOPICWHOTIME, _client, channelName, chan.getTopicSetBy(), timeStr.str());
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
    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }
    std::string args = _client->getArg();
    if (args.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "PART", "", "Not enough parameters");
        return;
    }
    // Parse channel name and optional part message
    std::string channelName, partMessage;
    size_t colonPos = args.find(" :");
    if (colonPos != std::string::npos) {
        channelName = args.substr(0, colonPos);
        partMessage = args.substr(colonPos + 2); // +2 to skip " :"
    } else {
        std::istringstream iss(args);
        iss >> channelName;
        // No part message provided
    }
    // Validate channel name format
    if (channelName.empty() || channelName[0] != '#') {
        sendReply(ERR_NOSUCHCHANNEL, _client, channelName.empty() ? "*" : channelName, "", "No such channel");
        return;
    }
    // Check if channel exists
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
    // Create PART message with optional part message
    std::string partMsg = ":" + _client->getPrefix() + " PART " + channelName;
    if (!partMessage.empty()) {
        partMsg += " :" + partMessage;
    }
    // Send PART message to all channel members
    const std::set<Client*>& members = chan.getMembers();
    for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
        sendToClient((*it)->getFd(), partMsg);
    if (chan.isOperator(_client)) {
        // If the client was an operator, remove them from the operators list
        std::cout << RED << "[DEBUG PART] Client " << _client->getNickname() << " was an operator in channel " << channelName << RESET << std::endl;
        chan.removeOperator(_client); // Remove client from operators if they were one
    }
    // Remove client from channel
    chan.part(_client);
    // Remove empty channel
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
    // std::cout << "[DEBUG] QUIT received → adding to _clientsToRemove: fd[" << _clientFd << "]" << std::endl;
    _clientsToRemove.push_back(_clientFd);
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
        std::string newTopic = args.substr(colonPos + 2); // Skip " :"

        // ========== DEBUG LOGS TEMPORAIRES ==========
        std::cout << RED << "[DEBUG TOPIC] Client: " << _client->getNickname() << RESET << std::endl;
        std::cout << RED << "[DEBUG TOPIC] Canal: " << channelName << RESET << std::endl;
        std::cout << RED << "[DEBUG TOPIC] Topic protégé: " << (chan.isTopicProtected() ? "OUI" : "NON") << RESET << std::endl;
        std::cout << RED << "[DEBUG TOPIC] Client est opérateur: " << (chan.isOperator(_client) ? "OUI" : "NON") << RESET << std::endl;
        std::cout << RED << "[DEBUG TOPIC] Nombre d'opérateurs: " << chan.getOperators().size() << RESET << std::endl;
        // ===============================================

        if (chan.isTopicProtected() && !chan.isOperator(_client)) {
            std::cout << RED << "[DEBUG TOPIC] BLOQUÉ - Envoi erreur 482" << RESET << std::endl;
            sendReply(ERR_CHANOPRIVSNEEDED, _client, channelName, "", "You're not channel operator");
            return;
        }

        std::cout << RED << "[DEBUG TOPIC] AUTORISÉ - Changement de topic" << RESET << std::endl;
        chan.setTopic(newTopic, _client->getNickname());
        std::string topicMsg = ":" + _client->getPrefix() + " TOPIC " + channelName + " :" + newTopic;
        const std::set<Client*>& members = chan.getMembers();
        for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
            sendToClient((*it)->getFd(), topicMsg);
        sendReply(RPL_TOPIC, _client, channelName, "", chan.getTopic());
        std::ostringstream timeStr;
        timeStr << chan.getTopicSetTime();
        sendReply(RPL_TOPICWHOTIME, _client, channelName, chan.getTopicSetBy(), timeStr.str());
    }
    else {
        if (chan.getTopic().empty())
            sendReply(RPL_NOTOPIC, _client, channelName, "", "No topic is set");
        else {
            sendReply(RPL_TOPIC, _client, channelName, "", chan.getTopic());
            std::ostringstream timeStr;
            timeStr << chan.getTopicSetTime();
            sendReply(RPL_TOPICWHOTIME, _client, channelName, chan.getTopicSetBy(), timeStr.str());
        }
    }
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
    if (channelName.empty() || channelName[0] != '#') {
        sendReply(ERR_NOSUCHCHANNEL, _client, channelName, "", "No such channel");
        return;
    }
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
    std::string kickMsg = ":" + _client->getPrefix() + " KICK " + channelName + " " + targetNick + " :" + comment + "\r\n";
    channel.sendToAll(kickMsg);
    channel.part(target);
}

void Server::userhost() {
    // The USERHOST command returns information about one or more users
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

    // Parse the requested nicknames
    std::istringstream iss(args);
    std::string nickname;
    std::string response;

    while (iss >> nickname) {
        Client* target = getClientByNick(nickname);
        if (target && target->isRegistered()) {
            if (!response.empty())
            response += " ";
            // Format: nick=+user@host (+ indicates user is available)
            response += target->getNickname() + "=+" + target->getUsername() + "@localhost";
        }
    }

    // USERHOST response (code 302)
    sendReply(302, _client, "", "", response);
}

void Server::whois() {
    // The WHOIS command returns detailed information about a user
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
    // RPL_WHOISUSER (311): nick user host * :realname
    sendReply(311, _client, nickname, target->getUsername() + " localhost *", target->getRealname());
    // RPL_WHOISSERVER (312): nick server :server info
    sendReply(312, _client, nickname, SERVER_NAME, "IRC Server");
    // RPL_ENDOFWHOIS (318): nick :End of /WHOIS list
    sendReply(318, _client, nickname, "", "End of /WHOIS list");
    // std::cout << "[DEBUG] WHOIS for " << nickname << " completed" << std::endl;
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
    std::string target;
    iss >> target;
    std::string modeStr;
    std::vector<std::string> params;
    // Get modes & parameters
    if (!(iss >> modeStr)) {
        // If no mode : show current modes
        Channel* chan = NULL;
        if (!validateModeCommand(target, chan))
            return;
        showCurrentModes(target, *chan);
        return;
    }
    std::string param;
    while (iss >> param) {
        params.push_back(param);
    }
    // Check if it's a user mode (target = client's nickname)
    if (target == _client->getNickname()) {
        return; // User mode - ignore
    }
    // Validate channel and get references
    Channel* chan = NULL;
    if (!validateModeCommand(target, chan)) {
        return; //error already sent
    }
    if (!chan->isOperator(_client)) {
        sendReply(ERR_CHANOPRIVSNEEDED, _client, target, "", "You're not channel operator");
        return;
    }
    // Process modes with improved parsing
    bool adding = true; // Start with + by default
    size_t paramIndex = 0;
    std::string appliedModes = "";
    std::string appliedParams = "";
    for (size_t i = 0; i < modeStr.length(); ++i) {
        char flag = modeStr[i];
        if (flag == '+') {
            adding = true;
            continue;
        }
        if (flag == '-') {
            adding = false;
            continue;
        }
        // ========== DEBUG LOGS MODE ==========
        std::cout << RED << "[DEBUG MODE] Processing flag: " << flag << " adding: " << (adding ? "YES" : "NO") << RESET << std::endl;
        // ====================================
    
        // Process the mode flag : if mode need param, he should be in params[paramIndex]
        if (!processSingleMode(flag, adding, params, paramIndex, *chan, target, appliedModes, appliedParams)) {
            // Continue processing other modes even if one fails (IRC standard behavior)
            continue;
        }

        // ========== DEBUG LOGS MODE AFTER ==========
        if (flag == 't') {
            std::cout << RED << "[DEBUG MODE] After processing +t: isTopicProtected = " << (chan->isTopicProtected() ? "OUI" : "NON") << RESET << std::endl;
        }
        // ==========================================
    }

    // Send notification to all channel members if any modes were applied
    if (!appliedModes.empty()) {
        std::string modeMsg = ":" + _client->getPrefix() + " MODE " + target + " " + appliedModes;
        if (!appliedParams.empty()) {
            modeMsg += " " + appliedParams;
        }
        const std::set<Client*>& members = chan->getMembers();
        for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
            sendToClient((*it)->getFd(), modeMsg);
        }
    }
}

void Server::list() {
    return;
}

void Server::notice() {
    return;
}
