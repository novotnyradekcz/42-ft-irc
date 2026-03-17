#include "Server.hpp"
#include <sstream>
#include <cstdlib>

void Server::handleMode(Client* client, const std::vector<std::string>& params) {
	if (params.empty()) {
		sendNumericReply(client, 461, "MODE :Not enough parameters");
		return;
	}

	std::string target = params[0];

	// Only handle channel modes (not user modes)
	if (target[0] != '#' && target[0] != '&') {
		sendNumericReply(client, 502, ":Can't change mode for other users");
		return;
	}

	Channel* channel = getChannel(target);
	if (!channel) {
		sendNumericReply(client, 403, target + " :No such channel");
		return;
	}

	if (!channel->isMember(client)) {
		sendNumericReply(client, 442, target + " :You're not on that channel");
		return;
	}

	// Query mode
	if (params.size() == 1) {
		std::string modes = "+";
		if (channel->isInviteOnly())
			modes += "i";
		if (channel->isTopicRestricted())
			modes += "t";
		if (!channel->getKey().empty())
			modes += "k";
		if (channel->getUserLimit() > 0)
			modes += "l";

		sendNumericReply(client, 324, target + " " + modes);
		return;
	}

	// Set mode - must be operator
	if (!channel->isOperator(client)) {
		sendNumericReply(client, 482, target + " :You're not channel operator");
		return;
	}

	std::string modeString = params[1];
	size_t paramIndex = 2;
	bool adding = true;
	std::string appliedModes;
	std::string modeParams;

	for (size_t i = 0; i < modeString.length(); ++i) {
		char mode = modeString[i];

		if (mode == '+') {
			adding = true;
			continue;
		} else if (mode == '-') {
			adding = false;
			continue;
		}

		switch (mode) {
			case 'i': // Invite-only
				channel->setInviteOnly(adding);
				appliedModes += (adding ? "+" : "-");
				appliedModes += "i";
				break;

			case 't': // Topic restricted
				channel->setTopicRestricted(adding);
				appliedModes += (adding ? "+" : "-");
				appliedModes += "t";
				break;

			case 'k': // Channel key
				if (adding) {
					if (paramIndex < params.size()) {
						channel->setKey(params[paramIndex]);
						appliedModes += "+k";
						modeParams += " " + params[paramIndex];
						paramIndex++;
					}
				} else {
					channel->setKey("");
					appliedModes += "-k";
				}
				break;

			case 'o': // Operator privilege
				if (paramIndex < params.size()) {
					Client* targetClient = getClientByNickname(params[paramIndex]);
					if (targetClient && channel->isMember(targetClient)) {
						if (adding) {
							channel->addOperator(targetClient);
						} else {
							channel->removeOperator(targetClient);
						}
						appliedModes += (adding ? "+" : "-");
						appliedModes += "o";
						modeParams += " " + params[paramIndex];
					}
					paramIndex++;
				}
				break;

			case 'l': // User limit
				if (adding) {
					if (paramIndex < params.size()) {
						int limit = std::atoi(params[paramIndex].c_str());
						if (limit > 0) {
							channel->setUserLimit(limit);
							appliedModes += "+l";
							modeParams += " " + params[paramIndex];
						}
						paramIndex++;
					}
				} else {
					channel->setUserLimit(0);
					appliedModes += "-l";
				}
				break;

			default:
				sendNumericReply(client, 472, std::string(1, mode) + " :is unknown mode char to me");
				break;
		}
	}

	// Broadcast mode change
	if (!appliedModes.empty()) {
		std::string modeMsg = ":" + client->getPrefix() + " MODE " + target + " " + appliedModes + modeParams;
		const std::set<Client*>& members = channel->getMembers();
		for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
			sendToClient(*it, modeMsg);
		}
	}
}
