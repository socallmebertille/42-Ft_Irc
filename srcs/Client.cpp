#include "Client.hpp"

Client::Client(int fd, const std::string& ip) : _fd(fd), _ip(ip), _isRegistered(false) {}

Client::~Client() {}

int Client::getFd() const{ return _fd; }
std::string Client::getIp() const{ return _ip; }
const std::string& Client::getUsername() const{ return _username; }
const std::string& Client::getNickname() const{ return _nickname; }
const std::string& Client::getRealname() const{ return _realName; }
const std::string& Client::getPassword() const { return _password; }
bool Client::hasPassword() const { return _hasPassword; }
bool Client::hasNick() const { return _hasNick; }
bool Client::hasUser() const { return _hasUser; }
std::string Client::getPrefix() const { return _nickname + "!" + _username + "@localhost"; }

bool Client::setNickname(const std::string& nick){
	if (nick.empty())
		return false;
	if (nick.find(' ') != std::string::npos)
		return false;
	_nickname = nick;
	return true;
}
void Client::setUsername(const std::string& user){ _username = user; }
void Client::setRealname(const std::string& real) {_realName = real; }
void Client::setPassword(const std::string& passW) { _password = passW;}
void Client::markPassword() { _hasPassword = true; }
void Client::markNick() { _hasNick = true; }
void Client::markUser() { _hasUser = true; }

bool Client::isRegistered() const {
    return !_nickname.empty() && !_username.empty(); 
}

void Client::registerUser(const std::string& nick, const std::string& user, const std::string& real) {
	setNickname(nick);
	setUsername(user);
	setRealname(real);
	_isRegistered = true;
}