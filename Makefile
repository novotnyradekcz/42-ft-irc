# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: rnovotny <rnovotny@student.42prague.com    +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/05/11 16:53:34 by rnovotny          #+#    #+#              #
#    Updated: 2026/03/17 14:37:38 by rnovotny         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME = ircserv
CC = c++
CPPFLAGS = -Wall -Wextra -Werror -std=c++98 -I./src
RM = rm -f

# Source files
SRC_DIR = src
OBJ_DIR = obj

SRCS = main.cpp \
		Client.cpp \
		Channel.cpp \
		Server.cpp \
		MessageHandler.cpp \
		Commands.cpp \
		ChannelCommands.cpp \
		ModeCommand.cpp

OBJS = $(addprefix $(OBJ_DIR)/, $(SRCS:.cpp=.o))

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CPPFLAGS) -o $@ $^
	@echo "✓ ircserv compiled successfully"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CPPFLAGS) -c $< -o $@

clean:
	$(RM) -r $(OBJ_DIR)
	@echo "✓ Object files removed"

fclean: clean
	$(RM) $(NAME)
	@echo "✓ Executable removed"

re: fclean all

.PHONY: all clean fclean re