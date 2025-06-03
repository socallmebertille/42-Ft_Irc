#include "Channel.hpp"

Channel::Channel(std::string name):
	_name(name), _topic(""), _password(""),
	_userLimit(0), _inviteOnly(false),
	_topicRestricted(false), _passwordProtected(false),
	_limitUsers(false) {}

Channel::~Channel() {

}

const std::string& Channel::getName() const {
	return _name;
}

void Channel::addMember(Client* client){
	_members.insert(client);
}

bool Channel::isMember(Client* client) const{
	return _members.find(client) != _members.end();
}
