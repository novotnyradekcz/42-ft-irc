*This project has been created as part of the 42 curriculum by aevstign, rnovotny.*

# ft_irc - Internet Relay Chat Server

## Description

ft_irc is a fully functional IRC (Internet Relay Chat) server implementation in C++98. The server provides real-time text-based communication supporting multiple simultaneous clients, channels, and private messaging. It complies with the IRC protocol specifications and has been tested with irssi as the reference IRC client.

The project demonstrates advanced network programming concepts including non-blocking I/O, socket programming, and protocol implementation. Key features include:
- Multi-client support using poll() for efficient I/O multiplexing
- User authentication with password protection
- Channel creation and management
- Private messaging between users
- Channel operators with special privileges
- Multiple channel modes (invite-only, topic restrictions, user limits, etc.)

## Reference IRC Client

**irssi** ([https://irssi.org](https://irssi.org)) is the official reference client used for testing this IRC server implementation. The server is designed to be fully compatible with irssi and follows the IRC protocol specifications (RFC 1459 and RFC 2812) to ensure proper interoperability.

### Why irssi?
- **Widely used**: The most popular terminal-based IRC client
- **Standards-compliant**: Implements IRC protocol correctly and predictably
- **Terminal-based**: No GUI required, works on any Unix machine
- **Lightweight**: Fast and scriptable with no external dependencies
- **Commonly used at 42**: Familiar to peers and evaluators

### Installation
```bash
# macOS
brew install irssi

# Ubuntu/Debian
sudo apt install irssi
```

## Instructions

### Compilation

To compile the project, run:
```bash
make
```

This will create the `ircserv` executable in the project root directory.

### Execution

Start the IRC server with:
```bash
./ircserv <port> <password>
```

Parameters:
- `<port>`: Port number for the server to listen on (1024-65535)
- `<password>`: Connection password required by clients

Example:
```bash
./ircserv 6667 mypassword
```

### Connecting with IRC Client

#### Using irssi (Reference Client):

**Basic Connection:**
```bash
irssi
/connect localhost 6667 mypassword
/nick YourNickname
/join #channel
```

**Useful irssi Commands:**
- `/join #channel` - Join a channel
- `/msg #channel Hello!` - Send message to channel
- `/msg nickname Hi` - Send private message
- `/part #channel` - Leave channel
- `/nick NewNick` - Change nickname
- `/topic #channel New topic` - Set channel topic (operators only)
- `/invite nickname #channel` - Invite user to channel
- `/kick #channel user reason` - Kick user (operators only)
- `/mode #channel +i` - Set channel mode (operators only)
- `/quit` - Disconnect from server

**Channel Operator Commands:**
- `/mode #channel +i` - Make channel invite-only
- `/mode #channel +k password` - Set channel key
- `/mode #channel +o nickname` - Give operator status
- `/mode #channel +l 10` - Set user limit
- `/mode #channel +t` - Restrict topic to operators

#### Using nc (netcat) for protocol testing:
```bash
nc localhost 6667
PASS mypassword
NICK testnick
USER testuser 0 * :Test User
JOIN #test
PRIVMSG #test :Hello, World!
```

### Cleaning

- `make clean`: Remove object files
- `make fclean`: Remove object files and executable
- `make re`: Rebuild everything from scratch

## Supported IRC Commands

### Connection Commands
- **PASS** `<password>` - Authenticate with the server
- **NICK** `<nickname>` - Set or change your nickname
- **USER** `<username> <hostname> <servername> <realname>` - Set user information
- **QUIT** `[<message>]` - Disconnect from the server

### Channel Commands
- **JOIN** `<channel> [<key>]` - Join a channel (creates if doesn't exist)
- **PART** `<channel> [<message>]` - Leave a channel
- **TOPIC** `<channel> [<topic>]` - View or set channel topic
- **KICK** `<channel> <user> [<reason>]` - Remove user from channel (operators only)
- **INVITE** `<nickname> <channel>` - Invite user to channel
- **MODE** `<channel> <modes> [<parameters>]` - Change channel modes (operators only)

### Messaging Commands
- **PRIVMSG** `<target> <message>` - Send message to user or channel

### Server Commands
- **PING** `<token>` - Ping the server (receives PONG response)

## Channel Modes

Operators can set the following channel modes:

- **i** (invite-only): Users must be invited to join
- **t** (topic restricted): Only operators can change the topic
- **k** (key): Set a password required to join the channel
- **o** (operator): Give/remove operator privileges to a user
- **l** (limit): Set maximum number of users allowed in the channel

### Mode Examples:
```
MODE #channel +i          # Set invite-only
MODE #channel -i          # Remove invite-only
MODE #channel +k pass123  # Set channel key
MODE #channel +o alice    # Give operator status to alice
MODE #channel -o bob      # Remove operator status from bob
MODE #channel +l 10       # Set user limit to 10
MODE #channel -l          # Remove user limit
```

## Architecture

The project consists of several key components:

### Core Classes
- **Server**: Main server class managing connections, clients, and channels
- **Client**: Represents a connected IRC client with authentication state
- **Channel**: Manages individual chat channels with members and operators

### File Structure
```
ft_irc/
├── Makefile
├── README.md
├── src/
│   ├── main.cpp              # Entry point and signal handling
│   ├── Server.hpp/cpp        # Server core functionality
│   ├── Client.hpp/cpp        # Client representation
│   ├── Channel.hpp/cpp       # Channel management
│   ├── MessageHandler.cpp    # Message parsing and routing
│   ├── Commands.cpp          # Basic IRC commands
│   ├── ChannelCommands.cpp   # Channel-related commands
│   └── ModeCommand.cpp       # MODE command implementation
├── obj/                      # Compiled object files
```

## irssi Compatibility

This server has been designed to be fully compatible with irssi. The following features are implemented to ensure smooth operation:

### Supported Features
- ✅ **CAP negotiation**: Handles CAP LS, CAP REQ, CAP END (sent by irssi on connect)
- ✅ **Proper numeric replies**: All numeric codes are 3-digit padded (001, 002, etc.) as per IRC spec
- ✅ **Welcome sequence**: Complete RPL_WELCOME through RPL_MYINFO messages (001-004)
- ✅ **PING/PONG**: Keep-alive mechanism for connection stability
- ✅ **Message buffering**: Handles partial messages and aggregates packets correctly
- ✅ **Nickname validation**: Follows IRC nickname rules (max 9 chars, starts with letter)
- ✅ **Channel prefixes**: Supports # and & channel types
- ✅ **Operator privileges**: First user in channel automatically becomes operator
- ✅ **NAMES list**: Sends proper channel member list (RPL_NAMREPLY 353, RPL_ENDOFNAMES 366)
- ✅ **PRIVMSG routing**: Messages sent to selected users or channels

### Registration Sequence
The server requires authentication in the following order (as per ft_irc subject):
1. **PASS** `<password>` - Must be sent first
2. **NICK** `<nickname>` - Can be sent before or after USER
3. **USER** `<username> 0 * <realname>` - Completes registration

irssi automatically sends these during connection. Once all three are processed, the server sends welcome messages (001-004).

### Tested Versions
- irssi 1.2.x and later
- Works with default irssi configuration

### Known Compatibility Notes
- The server implements the core IRC commands required by the ft_irc subject
- Advanced IRC features (WHOIS, WHO, LIST, etc.) are not required and not implemented
- SSL/TLS is not required by the subject and not supported
- SASL authentication is not required and not supported

## Testing

### Testing with irssi (Reference Client)

#### 1. Basic Connection Test
```bash
# Terminal 1: start the server
./ircserv 6667 password

# Terminal 2: connect with irssi
irssi
/connect localhost 6667 password
/nick alice
```
Expected result: welcome messages (001-004) confirm successful registration.

#### 2. Multi-Client Channel Test
```bash
# Terminal 2 (alice):
/join #test
# type: Hello from Alice!

# Terminal 3 (bob, another irssi instance):
/connect localhost 6667 password
/nick bob
/join #test
# type: Hello from Bob!
```
Expected result: each user sees the other's messages; their own messages appear locally without echo from the server.

#### 3. Channel Operator Commands Test
As the first user in a channel (automatically an operator):
```
/join #operators
/topic #operators This is our topic
/mode #operators +i
/invite bob #operators
/mode #operators +o bob
/mode #operators +k secretpass
/kick #operators bob Testing kick
```

#### 4. Private Messaging Test
```
/msg bob Hello Bob!
```
Expected result: bob receives the message; alice sees it locally via irssi's own echo.

#### 5. Mode Testing
```
/join #modetest
/mode #modetest +i
/mode #modetest +t
/mode #modetest +k pass123
/mode #modetest +l 5
/mode #modetest +o bob
/mode #modetest -i
/mode #modetest -k
```

### Basic Protocol Testing with nc

#### Basic Connection Test
1. Start the server: `./ircserv 6667 password`
2. Connect with client: `nc localhost 6667`
3. Authenticate:
   ```
   PASS password
   NICK testnick
   USER test 0 * :Test User
   ```
4. You should receive welcome messages (001-004)

### Partial Data Test (nc ctrl+D)
As specified in the subject, test with:
```bash
nc -C 127.0.0.1 6667
com^Dman^Dd
```
The server correctly aggregates partial packets before processing commands.

### Multi-Client Test
1. Open multiple terminal windows
2. Connect multiple clients with different nicknames
3. Have them join the same channel
4. Test that messages are broadcast to all clients in the channel

### Mode Testing
1. Create a channel (first user becomes operator)
2. Test setting various modes (+i, +t, +k, +o, +l)
3. Verify that non-operators cannot change modes
4. Test that mode restrictions work correctly

## Technical Details

- **Language**: C++98
- **Compilation flags**: `-Wall -Wextra -Werror -std=c++98`
- **I/O Model**: Non-blocking sockets with poll()
- **Protocol**: IRC (RFC 1459/2812 subset)
- **Maximum message size**: 512 bytes (as per IRC spec)

## Known Limitations

- Only TCP/IP v4 is supported (not v6)
- Server-to-server communication is not implemented
- Some advanced IRC features (WHOIS, WHO, LIST, etc.) are not implemented
- DCC (Direct Client-to-Client) is not supported
- No SSL/TLS encryption

## Resources

### Classic References
- [RFC 1459](https://tools.ietf.org/html/rfc1459) - Internet Relay Chat Protocol
- [RFC 2812](https://tools.ietf.org/html/rfc2812) - IRC Client Protocol
- [Modern IRC Documentation](https://modern.ircdocs.horse/) - Updated IRC specifications
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) - Socket programming tutorial
- [poll() man page](https://man7.org/linux/man-pages/man2/poll.2.html) - I/O multiplexing documentation

### AI Usage
AI tools (GitHub Copilot, Claude) were used in this project for:
- **Code generation**: Initial boilerplate for class structures and function signatures
- **Documentation**: Assistance with README formatting and IRC protocol documentation
- **Debugging assistance**: Identifying edge cases in message parsing
- **Refactoring suggestions**: Code organization and C++98 compliance checks

All AI-generated content was thoroughly reviewed, tested, and modified to ensure correctness, compliance with project requirements, and full understanding of implementation details.

## License

This is an educational project created as part of the 42 school curriculum.
