# コンパイラ
CC = gcc

# コンパイルオプション
CFLAGS = -I/usr/local/include/SDL2 -Wall -g
LDFLAGS = -L/usr/local/lib -lSDL2 -lSDL2_ttf -lm

# --- 新しい3Dクライアント ---
CLIENT3D_SRCS = client_3d.c player.c map.c
CLIENT3D_OBJS = $(CLIENT3D_SRCS:.c=.o)
CLIENT3D_TARGET = client3d

# --- サーバー ---
SERVER_SRCS = server.c
SERVER_OBJS = $(SERVER_SRCS:.c=.o)
SERVER_TARGET = server

# --- ルール ---
all: $(CLIENT3D_TARGET) $(SERVER_TARGET)

$(CLIENT3D_TARGET): $(CLIENT3D_OBJS)
	$(CC) $(CLIENT3D_OBJS) -o $(CLIENT3D_TARGET) $(LDFLAGS)

$(SERVER_TARGET): $(SERVER_OBJS)
	$(CC) $(SERVER_OBJS) -o $(SERVER_TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(CLIENT3D_OBJS) $(SERVER_OBJS) $(CLIENT3D_TARGET) $(SERVER_TARGET)