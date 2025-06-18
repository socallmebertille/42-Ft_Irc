#include "Server.hpp"
#include "Replies.hpp"
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <fstream>

void Server::loadBotStats() {
    std::ifstream file("bot_stats.txt");
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            if (key == "TOTAL_INTERACTIONS") {
                _totalBotInteractions = std::atoi(value.c_str());
            } else if (key == "TOTAL_JOKES") {
                _totalJokesShared = std::atoi(value.c_str());
            }
        }
    }
    file.close();
}

void Server::saveBotStats() {
    std::ofstream file("bot_stats.txt");
    if (!file.is_open()) return;

    file << "# Bot Statistics File\n";
    file << "# Format: KEY=VALUE\n";
    file << "TOTAL_MESSAGES=" << (_userMessageCount.size() > 0 ? _userMessageCount.size() : 0) << "\n";
    file << "TOTAL_JOKES=" << _totalJokesShared << "\n";
    file << "TOTAL_INTERACTIONS=" << _totalBotInteractions << "\n";
    file << "UPTIME_START=" << _serverStartTime << "\n";

    std::string mostActiveUser = "";
    int maxUserMessages = 0;
    for (std::map<std::string, int>::const_iterator it = _userMessageCount.begin(); it != _userMessageCount.end(); ++it) {
        if (it->second > maxUserMessages) {
            maxUserMessages = it->second;
            mostActiveUser = it->first;
        }
    }
    file << "MOST_ACTIVE_USER=" << mostActiveUser << "\n";

    std::string mostActiveChannel = "";
    int maxChannelMessages = 0;
    for (std::map<std::string, int>::const_iterator it = _channelMessageCount.begin(); it != _channelMessageCount.end(); ++it) {
        if (it->second > maxChannelMessages) {
            maxChannelMessages = it->second;
            mostActiveChannel = it->first;
        }
    }
    file << "MOST_ACTIVE_CHANNEL=" << mostActiveChannel << "\n";
    file << "LAST_RESTART=" << time(0) << "\n";

    file.close();
}

void Server::updateBotStats(const std::string& action, const std::string& user, const std::string& channel) {
    if (action == "interaction") {
        _totalBotInteractions++;
    } else if (action == "joke") {
        _totalJokesShared++;
    } else if (action == "message" && !user.empty()) {
        _userMessageCount[user]++;
        if (!channel.empty()) {
            _channelMessageCount[channel]++;
        }
    }
}

std::string Server::getUptimeString() {
    time_t currentTime = time(0);
    time_t uptime = currentTime - _serverStartTime;

    int days = uptime / 86400;
    int hours = (uptime % 86400) / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = uptime % 60;

    std::ostringstream oss;
    if (days > 0) oss << days << "d ";
    if (hours > 0) oss << hours << "h ";
    if (minutes > 0) oss << minutes << "m ";
    oss << seconds << "s";

    return oss.str();
}

// ================================================

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
    if (message.find("!") == 0) {
        std::string botResponse = processIRCBotCommand(message, channelName);
        if (!botResponse.empty()) {
            std::string botMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) + " PRIVMSG " + channelName + " :" + botResponse;
            const std::set<Client*>& members = channel.getMembers();
            for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
                sendToClient((*it)->getFd(), botMsg);
            }
            return;
        }
    }
    if (_botEnabled) {
        if (message.find("hello") != std::string::npos ||
            message.find("hi") != std::string::npos ||
            message.find("time") != std::string::npos ||
            message.find("help") != std::string::npos ||
            message.find("joke") != std::string::npos ||
            message.find("bye") != std::string::npos ||
            message.find("goodbye") != std::string::npos ||
            (message.find("bot") != std::string::npos && message.find("online") != std::string::npos)) {

            std::string botResponse = "";
            if (message.find("hello") != std::string::npos || message.find("hi") != std::string::npos) {
                botResponse = "👋 Hello! How can I assist you?";
            } else if (message.find("time") != std::string::npos) {
                time_t now = time(0);
                char* dt = ctime(&now);
                std::string timeStr = dt;
                if (!timeStr.empty() && timeStr[timeStr.length()-1] == '\n') {
                    timeStr.erase(timeStr.length()-1);
                }
                botResponse = "⏰ Current time: " + timeStr;
            } else if (message.find("joke") != std::string::npos) {
                updateBotStats("joke", _client->getNickname(), channelName);
                const char* jokes[] = {
                    "😄 Why do programmers prefer dark mode? Because light attracts bugs! 🐛",
                    "😄 Why did the IRC user go broke? Too many /quit commands! 💸",
                    "😄 What's a computer's favorite snack? Microchips! 🍟"
                };
                botResponse = jokes[rand() % 3];
            } else if (message.find("bye") != std::string::npos || message.find("goodbye") != std::string::npos) {
                botResponse = "👋 Goodbye! Have a great day!";
            } else if (message.find("help") != std::string::npos) {
                botResponse = "📋 I understand: hello, time, help, joke, bye, goodbye";
            } else if (message.find("bot") != std::string::npos && message.find("online") != std::string::npos) {
                botResponse = "🤖 Yes, I'm online and ready to help!";
            }

            if (!botResponse.empty()) {
                std::string botMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) + " PRIVMSG " + channelName + " :" + botResponse;
                const std::set<Client*>& members = channel.getMembers();
                for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
                    sendToClient((*it)->getFd(), botMsg);
                }
            }
        }
    }
	updateBotStats("message", _client->getNickname(), channelName);

    moderateMessage(message, channelName);
    if (!channel.isMember(_client)) {
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
}

