#include "Server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cerrno>

Server::Server(int port, const std::string& password) : _port(port), _password(password), _serverSocket(-1) {
	setupServer();
}

Server::~Server() {
	// Clean up all clients
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		close(it->first);
		delete it->second;
	}
	_clients.clear();

	// Clean up all channels
	for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
		delete it->second;
	}
	_channels.clear();

	// Close server socket
	if (_serverSocket != -1)
		close(_serverSocket);
}

void Server::setupServer() {
	// Create socket
	_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (_serverSocket < 0) {
		throw std::runtime_error("Failed to create socket");
	}

	// Set socket to non-blocking
	if (fcntl(_serverSocket, F_SETFL, O_NONBLOCK) < 0) {
		close(_serverSocket);
		throw std::runtime_error("Failed to set socket to non-blocking");
	}

	// Set socket options to reuse address
	int opt = 1;
	if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		close(_serverSocket);
		throw std::runtime_error("Failed to set socket options");
	}

	bindSocket();
	listenSocket();

	// Add server socket to poll
	struct pollfd pfd;
	pfd.fd = _serverSocket;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollFds.push_back(pfd);

	std::cout << "Server started on port " << _port << std::endl;
}

void Server::bindSocket() {
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(_port);

	if (bind(_serverSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		close(_serverSocket);
		throw std::runtime_error("Failed to bind socket");
	}
}

void Server::listenSocket() {
	if (listen(_serverSocket, 10) < 0) {
		close(_serverSocket);
		throw std::runtime_error("Failed to listen on socket");
	}
}

void Server::run() {
	while (true) {
		int ret = poll(&_pollFds[0], _pollFds.size(), -1);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			throw std::runtime_error("Poll failed");
		}

		// Check all file descriptors
		for (size_t i = 0; i < _pollFds.size(); ++i) {
			if (_pollFds[i].revents & POLLIN) {
				if (_pollFds[i].fd == _serverSocket) {
					acceptNewClient();
				} else {
					handleClientData(_pollFds[i].fd);
				}
			}
		}
	}
}

void Server::acceptNewClient() {
	struct sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);

	int clientSocket = accept(_serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
	if (clientSocket < 0) {
		if (errno != EWOULDBLOCK && errno != EAGAIN) {
			std::cerr << "Failed to accept client" << std::endl;
		}
		return;
	}

	// Set client socket to non-blocking
	if (fcntl(clientSocket, F_SETFL, O_NONBLOCK) < 0) {
		std::cerr << "Failed to set client socket to non-blocking" << std::endl;
		close(clientSocket);
		return;
	}

	// Create new client
	Client* client = new Client(clientSocket);
	client->setHostname(inet_ntoa(clientAddr.sin_addr));
	_clients[clientSocket] = client;

	// Add to poll
	struct pollfd pfd;
	pfd.fd = clientSocket;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollFds.push_back(pfd);

	std::cout << "New client connected: " << clientSocket << std::endl;
}

void Server::removeClient(int fd) {
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;

	Client* client = it->second;

	// Remove from all channels
	for (std::map<std::string, Channel*>::iterator chanIt = _channels.begin(); chanIt != _channels.end();) {
		if (chanIt->second->isMember(client)) {
			chanIt->second->removeMember(client);
			// Remove empty channels
			if (chanIt->second->getMemberCount() == 0) {
				delete chanIt->second;
				_channels.erase(chanIt++);
				continue;
			}
		}
		++chanIt;
	}

	// Remove from poll
	for (std::vector<struct pollfd>::iterator pollIt = _pollFds.begin(); pollIt != _pollFds.end(); ++pollIt) {
		if (pollIt->fd == fd) {
			_pollFds.erase(pollIt);
			break;
		}
	}

	close(fd);
	delete client;
	_clients.erase(it);

	std::cout << "Client disconnected: " << fd << std::endl;
}

Client* Server::getClientByFd(int fd) {
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it != _clients.end())
		return it->second;
	return NULL;
}

Client* Server::getClientByNickname(const std::string& nickname) {
	std::string lowerNick = toLowerCase(nickname);
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		if (toLowerCase(it->second->getNickname()) == lowerNick)
			return it->second;
	}
	return NULL;
}

Channel* Server::getChannel(const std::string& name) {
	std::string lowerName = toLowerCase(name);
	std::map<std::string, Channel*>::iterator it = _channels.find(lowerName);
	if (it != _channels.end())
		return it->second;
	return NULL;
}

Channel* Server::createChannel(const std::string& name) {
	std::string lowerName = toLowerCase(name);
	Channel* channel = new Channel(name);
	_channels[lowerName] = channel;
	return channel;
}

void Server::removeChannel(const std::string& name) {
	std::string lowerName = toLowerCase(name);
	std::map<std::string, Channel*>::iterator it = _channels.find(lowerName);
	if (it != _channels.end()) {
		delete it->second;
		_channels.erase(it);
	}
}

void Server::sendMessage(int fd, const std::string& message) {
	std::string fullMessage = message;
	if (fullMessage.size() < 2 || fullMessage.substr(fullMessage.size() - 2) != "\r\n")
		fullMessage += "\r\n";

	send(fd, fullMessage.c_str(), fullMessage.size(), 0);
}

void Server::sendToClient(Client* client, const std::string& message) {
	sendMessage(client->getFd(), message);
}

std::string Server::toLowerCase(const std::string& str) {
	std::string result = str;
	for (size_t i = 0; i < result.size(); ++i) {
		if (result[i] >= 'A' && result[i] <= 'Z')
			result[i] = result[i] - 'A' + 'a';
	}
	return result;
}

void Server::stop() {
	// This would be called for graceful shutdown
	// For now, the destructor handles cleanup
}
