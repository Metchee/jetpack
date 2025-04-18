##
## EPITECH PROJECT, 2024
## Makefile
## File description:
## Makefile for jetpack multiplayer game
##

CC = g++
CFLAGS = -Wall -Wextra -std=c++17 -Ishared_include
LDFLAGS = -lpthread

# Server
SERVER_DIR = server_src
SHARED_DIR = shared_include
SERVER_SRC = 	$(SERVER_DIR)/server.cpp \
				$(SERVER_DIR)/main.cpp \
				$(SERVER_DIR)/config.cpp \
				$(SERVER_DIR)/packet.cpp \
				$(SERVER_DIR)/sendAssets.cpp \
				$(SERVER_DIR)/map.cpp \
				$(SERVER_DIR)/game.cpp

SERVER_OBJ = $(SERVER_SRC:.cpp=.o)
SERVER_NAME = jetpack_server
SERVER_LDFLAGS = $(LDFLAGS)

# Client
CLIENT_DIR = client_src
CLIENT_SRC = 	$(CLIENT_DIR)/client.cpp \
				$(CLIENT_DIR)/main.cpp \
				$(CLIENT_DIR)/receiveAssets.cpp \
				$(CLIENT_DIR)/clientGraphics.cpp \
				$(SERVER_DIR)/packet.cpp \
				$(SERVER_DIR)/map.cpp \
				$(SERVER_DIR)/game.cpp

CLIENT_OBJ = $(CLIENT_SRC:.cpp=.o)
CLIENT_NAME = jetpack_client
CLIENT_LDFLAGS = $(LDFLAGS) -lsfml-graphics -lsfml-window -lsfml-system

# All
all: assets server client

# Server
server: $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $(SERVER_NAME) $(SERVER_OBJ) $(SERVER_LDFLAGS)

# Client
client: $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o $(CLIENT_NAME) $(CLIENT_OBJ) $(CLIENT_LDFLAGS)

# Object files
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Create assets directory if it doesn't exist
assets:
	mkdir -p assets

# Clean
clean:
	rm -f $(SERVER_OBJ) $(CLIENT_OBJ)

# Full clean
fclean: clean
	rm -f $(SERVER_NAME) $(CLIENT_NAME)

# Rebuild
re: fclean all

.PHONY: all clean fclean re server client assets