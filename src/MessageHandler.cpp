#include "Server.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <sstream>
#include <iostream>

void Server::handleClientData(int fd) {
	char buffer[512];
	int bytesRead = recv(fd, buffer, sizeof(buffer) - 1, 0);

	if (bytesRead <= 0) {
		// Client disconnected or error
		removeClient(fd);
		return;
	}

	buffer[bytesRead] = '\0';
	Client* client = getClientByFd(fd);
	if (!client)
		return;

	// Append to client buffer
	client->appendToBuffer(std::string(buffer, bytesRead));

	// Process complete messages (ending with \r\n)
	std::string& buf = const_cast<std::string&>(client->getBuffer());
	size_t pos;
	while ((pos = buf.find("\r\n")) != std::string::npos) {
		std::string message = buf.substr(0, pos);
		buf.erase(0, pos + 2);
		
		if (!message.empty()) {
			std::cout << "Received from " << fd << ": " << message << std::endl;
			processMessage(client, message);
		}
	}

	// Handle partial data (nc ctrl+D test case)
	if (!buf.empty() && buf.find('\n') != std::string::npos) {
		pos = buf.find('\n');
		std::string message = buf.substr(0, pos);
		buf.erase(0, pos + 1);
		if (!message.empty() && message[message.size() - 1] == '\r')
			message.erase(message.size() - 1);
		if (!message.empty()) {
			std::cout << "Received (partial) from " << fd << ": " << message << std::endl;
			processMessage(client, message);
		}
	}
}

std::vector<std::string> Server::parseMessage(const std::string& message) {
	std::vector<std::string> tokens;
	std::istringstream iss(message);
	std::string token;
	bool colonFound = false;

	while (iss >> token) {
		if (token[0] == ':' && !colonFound) {
			colonFound = true;
			// Get the rest of the line as one token
			std::string rest;
			std::getline(iss, rest);
			token = token.substr(1) + rest;
			tokens.push_back(token);
			break;
		}
		tokens.push_back(token);
	}

	return tokens;
}

void Server::processMessage(Client* client, const std::string& message) {
	std::vector<std::string> tokens = parseMessage(message);
	if (tokens.empty())
		return;

	std::string command = tokens[0];
	// Convert command to uppercase
	for (size_t i = 0; i < command.size(); ++i) {
		if (command[i] >= 'a' && command[i] <= 'z')
			command[i] = command[i] - 'a' + 'A';
	}

	std::vector<std::string> params(tokens.begin() + 1, tokens.end());

	// Route to appropriate handler
	if (command == "CAP") {
		// Handle capability negotiation (modern IRC clients like Halloy)
		// We don't support any extended capabilities, so respond with empty list
		if (!params.empty()) {
			std::string subcommand = params[0];
			for (size_t i = 0; i < subcommand.size(); ++i) {
				if (subcommand[i] >= 'a' && subcommand[i] <= 'z')
					subcommand[i] = subcommand[i] - 'a' + 'A';
			}
			if (subcommand == "LS" || subcommand == "LIST")
				sendToClient(client, ":server CAP * LS :");
			else if (subcommand == "REQ")
				sendToClient(client, ":server CAP * NAK :" + (params.size() > 1 ? params[1] : ""));
			else if (subcommand == "END")
				; // Client finished negotiation, continue with registration
		}
		return;
	}
	else if (command == "PASS")
		handlePass(client, params);
	else if (command == "NICK")
		handleNick(client, params);
	else if (command == "USER")
		handleUser(client, params);
	else if (command == "PING")
		handlePing(client, params);
	else if (!client->isRegistered()) {
		sendNumericReply(client, 451, ":You have not registered");
		return;
	}
	else if (command == "JOIN")
		handleJoin(client, params);
	else if (command == "PART")
		handlePart(client, params);
	else if (command == "PRIVMSG")
		handlePrivmsg(client, params);
	else if (command == "KICK")
		handleKick(client, params);
	else if (command == "INVITE")
		handleInvite(client, params);
	else if (command == "TOPIC")
		handleTopic(client, params);
	else if (command == "MODE")
		handleMode(client, params);
	else if (command == "QUIT")
		handleQuit(client, params);
	else if (command == "WHO")
		handleWho(client, params);
	else {
		sendNumericReply(client, 421, command + " :Unknown command");
	}
}

void Server::sendNumericReply(Client* client, int code, const std::string& message) {
	std::ostringstream oss;
	oss << ":server ";
	// Pad the numeric code to 3 digits (required by IRC spec)
	if (code < 10)
		oss << "00";
	else if (code < 100)
		oss << "0";
	oss << code << " ";
	if (!client->getNickname().empty())
		oss << client->getNickname();
	else
		oss << "*";
	oss << " " << message;
	sendToClient(client, oss.str());
}

void Server::sendWelcome(Client* client) {
	sendNumericReply(client, 1, ":Welcome to the IRC Network " + client->getPrefix());
	sendNumericReply(client, 2, ":Your host is server, running version 1.0");
	sendNumericReply(client, 3, ":This server was created just now");
	sendNumericReply(client, 4, "server 1.0 o o");
}

bool Server::isValidNickname(const std::string& nickname) {
	if (nickname.empty() || nickname.length() > 9)
		return false;

	// First character must be a letter
	if (!((nickname[0] >= 'a' && nickname[0] <= 'z') ||
		  (nickname[0] >= 'A' && nickname[0] <= 'Z')))
		return false;

	// Rest can be letters, digits, or special characters
	for (size_t i = 1; i < nickname.length(); ++i) {
		char c = nickname[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			  (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '[' || c == ']'))
			return false;
	}

	return true;
}

bool Server::isValidChannelName(const std::string& name) {
	if (name.empty() || name.length() > 50)
		return false;

	if (name[0] != '#' && name[0] != '&')
		return false;

	for (size_t i = 1; i < name.length(); ++i) {
		if (name[i] == ' ' || name[i] == ',' || name[i] == ':')
			return false;
	}

	return true;
}
