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
