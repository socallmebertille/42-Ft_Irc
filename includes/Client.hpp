#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <iostream>

class Client
{
private:
	int _fd;//pr savoir quand et ou le client a besoin de communiquer(util pour poll)
	std::string _ip; // IP address of the client
	bool _authenticated;
	std::string _username;
	bool _hasUser;
	std::string _nickname;
	bool _hasNick;
	std::string _realName;
	std::string _password; //pr rej un channel protege
	bool _hasPassword;
	std::string _readBuf;   // tampons entrants
    bool _clientType = false; // true if netcat (msg without \r)

public:
	Client(int fd, const std::string& ip);
	~Client();

	int getFd() const;
	std::string getIp() const;
	bool isAuthenticated() const;
	void authenticate();

	const std::string& getUsername() const;
	const std::string& getNickname() const;
	const std::string& getRealname() const;
	const std::string& getPassword() const;
	bool hasPassword() const;
	bool hasNick() const;
	bool hasUser() const;
	std::string getPrefix() const; // format "nickname!username@localhost"
	std::string getBuffer() const;
	bool getClientType() const;

	void setUsername(const std::string& user);
	bool setNickname(const std::string& nick);
	void setRealname(const std::string& real);
	void setPassword(const std::string& passW);
	void markPassword();
	void markNick();
	void markUser();
	void setBuf(const std::string& buf, ssize_t bytesRead);
	void setClientType(bool type);
	void registerUser(const std::string& nick, const std::string& user, const std::string& real);
};

#endif
