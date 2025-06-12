#ifndef CHANNEL_HPP
# define CHANNEL_HPP

# include <string>
# include <set>
# include <sys/socket.h>

class Client;

class Channel {
private:
	std::string _name;
	std::string _topic;
	std::string _password;
	int _userLimit;

	std::set<Client*> _members;
	std::set<Client*> _operators;
	std::set<Client*> _invited;

	bool _inviteOnly;
	bool _topicRestricted;
	bool _passwordProtected;
	bool _limitUsers;

public:
	Channel(const std::string& name);
	~Channel();

	const std::string& getName() const;
	const std::set<Client*>& getMembers() const;

	void addMember(Client* client);
	void join(Client* client);
	void part(Client* client);
	bool isMember(Client* client) const;

	size_t getMemberCount() const;
	void sendToAll(const std::string& msg) const;

	void addOperator(Client* client);
	bool isOperator(Client* client) const;

	void invite(Client* client) {
		_invited.insert(client);
	}

	bool isInvited(Client* client) const {
		return _invited.find(client) != _invited.end();
	}
};

#endif

