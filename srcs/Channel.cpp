#include "Channel.hpp"
#include "Client.hpp"
#include "colors.hpp"

Channel::Channel(const std::string& name)
	: _name(name), _topic(""), _password("")//, _userLimit(0),
	//   _inviteOnly(false), _topicRestricted(false),
	//   _passwordProtected(false), _limitUsers(false)
{}

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
	std::cout << GREEN << client->getNickname() << " added the channel " << getName() << RESET << std::endl;
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
