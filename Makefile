# コンパイラ
CC = gcc

# コンパイルオプション
CFLAGS = -I/usr/local/include/SDL2 -Wall -g
LDFLAGS = -L/usr/local/lib -lSDL2 -lSDL2_ttf

# --- クライアント (game) ---
CLIENT_SRCS = main.c player.c
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)
CLIENT_TARGET = game

# --- サーバー (server) ---
SERVER_SRCS = server.c
SERVER_OBJS = $(SERVER_SRCS:.c=.o)
SERVER_TARGET = server

# --- ルール ---
all: $(CLIENT_TARGET) $(SERVER_TARGET)

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CC) $(CLIENT_OBJS) -o $(CLIENT_TARGET) $(LDFLAGS) -lm

$(SERVER_TARGET): $(SERVER_OBJS)
	$(CC) $(SERVER_OBJS) -o $(SERVER_TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(CLIENT_OBJS) $(SERVER_OBJS) $(CLIENT_TARGET) $(SERVER_TARGET)