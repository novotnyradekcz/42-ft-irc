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

// Bit check:
// revents: 0101
// event:   0001
// &:       0001 -> event is present
static bool hasPollEvent(short revents, short event) {
	return (revents & event) != 0;
}

// Error flags are independent bits:
// revents: 1010
// POLLERR: 0010 -> present
// POLLHUP: 1000 -> present
// POLLNVAL: checked the same way
static bool hasPollError(short revents) {
	return hasPollEvent(revents, POLLERR)
		|| hasPollEvent(revents, POLLHUP)
		|| hasPollEvent(revents, POLLNVAL);
}

// Event selection:
// POLLIN:  0001
// POLLOUT: 0100
// |:       0101 -> watch read and write
static short clientPollEvents(bool writable) {
	if (writable)
		return POLLIN | POLLOUT;
	return POLLIN;
}

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
	while (g_running) {
		int ret = poll(&_pollFds[0], _pollFds.size(), -1);
		if (ret < 0) {
			if (errno == EINTR)
				continue; // re-check g_running at top of loop
			throw std::runtime_error("Poll failed");
		}

		// Check all file descriptors
		for (size_t i = 0; i < _pollFds.size();) {
			int fd = _pollFds[i].fd;
			short revents = _pollFds[i].revents;

			if (hasPollError(revents)) {
				if (fd != _serverSocket)
					removeClient(fd);
				continue;
			}
			if (hasPollEvent(revents, POLLIN)) {
				if (fd == _serverSocket) {
					acceptNewClient();
				} else {
					handleClientData(fd);
				}
			}
			if (fd != _serverSocket && getClientByFd(fd) && hasPollEvent(revents, POLLOUT))
				flushClientOutput(fd);
			if (fd != _serverSocket && !getClientByFd(fd))
				continue;
			++i;
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
	Client* client = getClientByFd(fd);
	if (!client)
		return;

	std::string fullMessage = message;
	if (fullMessage.size() < 2 || fullMessage.substr(fullMessage.size() - 2) != "\r\n")
		fullMessage += "\r\n";

	client->appendToOutputBuffer(fullMessage);
	setClientWritable(fd, true);
}

void Server::sendToClient(Client* client, const std::string& message) {
	if (!client)
		return;
	sendMessage(client->getFd(), message);
}

void Server::flushClientOutput(int fd) {
	Client* client = getClientByFd(fd);
	if (!client)
		return;

	const std::string& buffer = client->getOutputBuffer();
	if (buffer.empty()) {
		setClientWritable(fd, false);
		return;
	}

	ssize_t sent = send(fd, buffer.c_str(), buffer.size(), 0);
	if (sent > 0) {
		client->eraseOutputBuffer(static_cast<size_t>(sent));
		if (!client->hasPendingOutput())
			setClientWritable(fd, false);
		return;
	}
	if (sent < 0 && (errno == EWOULDBLOCK || errno == EAGAIN))
		return;
	removeClient(fd);
}

void Server::setClientWritable(int fd, bool enabled) {
	for (std::vector<struct pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); ++it) {
		if (it->fd == fd) {
			it->events = clientPollEvents(enabled);
			return;
		}
	}
}

std::string Server::toLowerCase(const std::string& str) {
	std::string result = str;
	for (size_t i = 0; i < result.size(); ++i) {
		if (result[i] >= 'A' && result[i] <= 'Z')
			result[i] = result[i] - 'A' + 'a';
	}
	return result;
}
