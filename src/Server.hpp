#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include <vector>
#include <poll.h>
#include <csignal>
#include "Client.hpp"
#include "Channel.hpp"

extern volatile sig_atomic_t g_running;

class Server {
private:
	int							_port;
	std::string					_password;
	int							_serverSocket;
	std::vector<struct pollfd>	_pollFds;
	std::map<int, Client*>		_clients;
	std::map<std::string, Channel*>	_channels;

	// Server initialization
	void setupServer();
	void bindSocket();
	void listenSocket();

	// Client management
	void acceptNewClient();
	void removeClient(int fd);
	Client* getClientByFd(int fd);
	Client* getClientByNickname(const std::string& nickname);

	// Channel management
	Channel* getChannel(const std::string& name);
	Channel* createChannel(const std::string& name);
	void removeChannel(const std::string& name);

	// Message handling
	void handleClientData(int fd);
	void processMessage(Client* client, const std::string& message);
	std::vector<std::string> parseMessage(const std::string& message);
	void sendMessage(int fd, const std::string& message);
	void sendToClient(Client* client, const std::string& message);

	// IRC Commands
	void handlePass(Client* client, const std::vector<std::string>& params);
	void handleNick(Client* client, const std::vector<std::string>& params);
	void handleUser(Client* client, const std::vector<std::string>& params);
	void handleJoin(Client* client, const std::vector<std::string>& params);
	void handlePart(Client* client, const std::vector<std::string>& params);
	void handlePrivmsg(Client* client, const std::vector<std::string>& params);
	void handleKick(Client* client, const std::vector<std::string>& params);
	void handleInvite(Client* client, const std::vector<std::string>& params);
	void handleTopic(Client* client, const std::vector<std::string>& params);
	void handleMode(Client* client, const std::vector<std::string>& params);
	void handleQuit(Client* client, const std::vector<std::string>& params);
	void handlePing(Client* client, const std::vector<std::string>& params);
	void handleWho(Client* client, const std::vector<std::string>& params);

	// Helper functions
	void sendWelcome(Client* client);
	void sendNumericReply(Client* client, int code, const std::string& message);
	bool isValidNickname(const std::string& nickname);
	bool isValidChannelName(const std::string& name);
	std::string toLowerCase(const std::string& str);

public:
	Server(int port, const std::string& password);
	~Server();

	void run();
};

#endif
