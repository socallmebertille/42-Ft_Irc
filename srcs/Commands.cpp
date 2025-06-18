#include "Server.hpp"
#include "Replies.hpp"
#include <sstream>
#include <cstdlib>

void Server::cap() {
	// CAP is used for capability negotiation in IRC
	// it allows clients to request or list capabilities
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
	// The PING command is used to check if the server is reachable

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
	// The PASS command is used to set the password for the client

    if (std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end()) {
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
    }
    else {
        sendReply(ERR_PASSWDMISMATCH, _client, "", "", "Password incorrect");
        _client->setPasswordOk(false);
    }
}

void Server::nick() {
	// The NICK command is used to set the nickname for the client

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
	// The USER command is used to set the username and real name for the client

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
}

void Server::privmsg() {
	// The PRIVMSG command is used to send a private message to a user or a channel

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
    if (message.find("\001DCC") == 0) { // get file transfer from irssi
        std::string dccMsg = message;
        if (dccMsg[0] == '\001')
            dccMsg = dccMsg.substr(1);
        if (dccMsg[dccMsg.length() - 1] == '\001')
            dccMsg = dccMsg.substr(0, dccMsg.length() - 1);
        std::string fullDccMsg = ":" + _client->getPrefix() + " PRIVMSG " + target + " :\001" + dccMsg + "\001\r\n";
        Client* targetClient = getClientByNick(target);
        if (targetClient)
            sendToClient(targetClient->getFd(), fullDccMsg);
        return;
    }
    if (target[0] == '#') {
        handleChannelMessage(target, message);
    }
    else {
        handlePrivateMessage(target, message);
    }
}

