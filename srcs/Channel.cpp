#include "Channel.hpp"
#include "Client.hpp"
#include "colors.hpp"

//A VERIF DEMAIN AVEC SARAH
	//membre qui se connecte, ds channel j'ai choisi de gerer avc ptr

Channel::Channel(std::string name):
	_name(name), _topic(""), _password(""),
	_userLimit(0), _inviteOnly(false),
	_topicRestricted(false), _passwordProtected(false),
	_limitUsers(false)
{
	if (_userLimit < 0)
		_userLimit = 0; // Limite d'utilisateurs ne peut pas être négative
	if (_inviteOnly)
		_invited.clear(); // Si le channel est invite-only, on vide la liste des invités
	if (_topicRestricted)
		_topic = ""; // Si le channel a un topic restreint, on le vide
	if (_passwordProtected)
		_password = ""; // Si le channel est protégé par mot de passe, on le vide
	if (_limitUsers)
		_members.clear(); // Si le channel a une limite d'utilisateurs, on vide la liste des membres
}

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

void Channel::join(Client* client) {
	_members.insert(client);
	std::cout << GREEN << "New member added the channel" << RESET << std::endl;//a replace par methode broadcats?
}

void Channel::part(Client* client) {
	_members.erase(client);
	std::cout << RED << "A member left the channel" << RESET << std::endl; //a replace par methode broadcats?
}
//Prévoir clairement comment notifier les clients via le réseau lorsque
// des événements se produisent (rejoindre/quitter channel, etc.)
size_t Channel::getMemberCount() const {
    return _members.size();
}
