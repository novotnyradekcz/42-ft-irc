#include "Channel.hpp"
#include <sstream>

Channel::Channel(const std::string& name) : _name(name), _inviteOnly(false),
	_topicRestricted(true), _userLimit(0) {
}

Channel::~Channel() {
}

// Getters
const std::string& Channel::getName() const {
	return _name;
}

const std::string& Channel::getTopic() const {
	return _topic;
}

const std::string& Channel::getKey() const {
	return _key;
}

const std::set<Client*>& Channel::getMembers() const {
	return _members;
}

const std::set<Client*>& Channel::getOperators() const {
	return _operators;
}

bool Channel::isInviteOnly() const {
	return _inviteOnly;
}

bool Channel::isTopicRestricted() const {
	return _topicRestricted;
}

size_t Channel::getUserLimit() const {
	return _userLimit;
}

// Setters
void Channel::setTopic(const std::string& topic) {
	_topic = topic;
}

void Channel::setKey(const std::string& key) {
	_key = key;
}

void Channel::setInviteOnly(bool inviteOnly) {
	_inviteOnly = inviteOnly;
}

void Channel::setTopicRestricted(bool restricted) {
	_topicRestricted = restricted;
}

void Channel::setUserLimit(size_t limit) {
	_userLimit = limit;
}

// Member management
void Channel::addMember(Client* client) {
	_members.insert(client);
	_invitedUsers.erase(client); // Remove from invited list if present
}

void Channel::removeMember(Client* client) {
	_members.erase(client);
	_operators.erase(client);
}

bool Channel::isMember(Client* client) const {
	return _members.find(client) != _members.end();
}

size_t Channel::getMemberCount() const {
	return _members.size();
}

// Operator management
void Channel::addOperator(Client* client) {
	_operators.insert(client);
}

void Channel::removeOperator(Client* client) {
	_operators.erase(client);
}

bool Channel::isOperator(Client* client) const {
	return _operators.find(client) != _operators.end();
}

// Invite management
void Channel::addInvite(Client* client) {
	_invitedUsers.insert(client);
}

void Channel::removeInvite(Client* client) {
	_invitedUsers.erase(client);
}

bool Channel::isInvited(Client* client) const {
	return _invitedUsers.find(client) != _invitedUsers.end();
}

// Broadcasting
void Channel::broadcast(const std::string& message, Client* exclude) {
	(void)message;
	(void)exclude;
	// The actual sending will be handled by the Server class
	// This function is kept for future extensibility
}

// Get list of nicknames
std::string Channel::getNicknameList() const {
	std::string list;
	for (std::set<Client*>::const_iterator it = _members.begin(); it != _members.end(); ++it) {
		if (!list.empty())
			list += " ";
		if (isOperator(*it))
			list += "@";
		list += (*it)->getNickname();
	}
	return list;
}
