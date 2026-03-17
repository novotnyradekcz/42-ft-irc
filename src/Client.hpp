#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>

class Client {
private:
	int			_fd;
	std::string	_nickname;
	std::string	_username;
	std::string	_realname;
	std::string	_hostname;
	std::string	_buffer;
	bool		_authenticated;
	bool		_registered;
	bool		_receivedPass;
	bool		_receivedNick;
	bool		_receivedUser;

public:
	Client(int fd);
	~Client();

	// Getters
	int getFd() const;
	const std::string& getNickname() const;
	const std::string& getUsername() const;
	const std::string& getRealname() const;
	const std::string& getHostname() const;
	const std::string& getBuffer() const;
	bool isAuthenticated() const;
	bool isRegistered() const;
	bool hasReceivedPass() const;
	bool hasReceivedNick() const;
	bool hasReceivedUser() const;

	// Setters
	void setNickname(const std::string& nickname);
	void setUsername(const std::string& username);
	void setRealname(const std::string& realname);
	void setHostname(const std::string& hostname);
	void setAuthenticated(bool auth);
	void setReceivedPass(bool received);
	void setReceivedNick(bool received);
	void setReceivedUser(bool received);

	// Buffer management
	void appendToBuffer(const std::string& data);
	void clearBuffer();

	// Registration check
	void checkRegistration();

	// Get prefix for messages
	std::string getPrefix() const;
};

#endif
