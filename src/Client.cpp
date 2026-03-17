#include "Client.hpp"

Client::Client(int fd) : _fd(fd), _authenticated(false), _registered(false),
	_receivedPass(false), _receivedNick(false), _receivedUser(false) {
}

Client::~Client() {
}

// Getters
int Client::getFd() const {
	return _fd;
}

const std::string& Client::getNickname() const {
	return _nickname;
}

const std::string& Client::getUsername() const {
	return _username;
}

const std::string& Client::getRealname() const {
	return _realname;
}

const std::string& Client::getHostname() const {
	return _hostname;
}

const std::string& Client::getBuffer() const {
	return _buffer;
}

bool Client::isAuthenticated() const {
	return _authenticated;
}

bool Client::isRegistered() const {
	return _registered;
}

bool Client::hasReceivedPass() const {
	return _receivedPass;
}

bool Client::hasReceivedNick() const {
	return _receivedNick;
}

bool Client::hasReceivedUser() const {
	return _receivedUser;
}

// Setters
void Client::setNickname(const std::string& nickname) {
	_nickname = nickname;
}

void Client::setUsername(const std::string& username) {
	_username = username;
}

void Client::setRealname(const std::string& realname) {
	_realname = realname;
}

void Client::setHostname(const std::string& hostname) {
	_hostname = hostname;
}

void Client::setAuthenticated(bool auth) {
	_authenticated = auth;
}

void Client::setReceivedPass(bool received) {
	_receivedPass = received;
}

void Client::setReceivedNick(bool received) {
	_receivedNick = received;
}

void Client::setReceivedUser(bool received) {
	_receivedUser = received;
}

// Buffer management
void Client::appendToBuffer(const std::string& data) {
	_buffer += data;
}

void Client::clearBuffer() {
	_buffer.clear();
}

// Registration check
void Client::checkRegistration() {
	if (_authenticated && _receivedNick && _receivedUser && !_registered) {
		_registered = true;
	}
}

// Get prefix for messages
std::string Client::getPrefix() const {
	if (_nickname.empty())
		return "";
	std::string prefix = _nickname;
	if (!_username.empty())
		prefix += "!" + _username;
	if (!_hostname.empty())
		prefix += "@" + _hostname;
	return prefix;
}
