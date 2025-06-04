#include "Client.hpp"

Client::Client(int fd) : _fd(fd), _authenticated(false) {}

Client::~Client() {}

int Client::getFd() const{ return _fd; }
const std::string& Client::getUsername() const{ return _username; }
const std::string& Client::getNickname() const{ return _nickname; }
const std::string& Client::getRealname() const{ return _realName; }
const std::string& Client::getPassword() const { return _password; }

void Client::setUsername(const std::string& user){ _username = user; }
void Client::setNickname(const std::string& nick){ _nickname = nick; }
void Client::setRealname(const std::string& real) {_realName = real; }
void Client::setPassword(const std::string& passW) { _password = passW;}

bool Client::isAuthenticated() const { return _authenticated; }

void Client::authenticate() { _authenticated = true; }