void Server::join() {
	// The JOIN command is used to join a channel

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
    if (channelName == ":") {
            return;
    }
    if (!_client->isPasswordOk()) {
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
    if (isUserBannedFromChannel(_client->getNickname(), channelName)) {
        std::string banMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                            " PRIVMSG " + _client->getNickname() +
                            " :You are BANNED from " + channelName +
                            " for inappropriate language. Access denied 🚫 .";
        sendToClient(_client->getFd(), banMsg);
        sendReply(ERR_BANNEDFROMCHAN, _client, channelName, "", "Cannot join channel (+b) - you are banned 🚫 ");
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
	// The PART command is used to leave a channel

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
    std::string channelName, partMessage;
    size_t colonPos = args.find(" :");
    if (colonPos != std::string::npos) {
        channelName = args.substr(0, colonPos);
        partMessage = args.substr(colonPos + 2); // +2 to skip " :"
    } else {
        std::istringstream iss(args);
        iss >> channelName;
    }
    if (channelName.empty() || channelName[0] != '#') {
        sendReply(ERR_NOSUCHCHANNEL, _client, channelName.empty() ? "*" : channelName, "", "No such channel");
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
    if (!partMessage.empty()) {
        partMsg += " :" + partMessage;
    }
    const std::set<Client*>& members = chan.getMembers();
    for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
        sendToClient((*it)->getFd(), partMsg);
    if (chan.isOperator(_client)) {
        chan.removeOperator(_client);
    }
    chan.part(_client);
    if (chan.getMemberCount() == 0) {
        _channels.erase(channelName);
    }
}

void Server::quit() {
	// The QUIT command is used to disconnect the client from the server

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
    _clientsToRemove.push_back(_clientFd);
}

void Server::mode() {
    // The MODE command is used to set or retrieve channel modes
    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;

    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }

    std::string args = _client->getArg();
    if (args.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "MODE", "", "Not enough parameters");
        return;
    }

    std::vector<std::string> params = ft_split(args, ' ');
    if (params.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "MODE", "", "Not enough parameters");
        return;
    }

    std::string target = params[0];
    Channel* chan = NULL;
    if (!validateModeCommand(target, chan)) {
        return;
    }

    if (params.size() == 1) {
        showCurrentModes(target, *chan);
        return;
    }

    if (!chan->isOperator(_client)) {
        sendReply(ERR_CHANOPRIVSNEEDED, _client, target, "", "You're not channel operator");
        return;
    }

    std::string modeString = params[1];
    std::vector<std::string> modeParams(params.begin() + 2, params.end());

    std::string appliedModes;
    std::string appliedParams;
    size_t paramIndex = 0;
    bool adding = true;

    for (size_t i = 0; i < modeString.length(); i++) {
        char c = modeString[i];
        if (c == '+') {
            adding = true;
        } else if (c == '-') {
            adding = false;
        } else {
            if (!processSingleMode(c, adding, modeParams, paramIndex, *chan, target, appliedModes, appliedParams)) {
                continue;
            }
        }
    }

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

void Server::topic() {
	// The TOPIC command is used to set or retrieve the topic of a channel

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
        if (chan.isTopicProtected() && !chan.isOperator(_client)) {
            sendReply(ERR_CHANOPRIVSNEEDED, _client, channelName, "", "You're not channel operator");
            return;
        }
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
	// The INVITE command is used to invite a user to a channel

    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;

    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }
    std::string args = _client->getArg();
    if (args.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "INVITE", "", "Not enough parameters");
        return;
    }
    std::istringstream iss(args);
    std::string targetNick, channelName;

    if (!(iss >> targetNick >> channelName)) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "INVITE", "", "Not enough parameters");
        return;
    }
    Client* target = getClientByNick(targetNick);
    if (!target) {
        sendReply(ERR_NOSUCHNICK, _client, targetNick, "", "No such nick/channel");
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
    if (chan.isInviteOnly() && !chan.isOperator(_client)) {
        sendReply(ERR_CHANOPRIVSNEEDED, _client, channelName, "", "You're not channel operator");
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
	// The KICK command is used to remove a user from a channel

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
	// The USERHOST command returns information about a user
	// Format: USERHOST <nick1>  ...
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
    std::istringstream iss(args);
    std::string nickname;
    std::string response;

    while (iss >> nickname) {
        Client* target = getClientByNick(nickname);
        if (target && target->isRegistered()) {
            if (!response.empty())
            response += " ";
            response += target->getNickname() + "=+" + target->getUsername() + "@localhost";
        }
    }
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
    sendReply(311, _client, nickname, target->getUsername() + " localhost *", target->getRealname());
    sendReply(312, _client, nickname, SERVER_NAME, "IRC Server");
    sendReply(318, _client, nickname, "", "End of /WHOIS list");
}

void Server::bot() {
    // Commande BOT - Contrôle complet du bot IRC
    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;

    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }

    std::string args = _client->getArg();
    if (args.empty()) {
        std::string helpMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                             " PRIVMSG " + _client->getNickname() +
                             " :🤖 BOT COMMAND USAGE:\n"
                             "• BOT enable/disable - Control bot activation\n"
                             "• BOT status - Check bot status\n"
                             "• BOT stats - Server statistics\n"
                             "• BOT uptime - Server uptime\n"
                             "• BOT users - Connected users\n"
                             "• BOT channels - Active channels\n"
                             "• BOT chat #channel - Enable chat mode\n"
                             "• BOT chat exit - Disable chat mode\n"
                             "• BOT help - Show this help";
        sendToClient(_clientFd, helpMsg);
        return;
    }
    std::string botCommand = args;
    if (args.find("chat ") == 0) {
        std::string chatArgs = args.substr(5);
        if (chatArgs == "exit") {
            if (isInChatMode(_clientFd)) {
                std::string currentChannel = getCurrentChannel(_clientFd);
                exitChatMode(_clientFd);
                std::string exitMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                     " PRIVMSG " + _client->getNickname() +
                                     " :Chat mode DISABLED for " + currentChannel + ". Back to normal IRC mode.";
                sendToClient(_clientFd, exitMsg);
            } else {
                std::string errorMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                      " PRIVMSG " + _client->getNickname() +
                                      " :You are not in chat mode.";
                sendToClient(_clientFd, errorMsg);
            }
            return;
        }
        else if (chatArgs == "status") {
            if (isInChatMode(_clientFd)) {
                std::string currentChannel = getCurrentChannel(_clientFd);
                std::string statusMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                       " PRIVMSG " + _client->getNickname() +
                                       " :Chat mode ACTIVE for " + currentChannel + ". Type directly to send messages!";
                sendToClient(_clientFd, statusMsg);
            } else {
                std::string statusMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                       " PRIVMSG " + _client->getNickname() +
                                       " :Chat mode DISABLED. Use BOT chat #channel to enable.";
                sendToClient(_clientFd, statusMsg);
            }
            return;
        }
        else if (chatArgs[0] == '#') {
            std::string channelName = chatArgs;
            std::map<std::string, Channel>::iterator it = _channels.find(channelName);
            if (it == _channels.end()) {
                std::string errorMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                      " PRIVMSG " + _client->getNickname() +
                                      " :Channel " + channelName + " does not exist. Join it first with: JOIN " + channelName;
                sendToClient(_clientFd, errorMsg);
                return;
            }
            Channel& chan = it->second;
            if (!chan.isMember(_client)) {
                std::string errorMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                      " PRIVMSG " + _client->getNickname() +
                                      " :You are not a member of " + channelName + ". Join it first with: JOIN " + channelName;
                sendToClient(_clientFd, errorMsg);
                return;
            }
            setChatMode(_clientFd, channelName);
            std::string successMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                   " PRIVMSG " + _client->getNickname() +
                                   " :Chat mode ENABLED for " + channelName + "! \n"
                                   "Now type messages directly (no need for PRIVMSG)\n"
                                   "Use BOT chat exit to return to normal IRC mode";
            sendToClient(_clientFd, successMsg);
            return;
        }
    }
    std::string botResponse = processIRCBotCommand(args, "");
    if (!botResponse.empty()) {
        if (_client->isNetcatLike()) {
            std::string botMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                               " PRIVMSG " + _client->getNickname() + " :" + botResponse;
            sendToClient(_clientFd, botMsg);
        } else {
            if (_ircBotCreated && _ircBotClient) {
                std::string botMsg = ":" + _ircBotClient->getPrefix() +
                                   " PRIVMSG " + _client->getNickname() + " :" + botResponse;
                sendToClient(_clientFd, botMsg);
            } else {
                std::string botMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                   " PRIVMSG " + _client->getNickname() + " :" + botResponse;
                sendToClient(_clientFd, botMsg);
            }
        }
    } else {
        std::string errorMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                              " PRIVMSG " + _client->getNickname() +
                              " :❌ Unknown BOT command. Use BOT help for available commands.";
        sendToClient(_clientFd, errorMsg);
    }
}

