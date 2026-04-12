#include "Server.hpp"
#include <sstream>

void Server::handlePass(Client* client, const std::vector<std::string>& params) {
	if (client->isRegistered()) {
		sendNumericReply(client, 462, ":You may not reregister");
		return;
	}

	if (params.empty()) {
		sendNumericReply(client, 461, "PASS :Not enough parameters");
		return;
	}

	if (params[0] == _password) {
		client->setAuthenticated(true);
		client->setReceivedPass(true);
		client->checkRegistration();
		if (client->isRegistered())
			sendWelcome(client);
	} else {
		sendNumericReply(client, 464, ":Password incorrect");
	}
}

void Server::handleNick(Client* client, const std::vector<std::string>& params) {
	if (params.empty()) {
		sendNumericReply(client, 431, ":No nickname given");
		return;
	}

	std::string newNick = params[0];

	if (!isValidNickname(newNick)) {
		sendNumericReply(client, 432, newNick + " :Erroneous nickname");
		return;
	}

	// Check if nickname is already in use
	if (getClientByNickname(newNick)) {
		sendNumericReply(client, 433, newNick + " :Nickname is already in use");
		return;
	}

	std::string oldNick = client->getNickname();
	client->setNickname(newNick);
	client->setReceivedNick(true);

	// Notify channels if user was already registered
	if (client->isRegistered() && !oldNick.empty()) {
		std::string message = ":" + oldNick + " NICK :" + newNick;
		// Notify all channels where the user is present
		for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
			if (it->second->isMember(client)) {
				const std::set<Client*>& members = it->second->getMembers();
				for (std::set<Client*>::const_iterator mem = members.begin(); mem != members.end(); ++mem) {
					sendToClient(*mem, message);
				}
			}
		}
	}

	client->checkRegistration();
	if (client->isRegistered() && oldNick.empty())
		sendWelcome(client);
}

void Server::handleUser(Client* client, const std::vector<std::string>& params) {
	if (client->isRegistered()) {
		sendNumericReply(client, 462, ":You may not reregister");
		return;
	}

	if (params.size() < 4) {
		sendNumericReply(client, 461, "USER :Not enough parameters");
		return;
	}

	client->setUsername(params[0]);
	// params[1] and params[2] are typically hostname and servername (ignored)
	client->setRealname(params[3]);
	client->setReceivedUser(true);

	client->checkRegistration();
	if (client->isRegistered())
		sendWelcome(client);
}

void Server::handlePing(Client* client, const std::vector<std::string>& params) {
	if (params.empty()) {
		sendNumericReply(client, 409, ":No origin specified");
		return;
	}

	sendToClient(client, ":server PONG server :" + params[0]);
}

void Server::handleQuit(Client* client, const std::vector<std::string>& params) {
	std::string reason = params.empty() ? "Client quit" : params[0];
	
	// Notify all channels
	for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
		if (it->second->isMember(client)) {
			std::string message = ":" + client->getPrefix() + " QUIT :" + reason;
			const std::set<Client*>& members = it->second->getMembers();
			for (std::set<Client*>::const_iterator mem = members.begin(); mem != members.end(); ++mem) {
				if (*mem != client)
					sendToClient(*mem, message);
			}
		}
	}

	removeClient(client->getFd());
}

void Server::handleWho(Client* client, const std::vector<std::string>& params) {
	if (params.empty()) {
		sendNumericReply(client, 461, "WHO :Not enough parameters");
		return;
	}

	std::string target = params[0];

	// WHO for a channel
	if (target[0] == '#' || target[0] == '&') {
		Channel* channel = getChannel(target);
		if (!channel) {
			sendNumericReply(client, 315, target + " :End of WHO list");
			return;
		}

		// Send WHO reply for each member
		const std::set<Client*>& members = channel->getMembers();
		for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
			Client* member = *it;
			std::string flags = "H"; // H = Here (not away)
			if (channel->isOperator(member))
				flags += "@";
			
			// Format: 352 <client> <channel> <user> <host> <server> <nick> <flags> :<hopcount> <realname>
			std::ostringstream oss;
			oss << target << " "
				<< member->getUsername() << " "
				<< member->getHostname() << " "
				<< "server " << member->getNickname() << " "
				<< flags << " :0 " << member->getRealname();
			sendNumericReply(client, 352, oss.str());
		}
		sendNumericReply(client, 315, target + " :End of WHO list");
	}
	else {
		// WHO for a specific user
		Client* targetClient = getClientByNickname(target);
		if (targetClient) {
			// Find a common channel or use * if none
			std::string channelName = "*";
			for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
				if (it->second->isMember(targetClient) && it->second->isMember(client)) {
					channelName = it->second->getName();
					break;
				}
			}
			
			std::string flags = "H";
			// Check if user is operator in any common channel
			for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
				if (it->second->isMember(targetClient) && it->second->isOperator(targetClient)) {
					flags += "@";
					break;
				}
			}
			
			std::ostringstream oss;
			oss << channelName << " "
				<< targetClient->getUsername() << " "
				<< targetClient->getHostname() << " "
				<< "server " << targetClient->getNickname() << " "
				<< flags << " :0 " << targetClient->getRealname();
			sendNumericReply(client, 352, oss.str());
		}
		sendNumericReply(client, 315, target + " :End of WHO list");
	}
}
