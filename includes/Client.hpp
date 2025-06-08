#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <iostream>
# include <unistd.h>
# include <sstream>
# include <algorithm>  // Pour std::transform

class Client
{
private:
	int _fd;//pr savoir quand et ou le client a besoin de communiquer(util pour poll)
	std::string _ip; // IP address of the client
	bool _isRegistered;
	std::string _username;
	bool _hasUser;
	std::string _nickname;
	bool _hasNick;
	std::string _realName;
	std::string _password; //pr rej un channel protege
	bool _hasPassword;
	std::string _readBuf, _command, _arg;   // tampons entrants et CMD + ARG
    bool _clientType; // true if netcat (msg without \r)
	int _space;
	bool _passOk;
	bool _passErrorSent;

public:
	Client(int fd, const std::string& ip);
	~Client();

	int getFd() const;
	std::string getIp() const;
	bool isRegistered() const;

	const std::string& getUsername() const;
	const std::string& getNickname() const;
	const std::string& getRealname() const;
	const std::string& getPassword() const;
	bool isPasswordOk() const;
	bool hasNick() const;
	bool hasUser() const;
	std::string getPrefix() const; // format "nickname!username@localhost"
	std::string& getBuffer();
	std::string getCmd() const;
	std::string getArg() const;
	bool getClientType() const;
	int getSpace() const;

	void setUsername(const std::string& user);
	bool setNickname(const std::string& nick);
	void setRealname(const std::string& real);
	void setPassword(const std::string& passW);
	void markPassword();
	void markNick();
	void markUser();
	void setBuf(const std::string& buf);
	void eraseBuf();
	void setArg(const std::string& arg);
	void setClientType(bool type);
	void registerUser(const std::string& nick, const std::string& user, const std::string& real);

	void parseLine();

	void setPasswordOk(bool ok);
	void setCommand(const std::string& cmd);

	bool hasSentPassError() const { return _passErrorSent; }
	void setPassErrorSent(bool v) { _passErrorSent = v; }

};

#endif
