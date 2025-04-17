##
## EPITECH PROJECT, 2024
## Makefile
## File description:
## Makefile for jetpack
##

CC = g++
CFLAGS = -Wall -Wextra -std=c++17 -Ishared_include
LDFLAGS = -lpthread

SERVER_DIR = server_src
CLIENT_DIR = client_src
SHARED_DIR = shared_include

# SFML flags pour le client
CLIENT_LDFLAGS = $(LDFLAGS) -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio

SERVER_SRC = $(SERVER_DIR)/server.cpp \
             $(SERVER_DIR)/main.cpp \
             $(SERVER_DIR)/config.cpp \
             $(SERVER_DIR)/packet.cpp \
             $(SERVER_DIR)/sendAssets.cpp \
             $(SERVER_DIR)/Game.cpp

CLIENT_SRC = $(CLIENT_DIR)/client.cpp \
             $(CLIENT_DIR)/main.cpp \
             $(CLIENT_DIR)/receiveAssets.cpp \
             $(CLIENT_DIR)/Graphics.cpp \
             $(SERVER_DIR)/packet.cpp

SERVER_OBJ = $(SERVER_SRC:.cpp=.o)
CLIENT_OBJ = $(CLIENT_SRC:.cpp=.o)

SERVER_NAME = jetpack_server
CLIENT_NAME = jetpack_client

all: server client

server: $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $(SERVER_NAME) $(SERVER_OBJ) $(LDFLAGS)

client: $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o $(CLIENT_NAME) $(CLIENT_OBJ) $(CLIENT_LDFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SERVER_OBJ) $(CLIENT_OBJ)

fclean: clean
	rm -f $(SERVER_NAME) $(CLIENT_NAME)

re: fclean all

.PHONY: all clean fclean re server client