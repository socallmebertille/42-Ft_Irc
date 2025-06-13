#include "Channel.hpp"
#include "Client.hpp"
#include "colors.hpp"
#include <iostream>
#include <sys/socket.h>

Channel::Channel(const std::string& name)
	: _name(name), _topic(""), _inviteOnly(false), _topicProtected(false),
	  _key(""), _hasKey(false), _userLimit(0), _limitActive(false) {}

Channel::~Channel() {}

const std::string& Channel::getName() const {
	return _name;
}

void Channel::addMember(Client* client) {
	_members.insert(client);
}

bool Channel::isMember(Client* client) const {
	return _members.find(client) != _members.end();
}

void Channel::join(Client* client) {
	_members.insert(client);
	std::cout << GREEN << client->getNickname() << " joined the channel " << getName() << RESET << std::endl;
	std::cout << "There are currently " << getMemberCount() << " members in the channel : " << getName() << "." << std::endl;
}

void Channel::part(Client* client) {
	_members.erase(client);
	std::cout << RED << client->getNickname() << " left the channel " << getName() << RESET << std::endl;
	std::cout << "There are currently " << getMemberCount() << " members in the channel : " << getName() << "." << std::endl;
}

size_t Channel::getMemberCount() const {
	return _members.size();
}

const std::set<Client*>& Channel::getMembers() const {
	return _members;
}

void Channel::addOperator(Client* client) {
	_operators.insert(client);
}

void Channel::removeOperator(Client* client) {
	_operators.erase(client);
}

bool Channel::isOperator(Client* client) const {
	return _operators.find(client) != _operators.end();
}

void Channel::sendToAll(const std::string& msg) const {
	for (std::set<Client*>::const_iterator it = _members.begin(); it != _members.end(); ++it) {
		send((*it)->getFd(), msg.c_str(), msg.length(), 0);
	}
}

// ========== MODES ==========

void Channel::setInviteOnly(bool value) {
	_inviteOnly = value;
}

bool Channel::isInviteOnly() const {
	return _inviteOnly;
}

void Channel::setTopicProtected(bool value) {
	_topicProtected = value;
}

bool Channel::isTopicProtected() const {
	return _topicProtected;
}

void Channel::setKey(const std::string& key) {
	_key = key;
	_hasKey = true;
}

void Channel::removeKey() {
	_key.clear();
	_hasKey = false;
}

bool Channel::hasKey() const {
	return _hasKey;
}

const std::string& Channel::getKey() const {
	return _key;
}

void Channel::setUserLimit(int limit) {
	_userLimit = limit;
	_limitActive = true;
}

void Channel::removeUserLimit() {
	_userLimit = 0;
	_limitActive = false;
}

bool Channel::hasUserLimit() const {
	return _limitActive;
}

int Channel::getUserLimit() const {
	return _userLimit;
}

void Channel::invite(Client* client) {
	_invited.insert(client);
}

bool Channel::isInvited(Client* client) const {
	return _invited.find(client) != _invited.end();
}

// #include "Channel.hpp"
// #include "Client.hpp"
// #include "colors.hpp"

// Channel::Channel(const std::string& name)
// 	: _name(name), _topic(""), _password(""), _userLimit(0),
// 	  _inviteOnly(false), _topicRestricted(false),
// 	  _passwordProtected(false), _limitUsers(false) {}

// Channel::~Channel() {}

// const std::string& Channel::getName() const {
// 	return _name;
// }

// void Channel::addMember(Client* client) {
// 	_members.insert(client);
// }

// bool Channel::isMember(Client* client) const {
// 	return _members.find(client) != _members.end();
// }

// void Channel::join(Client* client) {
// 	_members.insert(client);
// 	std::cout << GREEN << client->getNickname() << " added the channel " << getName() << RESET << std::endl;
// 	std::cout << "There are currently " << getMemberCount() << " members in the channel : " << getName() << "." << std::endl;
// }

// void Channel::part(Client* client) {
// 	_members.erase(client);
// 	std::cout << RED << client->getNickname() << " left the channel " << getName() << RESET << std::endl;
// 	std::cout << "There are currently " << getMemberCount() << " members in the channel : " << getName() << "." << std::endl;
// }

// size_t Channel::getMemberCount() const {
// 	return _members.size();
// }

// const std::set<Client*>& Channel::getMembers() const {
// 	return _members;
// }

// void Channel::addOperator(Client* client) {
// 	_operators.insert(client);
// }

// bool Channel::isOperator(Client* client) const {
// 	return _operators.find(client) != _operators.end();
// }

// void Channel::sendToAll(const std::string& msg) const {
//     for (std::set<Client*>::const_iterator it = _members.begin(); it != _members.end(); ++it) {
//         send((*it)->getFd(), msg.c_str(), msg.length(), 0);
//     }
// }
