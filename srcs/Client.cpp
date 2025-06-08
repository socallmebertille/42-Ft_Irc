#include "Client.hpp"

Client::Client(int fd, const std::string& ip):
_fd(fd), _ip(ip), _isRegistered(false), _username(""), _hasUser(false), _nickname(""),
_hasNick(false), _realName(""), _password(""), _hasPassword(false), _readBuf(""),
_command(""), _arg(""), _clientType(false), _space(0), _passOk(false), _passErrorSent(false
)
{}

Client::~Client() {}

int Client::getFd() const{ return _fd; }
std::string Client::getIp() const{ return _ip; }
const std::string& Client::getUsername() const{ return _username; }
const std::string& Client::getNickname() const{ return _nickname; }
const std::string& Client::getRealname() const{ return _realName; }
const std::string& Client::getPassword() const { return _password; }
bool Client::isPasswordOk() const { return _passOk; }

bool Client::hasNick() const { return _hasNick; }
bool Client::hasUser() const { return _hasUser; }
std::string Client::getPrefix() const { return _nickname + "!" + _username + "@localhost"; }
std::string& Client::getBuffer() { return _readBuf; }
std::string Client::getCmd() const { return _command; }
std::string Client::getArg() const { return _arg; }
bool Client::getClientType() const { return _clientType; }
int Client::getSpace() const { return _space; }

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
void Client::setBuf(const std::string& buf) {
	_readBuf.append(buf);
}
void Client::eraseBuf() { _readBuf.erase(0, _readBuf.size()); }
void Client::setArg(const std::string& arg)
{
	_arg.erase(0, _arg.size());
	_arg = arg;
}
void Client::setClientType(bool type) { _clientType = type; }

bool Client::isRegistered() const { return _isRegistered; }

void Client::registerUser(const std::string& nick, const std::string& user, const std::string& real) {
	setNickname(nick);
	setUsername(user);
	setRealname(real);
	_isRegistered = true;
}

void Client::parseLine() {
    std::string line = _readBuf;
    size_t pos = std::string::npos;

    // Chercher tous les types possibles de délimiteurs
    pos = line.find("\r\n");
    if (pos == std::string::npos) {
        pos = line.find('\n');
    }
    if (pos == std::string::npos) {
        return; // Pas de commande complète
    }

    std::string cmd = line.substr(0, pos);
    _readBuf = (pos + 2 <= line.length()) ? line.substr(pos + 2) : "";

    // Nettoyer la commande
    if (!cmd.empty()) {
        if (cmd[0] == ':') {
            cmd = cmd.substr(1);
        }

        // Parser la commande et les arguments
        std::istringstream iss(cmd);
        iss >> _command;
        std::getline(iss, _arg);
        if (!_arg.empty() && _arg[0] == ' ') {
            _arg = _arg.substr(1);
        }
    }
}

void Client::setPasswordOk(bool ok) { _passOk = ok; }
