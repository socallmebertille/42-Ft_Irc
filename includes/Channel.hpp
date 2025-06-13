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
	// std::string _password;
	// int _userLimit;

	std::set<Client*> _members;
	std::set<Client*> _operators;// +o
	std::set<Client*> _invited;

	// bool _inviteOnly;
	// bool _topicRestricted;
	// bool _passwordProtected;
	// bool _limitUsers;

	bool _inviteOnly;				// +i
    bool _topicProtected;			// +t
    std::string _key;				// +k
    bool _hasKey;
    int _userLimit;					// +l
    bool _limitActive;

public:
	// Channel(const std::string& name);
	// ~Channel();

	// const std::string& getName() const;
	// const std::set<Client*>& getMembers() const;

	// void addMember(Client* client);
	// void join(Client* client);
	// void part(Client* client);
	// bool isMember(Client* client) const;

	// size_t getMemberCount() const;
	// void sendToAll(const std::string& msg) const;

	// void addOperator(Client* client);
	// bool isOperator(Client* client) const;

	// void invite(Client* client) {
	// 	_invited.insert(client);
	// }

	// bool isInvited(Client* client) const {
	// 	return _invited.find(client) != _invited.end();
	// }
	Channel(const std::string& name);
	~Channel();

	// Infos générales
	const std::string& getName() const;
	const std::set<Client*>& getMembers() const;
	size_t getMemberCount() const;

	// Gestion membres
	void addMember(Client* client);
	void join(Client* client);
	void part(Client* client);
	bool isMember(Client* client) const;
	void sendToAll(const std::string& msg) const;

	// Gestion opérateurs
	void addOperator(Client* client);
	void removeOperator(Client* client);
	bool isOperator(Client* client) const;

	// Gestion invitations
	void invite(Client* client);
	bool isInvited(Client* client) const;

	// Modes +i
	void setInviteOnly(bool value);
	bool isInviteOnly() const;

	// Modes +t
	void setTopicProtected(bool value);
	bool isTopicProtected() const;

	// Modes +k
	void setKey(const std::string& key);
	void removeKey();
	bool hasKey() const;
	const std::string& getKey() const;

	// Modes +l
	void setUserLimit(int limit);
	void removeUserLimit();
	bool hasUserLimit() const;
	int getUserLimit() const;
};

#endif

