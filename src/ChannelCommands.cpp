#include "Server.hpp"
#include <sstream>

void Server::handleJoin(Client* client, const std::vector<std::string>& params) {
	if (params.empty()) {
		sendNumericReply(client, 461, "JOIN :Not enough parameters");
		return;
	}

	std::string channelName = params[0];
	std::string key = params.size() > 1 ? params[1] : "";

	if (!isValidChannelName(channelName)) {
		sendNumericReply(client, 403, channelName + " :No such channel");
		return;
	}

	Channel* channel = getChannel(channelName);
	bool isNewChannel = (channel == NULL);

	if (isNewChannel) {
		channel = createChannel(channelName);
		channel->addMember(client);
		channel->addOperator(client); // First user becomes operator
	} else {
		// Check if already a member
		if (channel->isMember(client))
			return;

		// Check invite-only
		if (channel->isInviteOnly() && !channel->isInvited(client)) {
			sendNumericReply(client, 473, channelName + " :Cannot join channel (+i)");
			return;
		}

		// Check user limit
		if (channel->getUserLimit() > 0 && channel->getMemberCount() >= channel->getUserLimit()) {
			sendNumericReply(client, 471, channelName + " :Cannot join channel (+l)");
			return;
		}

		// Check key
		if (!channel->getKey().empty() && channel->getKey() != key) {
			sendNumericReply(client, 475, channelName + " :Cannot join channel (+k)");
			return;
		}

		channel->addMember(client);
	}

	// Send JOIN message to all members
	std::string joinMsg = ":" + client->getPrefix() + " JOIN " + channelName;
	const std::set<Client*>& members = channel->getMembers();
	for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
		sendToClient(*it, joinMsg);
	}

	// Send topic
	if (!channel->getTopic().empty()) {
		sendNumericReply(client, 332, channelName + " :" + channel->getTopic());
	} else {
		sendNumericReply(client, 331, channelName + " :No topic is set");
	}

	// Send names list
	sendNumericReply(client, 353, "= " + channelName + " :" + channel->getNicknameList());
	sendNumericReply(client, 366, channelName + " :End of /NAMES list");
}

void Server::handlePart(Client* client, const std::vector<std::string>& params) {
	if (params.empty()) {
		sendNumericReply(client, 461, "PART :Not enough parameters");
		return;
	}

	std::string channelName = params[0];
	std::string reason = params.size() > 1 ? params[1] : "";

	Channel* channel = getChannel(channelName);
	if (!channel) {
		sendNumericReply(client, 403, channelName + " :No such channel");
		return;
	}

	if (!channel->isMember(client)) {
		sendNumericReply(client, 442, channelName + " :You're not on that channel");
		return;
	}

	// Send PART message to all members
	std::string partMsg = ":" + client->getPrefix() + " PART " + channelName;
	if (!reason.empty())
		partMsg += " :" + reason;

	const std::set<Client*>& members = channel->getMembers();
	for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
		sendToClient(*it, partMsg);
	}

	channel->removeMember(client);

	// Remove empty channel
	if (channel->getMemberCount() == 0) {
		removeChannel(channelName);
	}
}

void Server::handlePrivmsg(Client* client, const std::vector<std::string>& params) {
	if (params.size() < 2) {
		sendNumericReply(client, 411, ":No recipient given (PRIVMSG)");
		return;
	}

	std::string target = params[0];
	std::string message = params[1];

	// Check if target is a channel
	if (target[0] == '#' || target[0] == '&') {
		Channel* channel = getChannel(target);
		if (!channel) {
			sendNumericReply(client, 403, target + " :No such channel");
			return;
		}

		if (!channel->isMember(client)) {
			sendNumericReply(client, 442, target + " :You're not on that channel");
			return;
		}

		// Send to all members except sender
		std::string privMsg = ":" + client->getPrefix() + " PRIVMSG " + target + " :" + message;
		const std::set<Client*>& members = channel->getMembers();
		for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
			if (*it != client)
				sendToClient(*it, privMsg);
		}
	} else {
		// Private message to user
		Client* targetClient = getClientByNickname(target);
		if (!targetClient) {
			sendNumericReply(client, 401, target + " :No such nick/channel");
			return;
		}

		std::string privMsg = ":" + client->getPrefix() + " PRIVMSG " + target + " :" + message;
		sendToClient(targetClient, privMsg);
	}
}