void Server::sendfile() {
    std::istringstream iss(_client->getArg());
    std::string targetNick, filename;
    iss >> targetNick >> filename;
    if (targetNick.empty() || filename.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "SEND", "", "Usage: SEND <nick> <filename>");
        return;
    }
    Client* target = getClientByNick(targetNick);
    if (!target) {
        sendReply(ERR_NOSUCHNICK, _client, targetNick, "", "No such nick");
        return;
    }
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        sendReply(ERR_FILEERROR, _client, filename, "", "Cannot open file");
        return;
    }
    std::stringstream buffer;
    buffer << file.rdbuf(); // read file
    std::string content = buffer.str();
    file.close();
    std::string transferKey = targetNick + "_" + filename;
    PendingTransfer transfer; // keep track of the transfer
    transfer.sender = _client->getNickname();
    transfer.content = content;
    transfer.filename = filename;
    _pendingTransfers[transferKey] = transfer;
    // notif to target client
    std::string msg = ":" + _client->getPrefix() + " SEND " + filename + " :File transfer request from " + _client->getNickname() + "\r\n";
    sendToClient(target->getFd(), msg);
}

void Server::acceptFile() {
    std::istringstream iss(_client->getArg());
    std::string targetNick, filename;
    iss >> targetNick >> filename;
    if (targetNick.empty() || filename.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "ACCEPT", "", "Usage: ACCEPT <nick> <filename>");
        return;
    }
    // check if the transfer exists
    std::string transferKey = _client->getNickname() + "_" + filename;
    std::map<std::string, PendingTransfer>::iterator it = _pendingTransfers.find(transferKey);
    if (it == _pendingTransfers.end()) {
        sendReply(ERR_FILEERROR, _client, filename, "", "No pending transfer for this file");
        return;
    }
    Client* sender = getClientByNick(targetNick);
    if (!sender) {
        sendReply(ERR_NOSUCHNICK, _client, targetNick, "", "No such nick");
        _pendingTransfers.erase(transferKey);
        return;
    }
    // Notif sender
    std::string acceptMsg = ":" + _client->getPrefix() + " ACCEPT " + filename + "\r\n";
    sendToClient(sender->getFd(), acceptMsg);
    // get file
    std::string contentMsg = ":" + _client->getPrefix() + " NOTICE " + _client->getNickname() 
                         + " :File content of " + filename + ":\r\n" + it->second.content + "\r\n";
    sendToClient(_clientFd, contentMsg);
    // delete the tracker
    _pendingTransfers.erase(transferKey);
}

void Server::refuseFile() {
    std::istringstream iss(_client->getArg());
    std::string targetNick, filename;
    iss >> targetNick >> filename;
    
    if (targetNick.empty() || filename.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "REFUSE", "", "Usage: REFUSE <nick> <filename>");
        return;
    }
    // check if the transfer exists
    std::string transferKey = _client->getNickname() + "_" + filename;
    std::map<std::string, PendingTransfer>::iterator it = _pendingTransfers.find(transferKey);
    if (it == _pendingTransfers.end()) {
        sendReply(ERR_FILEERROR, _client, filename, "", "No pending transfer for this file");
        return;
    }
    Client* target = getClientByNick(targetNick);
    if (!target) {
        sendReply(ERR_NOSUCHNICK, _client, targetNick, "", "No such nick");
        return;
    }
    // Notif sender
    std::string msg = ":" + _client->getPrefix() + " REFUSE " + filename + "\r\n";
    sendToClient(target->getFd(), msg);
    // delete the tracker
    _pendingTransfers.erase(transferKey);
}

