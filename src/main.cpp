#include "Server.hpp"
#include <iostream>
#include <cstdlib>
#include <csignal>

Server* g_server = NULL;

void signalHandler(int signal) {
	(void)signal;
	std::cout << "\nShutting down server..." << std::endl;
	g_server = NULL;
	exit(0);
}

int main(int argc, char** argv) {
	if (argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
		return 1;
	}

	int port = std::atoi(argv[1]);
	if (port <= 0 || port > 65535) {
		std::cerr << "Error: Invalid port number" << std::endl;
		return 1;
	}

	std::string password = argv[2];
	if (password.empty()) {
		std::cerr << "Error: Password cannot be empty" << std::endl;
		return 1;
	}

	// Set up signal handlers
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);

	try {
		Server server(port, password);
		g_server = &server;
		server.run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	// run() returned cleanly: ~Server() will now be called, freeing all resources
	std::cout << "Server shut down gracefully." << std::endl;
	return 0;
}