void Server::handleKick(Client* client, const std::vector<std::string>& params) {
	if (params.size() < 2) {
		sendNumericReply(client, 461, "KICK :Not enough parameters");
		return;
	}

	std::string channelName = params[0];
	std::string targetNick = params[1];
	std::string reason = params.size() > 2 ? params[2] : client->getNickname();

	Channel* channel = getChannel(channelName);
	if (!channel) {
		sendNumericReply(client, 403, channelName + " :No such channel");
		return;
	}

	if (!channel->isMember(client)) {
		sendNumericReply(client, 442, channelName + " :You're not on that channel");
		return;
	}

	if (!channel->isOperator(client)) {
		sendNumericReply(client, 482, channelName + " :You're not channel operator");
		return;
	}

	Client* targetClient = getClientByNickname(targetNick);
	if (!targetClient || !channel->isMember(targetClient)) {
		sendNumericReply(client, 441, targetNick + " " + channelName + " :They aren't on that channel");
		return;
	}

	// Send KICK message to all members
	std::string kickMsg = ":" + client->getPrefix() + " KICK " + channelName + " " + targetNick + " :" + reason;
	const std::set<Client*>& members = channel->getMembers();
	for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
		sendToClient(*it, kickMsg);
	}

	channel->removeMember(targetClient);

	// Remove empty channel
	if (channel->getMemberCount() == 0) {
		removeChannel(channelName);
	}
}

void Server::handleInvite(Client* client, const std::vector<std::string>& params) {
	if (params.size() < 2) {
		sendNumericReply(client, 461, "INVITE :Not enough parameters");
		return;
	}

	std::string targetNick = params[0];
	std::string channelName = params[1];

	Client* targetClient = getClientByNickname(targetNick);
	if (!targetClient) {
		sendNumericReply(client, 401, targetNick + " :No such nick/channel");
		return;
	}

	Channel* channel = getChannel(channelName);
	if (!channel) {
		sendNumericReply(client, 403, channelName + " :No such channel");
		return;
	}

	if (!channel->isMember(client)) {
		sendNumericReply(client, 442, channelName + " :You're not on that channel");
		return;
	}

	if (channel->isInviteOnly() && !channel->isOperator(client)) {
		sendNumericReply(client, 482, channelName + " :You're not channel operator");
		return;
	}

	if (channel->isMember(targetClient)) {
		sendNumericReply(client, 443, targetNick + " " + channelName + " :is already on channel");
		return;
	}

	// Add to invite list
	channel->addInvite(targetClient);

	// Send invite to target
	std::string inviteMsg = ":" + client->getPrefix() + " INVITE " + targetNick + " :" + channelName;
	sendToClient(targetClient, inviteMsg);

	// Confirm to sender
	sendNumericReply(client, 341, targetNick + " " + channelName);
}

void Server::handleTopic(Client* client, const std::vector<std::string>& params) {
	if (params.empty()) {
		sendNumericReply(client, 461, "TOPIC :Not enough parameters");
		return;
	}

	std::string channelName = params[0];
	Channel* channel = getChannel(channelName);

	if (!channel) {
		sendNumericReply(client, 403, channelName + " :No such channel");
		return;
	}

	if (!channel->isMember(client)) {
		sendNumericReply(client, 442, channelName + " :You're not on that channel");
		return;
	}

	// Query topic
	if (params.size() == 1) {
		if (channel->getTopic().empty()) {
			sendNumericReply(client, 331, channelName + " :No topic is set");
		} else {
			sendNumericReply(client, 332, channelName + " :" + channel->getTopic());
		}
		return;
	}

	// Set topic
	if (channel->isTopicRestricted() && !channel->isOperator(client)) {
		sendNumericReply(client, 482, channelName + " :You're not channel operator");
		return;
	}

	std::string newTopic = params[1];
	channel->setTopic(newTopic);

	// Broadcast topic change
	std::string topicMsg = ":" + client->getPrefix() + " TOPIC " + channelName + " :" + newTopic;
	const std::set<Client*>& members = channel->getMembers();
	for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
		sendToClient(*it, topicMsg);
	}
}
