#include "Client.hpp"

Client::Client(int fd, const std::string& ip):
_fd(fd), _ip(ip), _authenticated(false), _username(""), _hasUser(false), _nickname(""),
_hasNick(false), _realName(""), _password(""), _hasPassword(false), _readBuf(""),
_command(""), _arg(""), _clientType(false), _space(0)
{}

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
std::string Client::getBuffer() const { return _readBuf; }
std::string Client::getCmd() const { return _command; }
std::string Client::getArg() const { return _arg; }
bool Client::getClientType() const { return _clientType; }

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
void Client::setBuf(const std::string& buf)
{
	_readBuf.append(buf);
}
void Client::setClientType(bool type) { _clientType = type; }

bool Client::isAuthenticated() const { return _authenticated; }

void Client::authenticate() {
	if (!_password.empty() && !_nickname.empty() && !_username.empty())
    	_authenticated = true;
	else
		_authenticated = false;
}

void Client::registerUser(const std::string& nick, const std::string& user, const std::string& real) {
	setNickname(nick);
	setUsername(user);
	setRealname(real);
	_authenticated = true;
}

void Client::parseLine() {
    std::istringstream iss(_readBuf);
    if (_readBuf.find("\r\n", _readBuf.size() - 3) != std::string::npos) {
        _readBuf.erase(_readBuf.find("\r\n", _readBuf.size() - 3), 2);
		_clientType = false;
    }
    else if (_readBuf.find("\n", _readBuf.size() - 2) != std::string::npos) {
        _readBuf.erase(_readBuf.find("\n", _readBuf.size() - 2), 1);
		_clientType = true;
    }
    iss >> _command >> _arg;
    _readBuf.erase(0, _command.size());
    if (_readBuf[0] == ' ') {
        _readBuf.erase(0, 1);
    }
    if (!_arg.empty()) {
        if (_arg[_arg.size() - 1] != ' ' || _arg[_arg.size() - 1] != ':')
            _space = 1;
        _readBuf.erase(0, _arg.size());
        if (_readBuf[0] == ' ')
            _readBuf.erase(0, 1);
        if (_readBuf.empty())
            _space = 0;
    }
}
