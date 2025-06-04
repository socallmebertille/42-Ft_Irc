#ifndef CHANNEL_HPP
# define CHANNEL_HPP

#include <string>
#include <map>
#include <set> // stock sans doublons, et classe les elmts en ordre croissant

class Client;
class Channel
{
private:
	std::map<std::string, Channel>	_channels;

	std::string _name;
	std::string _topic;//cmd topic
	std::string _password; //	+k
	int _userLimit;//	+l

	std::set<Client*> _members;
	std::set<Client*> _operators;//+o
	std::set<Client*> _invited; // +i

	bool _inviteOnly; // +i
	bool _topicRestricted; // +t
	bool _passwordProtected; // +k
	bool _limitUsers; // +l

public:
	Channel(const std::string name);
	~Channel();

	const std::string& getName() const;
	void addMember(Client* client);
	bool isMember(Client* client) const;
	void join(Client* client);
	void part(Client* client);
	size_t getMemberCount() const;
};

#endif
