#include "Server.hpp"
#include "Replies.hpp"
#include <sstream>
#include <cstdlib>

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
    // std::cout << "[DEBUG] PRIVMSG channel '" << channelName << "' sent to " << sentCount << " members" << std::endl;
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

    // std::cout << "[DEBUG] Private PRIVMSG sent to '" << targetNick << "' (fd=" << target->getFd() << "): " << fullMsg << std::endl;
}

bool Server::validateModeCommand(const std::string& target, Channel*& chan) {
	// Channel mode - target must start with #
	if (!isValidChannelName(target)) {
		sendReply(ERR_NOSUCHCHANNEL, _client, target, "", "No such channel");
		return false;
	}

	std::map<std::string, Channel>::iterator it = _channels.find(target);
	if (it == _channels.end()) {
		sendReply(ERR_NOSUCHCHANNEL, _client, target, "", "No such channel");
		return false;
	}

	chan = &(it->second);
	return true;
}

void Server::showCurrentModes(const std::string& channelName, const Channel& chan) {
	std::string currentModes = "+";
	if (chan.isInviteOnly()) currentModes += "i";
	if (chan.isTopicProtected()) currentModes += "t";
	if (chan.hasKey()) currentModes += "k";
	if (chan.hasUserLimit()) currentModes += "l";
	sendToClient(_clientFd, std::string(":") + SERVER_NAME + " 324 " + _client->getNickname() + " " + channelName + " " + currentModes + "\r\n");
}

bool Server::processSingleMode(char flag, bool adding, const std::vector<std::string>& params,
							  size_t& paramIndex, Channel& chan, const std::string& channelName,
							  std::string& appliedModes, std::string& appliedParams) {
	switch (flag) {
		case 'i':
			chan.setInviteOnly(adding);
			appliedModes += (adding ? "+" : "-");
			appliedModes += flag;
			break;
		case 't':
			chan.setTopicProtected(adding);
			appliedModes += (adding ? "+" : "-");
			appliedModes += flag;
			break;
		case 'k':
			if (adding) {
				if (paramIndex >= params.size()) {
					sendReply(ERR_NEEDMOREPARAMS, _client, "MODE", "", "Need key parameter for +k");
					return false;
				}
				chan.setKey(params[paramIndex]);
				appliedModes += "+k";
				if (!appliedParams.empty()) appliedParams += " ";
				appliedParams += params[paramIndex];
				paramIndex++;
			} else {
				chan.removeKey();
				appliedModes += "-k";
			}
			break;
		case 'l':
			if (adding) {
				if (paramIndex >= params.size()) {
					sendReply(ERR_NEEDMOREPARAMS, _client, "MODE", "", "Need limit parameter for +l");
					return false;
				}
				int limit = std::atoi(params[paramIndex].c_str());
				if (limit <= 0) {
					sendReply(ERR_NEEDMOREPARAMS, _client, "MODE", "", "Invalid limit parameter for +l");
					return false;
				}
				chan.setUserLimit(limit);
				appliedModes += "+l";
				if (!appliedParams.empty()) appliedParams += " ";
				appliedParams += params[paramIndex];
				paramIndex++;
			} else {
				chan.removeUserLimit();
				appliedModes += "-l";
			}
			break;
		case 'o':
			return handleOperatorMode(adding, params, paramIndex, chan, channelName, appliedModes, appliedParams);
		default:
			sendReply(ERR_UNKNOWNMODE, _client, std::string(1, flag), "", "is unknown mode char to me");
			return false;
	}
	return true;
}

bool Server::handleOperatorMode(bool adding, const std::vector<std::string>& params, size_t& paramIndex,
							   Channel& chan, const std::string& channelName,
							   std::string& appliedModes, std::string& appliedParams) {
	if (paramIndex >= params.size()) {
		sendReply(ERR_NEEDMOREPARAMS, _client, "MODE", "", "Need nickname parameter for +o/-o");
		return false;
	}
	std::string targetNick = params[paramIndex];
	// Find the target client
	Client* targetClient = getClientByNick(targetNick);
	if (!targetClient) {
		sendReply(ERR_NOSUCHNICK, _client, targetNick, "", "No such nick/channel");
		return false;
	}
	// Check if the target is in the channel
	if (!chan.isMember(targetClient)) {
		sendReply(ERR_USERNOTINCHANNEL, _client, targetNick, channelName, "They aren't on that channel");
		return false;
	}
	if (adding) {
		// Add operator privileges (+o)
		if (!chan.isOperator(targetClient)) {
			chan.addOperator(targetClient);
			appliedModes += "+o";
			if (!appliedParams.empty()) appliedParams += " ";
			appliedParams += targetNick;
		}
	} else {
		// Remove operator privileges (-o)
		if (chan.isOperator(targetClient)) {
			chan.removeOperator(targetClient);
			appliedModes += "-o";
			if (!appliedParams.empty()) appliedParams += " ";
			appliedParams += targetNick;
		}
	}
	paramIndex++;
	return true;
}
