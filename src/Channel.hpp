#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include <set>
#include "Client.hpp"

class Channel {
private:
	std::string				_name;
	std::string				_topic;
	std::string				_key;
	std::set<Client*>		_members;
	std::set<Client*>		_operators;
	std::set<Client*>		_invitedUsers;
	bool					_inviteOnly;
	bool					_topicRestricted;
	size_t					_userLimit;

public:
	Channel(const std::string& name);
	~Channel();

	// Getters
	const std::string& getName() const;
	const std::string& getTopic() const;
	const std::string& getKey() const;
	const std::set<Client*>& getMembers() const;
	const std::set<Client*>& getOperators() const;
	bool isInviteOnly() const;
	bool isTopicRestricted() const;
	size_t getUserLimit() const;

	// Setters
	void setTopic(const std::string& topic);
	void setKey(const std::string& key);
	void setInviteOnly(bool inviteOnly);
	void setTopicRestricted(bool restricted);
	void setUserLimit(size_t limit);

	// Member management
	void addMember(Client* client);
	void removeMember(Client* client);
	bool isMember(Client* client) const;
	size_t getMemberCount() const;

	// Operator management
	void addOperator(Client* client);
	void removeOperator(Client* client);
	bool isOperator(Client* client) const;

	// Invite management
	void addInvite(Client* client);
	void removeInvite(Client* client);
	bool isInvited(Client* client) const;

	// Broadcasting
	void broadcast(const std::string& message, Client* exclude = NULL);

	// Get list of nicknames
	std::string getNicknameList() const;
};

#endif