void Server::handlePrivateMessage(const std::string& targetNick, const std::string& message) {
    if (targetNick == "IRCBot" && message.find("!") == 0) {
        std::string botResponse = processIRCBotCommand(message, "");
        if (!botResponse.empty()) {
            std::string botMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) + " PRIVMSG " + _client->getNickname() + " :" + botResponse;
            sendToClient(_client->getFd(), botMsg);
            return;
        }
    }
    Client* target = getClientByNick(targetNick);
    if (!target) {
        sendReply(ERR_NOSUCHNICK, _client, targetNick, "", "No such nick/channel");
        return;
    }
    if (!target->isRegistered()) {
        return;
    }

    std::string fullMsg = ":" + _client->getPrefix() + " PRIVMSG " + targetNick + " :" + message;
    sendToClient(target->getFd(), fullMsg);
}

bool Server::validateModeCommand(const std::string& target, Channel*& chan) {
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
	Client* targetClient = getClientByNick(targetNick);
	if (!targetClient) {
		sendReply(ERR_NOSUCHNICK, _client, targetNick, "", "No such nick/channel");
		return false;
	}
	if (!chan.isMember(targetClient)) {
		sendReply(ERR_USERNOTINCHANNEL, _client, targetNick, channelName, "They aren't on that channel");
		return false;
	}
	if (adding) {
		if (!chan.isOperator(targetClient)) {
			chan.addOperator(targetClient);
			appliedModes += "+o";
			if (!appliedParams.empty()) appliedParams += " ";
			appliedParams += targetNick;
		}
	} else {
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


std::string Server::processIRCBotCommand(const std::string& command, const std::string& channelName) {
    (void)channelName;
    std::string cmd = command;
    if (cmd == "enable" || cmd == "bot enable" || cmd == "enable bot") {
        if (!_botEnabled) {
            _botEnabled = true;
            return "🤖 Bot is now ENABLED! ✅";
        } else {
            return "🤖 Bot is going sleepy... I'm already ENABLED! ❌";
        }
    }
    if (cmd == "disable" || cmd == "bot disable" || cmd == "disable bot") {
        if (_botEnabled) {
            _botEnabled = false;
            return "🤖 Bot is now DISABLED. Use '!bot enable' to reactivate me.";
        } else {
            return "🤖 Bot is going sleepy... I'm already ENABLED! ✅";
        }
    }
    if (cmd == "status" || cmd == "bot status") {
        return _botEnabled ? "🤖 Bot status: ENABLED ✅" : "🤖 Bot status: DISABLED ❌";
    }
    if (cmd == "stats") {
        updateBotStats("interaction", _client->getNickname());

        std::ostringstream oss;
        oss << "📊 DETAILED SERVER STATISTICS:\n";
        oss << "┌─ CONNECTION STATS ─────────────────┐\n";
        oss << "│ Connected Users: " << _clients.size() << " clients        │\n";
        oss << "│ Active Channels: " << _channels.size() << " channels       │\n";

        int totalMembers = 0;
        int maxChannelSize = 0;
        std::string largestChannel = "N/A";

        for (std::map<std::string, Channel>::const_iterator it = _channels.begin(); it != _channels.end(); ++it) {
            int members = it->second.getMemberCount();
            totalMembers += members;
            if (members > maxChannelSize) {
                maxChannelSize = members;
                largestChannel = it->first;
            }
        }
        oss << "│ 📈 Total Memberships: " << totalMembers << "            │\n";
        oss << "│ 🎯 Largest Channel: " << largestChannel;
        for (int i = largestChannel.length(); i < 10; i++) oss << " ";
        oss << "│\n";
        oss << "│ 🤖 Bot Interactions: " << _totalBotInteractions << "             │\n";
        oss << "│ 😄 Jokes Shared: " << _totalJokesShared << "                 │\n";
        oss << "│ ⏰ Server Uptime: ";
        std::string uptimeStr = getUptimeString();
        oss << uptimeStr;

		int paddingNeeded = 12 - uptimeStr.length();
        for (int i = 0; i < paddingNeeded; i++) oss << " ";
        oss << "│\n";
        oss << "└────────────────────────────────────┘\n";
        oss << "🔥 Status: OPERATIONAL & STABLE";

        saveBotStats();
        return oss.str();
    }
    if (cmd == "uptime") {
        updateBotStats("interaction", _client->getNickname());
        return "⏰ Server uptime: " + getUptimeString() + "\n🚀 Started: " + std::string(ctime(&_serverStartTime)).substr(0, 24);
    }
    if (cmd == "users") {
        updateBotStats("interaction", _client->getNickname());

        std::ostringstream oss;
        oss << "CONNECTED USERS (" << _clients.size() << " total):\n";

        int count = 0;
        for (std::map<int, Client*>::const_iterator it = _clients.begin(); it != _clients.end(); ++it) {
            if (it->second->isRegistered()) {
                oss << "• " << it->second->getNickname();
                if (count % 3 == 2) oss << "\n";
                else oss << "  ";
                count++;
            }
        }
        if (count % 3 != 0) oss << "\n";
        oss << "✨ All users are welcomed by IRCBot!";
        return oss.str();
    }
    if (cmd == "channels") {
        updateBotStats("interaction", _client->getNickname());

        std::ostringstream oss;
        oss << "🏠 ACTIVE CHANNELS (" << _channels.size() << " total):\n";

        for (std::map<std::string, Channel>::const_iterator it = _channels.begin(); it != _channels.end(); ++it) {
            oss << "• " << it->first << " (" << it->second.getMemberCount() << " users)";

            std::string modes = "";
            if (it->second.isInviteOnly()) modes += "i";
            if (it->second.isTopicProtected()) modes += "t";
            if (it->second.hasKey()) modes += "k";
            if (it->second.hasUserLimit()) modes += "l";

            if (!modes.empty()) {
                oss << " [+" << modes << "]";
            }
            if (!it->second.getTopic().empty()) {
                std::string topic = it->second.getTopic();
                if (topic.length() > 30) {
                    topic = topic.substr(0, 27) + "...";
                }
                oss << " - \"" << topic << "\"";
            }

            oss << "\n";
        }

        if (_channels.empty()) {
            oss << "No active channels. Create one with JOIN #channel!";
        } else {
            oss << "Join any channel to start chatting!\n";
            oss << "Use JOIN #channelname to join a channel";
        }

        return oss.str();
    }

    if (cmd == "joke") {
        updateBotStats("joke", _client->getNickname());
        const char* jokes[] = {
            "😄 Why do programmers prefer dark mode? Because light attracts bugs! 🐛",
            "😄 Why did the IRC user go broke? Too many /quit commands! 💸",
            "😄 What's a computer's favorite snack? Microchips! 🍟",
            "😄 How do you comfort a JavaScript bug? You console it! 💻",
            "😄 Why do Java developers wear glasses? Because they can't C# ! 👓",
            "😄 What's a programmer's favorite hangout place? Foo Bar! 🍺"
        };
        return jokes[rand() % 6];
    }

    if (cmd == "bot" || cmd == "help") {
        return "🤖 Bot Control Commands:\n"
               "• BOT enable/disable - Activate/deactivate bot\n"
               "• BOT status - Check bot status\n"
               "• BOT stats - Server statistics\n"
               "• BOT uptime - Server uptime\n"
               "• BOT users - Connected users\n"
               "• BOT channels - Active channels\n"
               "• BOT chat #channel - Enable chat mode\n"
               "• BOT chat exit - Disable chat mode\n"
               "• BOT joke - Tell a random joke\n"
               "💬 Conversational: hello, time, help, joke, bye in channels";
    }
    return "";
}

void Server::loadBannedWords() {
    std::ifstream file("banned_words.txt");
    if (!file.is_open()) {
        std::cout << YELLOW << "[WARNING] banned_words.txt file not found. Moderation disabled." << RESET << std::endl;
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::string word = line;
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        _bannedWords.push_back(word);
    }
    file.close();
}

bool Server::containsBannedWord(const std::string& message) {
    if (_bannedWords.empty()) return false;
    std::string lowerMessage = message;
    std::transform(lowerMessage.begin(), lowerMessage.end(), lowerMessage.begin(), ::tolower);
    for (std::vector<std::string>::const_iterator it = _bannedWords.begin(); it != _bannedWords.end(); ++it) {
        if (lowerMessage.find(*it) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void Server::moderateMessage(const std::string& message, const std::string& channelName) {
    if (!containsBannedWord(message)) return;

    std::map<std::string, Channel>::iterator it = _channels.find(channelName);
    if (it == _channels.end()) return;

    Channel& channel = it->second;
    std::string warningMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                            " PRIVMSG " + channelName +
                            " :⚠️ MODERATION: User " + _client->getNickname() +
                            " has been kicked for inappropriate language!";

    const std::set<Client*>& members = channel.getMembers();
    for (std::set<Client*>::const_iterator mit = members.begin(); mit != members.end(); ++mit) {
        sendToClient((*mit)->getFd(), warningMsg);
    }
    channel.part(_client);
    banUserFromChannel(_client->getNickname(), channelName);
    std::string privateMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                            " PRIVMSG " + _client->getNickname() +
                            " :🚫 You have been kicked AND BANNED from " + channelName +
                            " for inappropriate language. You cannot rejoin this channel.";
    sendToClient(_client->getFd(), privateMsg);
    std::cout << RED << "[MODERATION] " << _client->getNickname() << " expelled from " << channelName << " for: " << message << RESET << std::endl;
}

bool Server::isUserBannedFromChannel(const std::string& nickname, const std::string& channelName) {
    std::map<std::string, std::vector<std::string> >::const_iterator it = _channelBannedUsers.find(channelName);
    if (it == _channelBannedUsers.end()) {
        return false;
    }
    const std::vector<std::string>& bannedUsers = it->second;
    return std::find(bannedUsers.begin(), bannedUsers.end(), nickname) != bannedUsers.end();
}

void Server::banUserFromChannel(const std::string& nickname, const std::string& channelName) {
    _channelBannedUsers[channelName].push_back(nickname);
    std::cout << RED << "[BAN] " << nickname << " banni du canal " << channelName << RESET << std::endl;
}

bool Server::isInChatMode(int clientFd) {
    return _clientChatMode.find(clientFd) != _clientChatMode.end();
}

std::string Server::getCurrentChannel(int clientFd) {
    std::map<int, std::string>::const_iterator it = _clientChatMode.find(clientFd);
    return (it != _clientChatMode.end()) ? it->second : "";
}

void Server::setChatMode(int clientFd, const std::string& channel) {
    _clientChatMode[clientFd] = channel;
    std::cout << GREEN << "[CHAT MODE] Client fd[" << clientFd << "] entered chat mode for " << channel << RESET << std::endl;
}

void Server::exitChatMode(int clientFd) {
    std::map<int, std::string>::iterator it = _clientChatMode.find(clientFd);
    if (it != _clientChatMode.end()) {
        std::cout << YELLOW << "[CHAT MODE] Client fd[" << clientFd << "] exited chat mode for " << it->second << RESET << std::endl;
        _clientChatMode.erase(it);
    }
}

void Server::promptChatMode(int clientFd, const std::string& failedCommand) {
    _clientChatModePrompt[clientFd] = true;

    std::string promptMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                           " PRIVMSG " + _client->getNickname() +
                           " :I noticed you tried '" + failedCommand + "' - did you want to send a message?\n"
                           "Type 'yes' to enable Chat Mode and send messages directly without PRIVMSG!\n"
                           "Or type 'no' to continue with normal IRC commands.";
    sendToClient(clientFd, promptMsg);

    std::cout << CYAN << "[CHAT PROMPT] Offered chat mode to fd[" << clientFd << "] for command: " << failedCommand << RESET << std::endl;
}

bool Server::isAwaitingChatModeResponse(int clientFd) {
    return _clientChatModePrompt.find(clientFd) != _clientChatModePrompt.end() && _clientChatModePrompt[clientFd];
}

void Server::handleChatModeResponse(int clientFd, const std::string& response) {
    _clientChatModePrompt[clientFd] = false;

    if (response == "yes" || response == "YES" || response == "y" || response == "Y") {
        std::string targetChannel = "";
        for (std::map<std::string, Channel>::const_iterator it = _channels.begin(); it != _channels.end(); ++it) {
            if (it->second.isMember(_client)) {
                targetChannel = it->first;
                break;
            }
        }

        if (!targetChannel.empty()) {
            setChatMode(clientFd, targetChannel);
            std::string successMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                   " PRIVMSG " + _client->getNickname() +
                                   " :Chat Mode ENABLED for " + targetChannel + "!\n"
                                   "Now type messages directly. Use '!chat exit' to disable.";
            sendToClient(clientFd, successMsg);
        } else {
            std::string errorMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                 " PRIVMSG " + _client->getNickname() +
                                 " :Please join a channel first with: JOIN #channelname";
            sendToClient(clientFd, errorMsg);
        }
    } else {
        std::string cancelMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                              " PRIVMSG " + _client->getNickname() +
                              " :No problem! Continue using normal IRC commands.";
        sendToClient(clientFd, cancelMsg);
    }
}
