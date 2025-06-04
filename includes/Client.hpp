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
	std::string _nickname;
	std::string _realName;
	std::string _password; //pr rej un channel protege

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

	void setUsername(const std::string& user);
	bool setNickname(const std::string& nick);
	void setRealname(const std::string& real);
	void setPassword(const std::string& passW);
	void registerUser(const std::string& nick, const std::string& user, const std::string& real);
};

#endif
