# コンパイラ
CC=gcc

# コンパイルオプション
# -I はSDL2のヘッダファイルがある場所を指定
# -L はSDL2のライブラリファイルがある場所を指定
CFLAGS=-I/usr/local/include/SDL2 -Wall
LDFLAGS=-L/usr/local/lib -lSDL2 -lSDL2_ttf

# ソースファイル
SRCS=main.c player.c

# オブジェクトファイル
OBJS=$(SRCS:.c=.o)

# 実行ファイル名
TARGET=game

# --- ルール ---
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)