#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define PORT 12345
#define BUF_SIZE 512
#define MAX_DOOR_HP 1000
#define MAX_PLAYER_HP 100

// ゲーム状態
#define GS_WAITING 0
#define GS_COUNTDOWN 1
#define GS_SETUP 5
#define GS_PLAYING 2
#define GS_WIN_ATTACKER 3
#define GS_WIN_DEFENDER 4

#define TIME_SETUP 60
#define TIME_MATCH 120

// 受信パケット
#pragma pack(push,1)
typedef struct {
    uint32_t seq;
    float x, y;
    float angle;
    uint8_t btn; 
} pkt_t;
#pragma pack(pop)

// 送信パケット（ブロック座標を追加）
#pragma pack(push,1)
typedef struct {
    float x, y, angle;
    int active;
    int doorHP;
    int role;       
    int gameState;  
    int selfHP;     
    int enemyHP;
    int remainingTime;
    int blockX; // ★追加: ブロックのX座標
    int blockY; // ★追加: ブロックのY座標
} server_pkt_t;
#pragma pack(pop)

struct Client {
    struct sockaddr_in addr;
    int active;
    time_t last_seen;
    float x, y, angle;
    int role;
    int hp;
    uint8_t last_btn; // ボタンの押しっぱなし判定用
};

struct Client clients[2]; 
int client_count = 0;
int doorHP = MAX_DOOR_HP;
int attacker_id = -1;

int currentGameState = GS_WAITING;
time_t phaseStartTime = 0;

// ★ブロック管理用変数
int blockX = 8;  // 初期位置X (map.cと合わせる)
int blockY = 12; // 初期位置Y
int isBlockCarried = 0; // 誰かが持っているか

void init_game() {
    doorHP = MAX_DOOR_HP;
    attacker_id = rand() % 2;
    
    // ブロック位置リセット
    blockX = 8; 
    blockY = 12;
    isBlockCarried = 0;

    for(int i=0; i<2; i++) {
        clients[i].hp = MAX_PLAYER_HP;
        clients[i].last_btn = 0;
        if (clients[i].active) {
            clients[i].role = (i == attacker_id) ? 0 : 1;
        }
    }
    
    currentGameState = GS_COUNTDOWN;
    phaseStartTime = time(NULL); 
    printf("Game Init! Attacker ID: %d\n", attacker_id);
}

int main() {
    int sock;
    struct sockaddr_in srvaddr, cliaddr;
    socklen_t clilen;
    char buf[BUF_SIZE];

    srand(time(NULL));

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        return 1;
    }

    memset(&srvaddr, 0, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET;
    srvaddr.sin_addr.s_addr = INADDR_ANY;
    srvaddr.sin_port = htons(PORT);

    if (bind(sock, (const struct sockaddr *)&srvaddr, sizeof(srvaddr)) < 0) {
        perror("bind failed");
        return 1;
    }

    printf("Server Started on port %d\n", PORT);
    
    doorHP = MAX_DOOR_HP;
    currentGameState = GS_WAITING;

    while (1) {
        clilen = sizeof(cliaddr);
        ssize_t n = recvfrom(sock, buf, BUF_SIZE, 0, (struct sockaddr *)&cliaddr, &clilen);

        time_t now = time(NULL);
        int timeElapsed = (int)difftime(now, phaseStartTime);
        int remaining = 0;

        // フェーズ遷移
        if (currentGameState == GS_COUNTDOWN) {
            remaining = 3 - timeElapsed;
            if (remaining < 0) {
                currentGameState = GS_SETUP;
                phaseStartTime = now;
                remaining = TIME_SETUP;
                printf("Phase: SETUP\n");
            }
        } else if (currentGameState == GS_SETUP) {
            remaining = TIME_SETUP - timeElapsed;
            if (remaining < 0) {
                currentGameState = GS_PLAYING;
                phaseStartTime = now;
                remaining = TIME_MATCH;
                isBlockCarried = 0; // 時間切れで強制設置
                printf("Phase: PLAYING\n");
            }
        } else if (currentGameState == GS_PLAYING) {
            remaining = TIME_MATCH - timeElapsed;
            if (remaining < 0) {
                currentGameState = GS_WIN_DEFENDER;
                remaining = 0;
            }
        }

        if (n > 0) {
            pkt_t *in = (pkt_t*)buf;
            int id = -1;
            for (int i = 0; i < 2; i++) {
                if (clients[i].active && 
                    clients[i].addr.sin_addr.s_addr == cliaddr.sin_addr.s_addr &&
                    clients[i].addr.sin_port == cliaddr.sin_port) {
                    id = i;
                    break;
                }
            }

            if (id == -1) {
                if (client_count < 2) {
                    for(int i=0; i<2; i++) {
                        if(!clients[i].active) {
                            id = i;
                            clients[i].addr = cliaddr;
                            clients[i].active = 1;
                            clients[i].hp = MAX_PLAYER_HP;
                            client_count++;
                            if (client_count == 2) init_game();
                            else clients[i].role = 0;
                            break;
                        }
                    }
                }
            }

            if (id != -1) {
                clients[id].last_seen = time(NULL);
                clients[id].x = in->x;
                clients[id].y = in->y;
                clients[id].angle = in->angle;

                int enemyId = (id == 0) ? 1 : 0;

                // 強制スタート
                if (currentGameState == GS_WAITING && (in->btn & 16)) init_game();

                // ★ブロック持ち運び処理 (セットアップ中 & ディフェンダーのみ)
                if (currentGameState == GS_SETUP && clients[id].role == 1) {
                    // Fキー (bit 5 = 32) が押された瞬間だけ処理
                    if ((in->btn & 32) && !(clients[id].last_btn & 32)) {
                        if (isBlockCarried) {
                            // 既に持っているなら設置
                            isBlockCarried = 0;
                            printf("Block Placed at (%d, %d)\n", blockX, blockY);
                        } else {
                            // 持っていないなら、近くにあるか確認して持ち上げ
                            float dx = clients[id].x - blockX;
                            float dy = clients[id].y - blockY;
                            if (sqrt(dx*dx + dy*dy) < 2.0) {
                                isBlockCarried = 1;
                                printf("Block Picked Up by Defender\n");
                            }
                        }
                    }
                    
                    // 持っている間はプレイヤーの目の前に移動させる
                    if (isBlockCarried) {
                        blockX = (int)(clients[id].x + cos(clients[id].angle) * 2.0);
                        blockY = (int)(clients[id].y + sin(clients[id].angle) * 2.0);
                        // マップ範囲外チェック（簡易）
                        if(blockX < 1) blockX = 1; if(blockX > 22) blockX = 22;
                        if(blockY < 1) blockY = 1; if(blockY > 22) blockY = 22;
                    }
                }

                if (currentGameState == GS_PLAYING) {
                    // 扉への攻撃チェック (ブロック座標 blockX, blockY を使う必要はない。
                    // クライアント側でID:9を判定して送ってくるため、ここでは単純にダメージ処理)
                    if (clients[id].role == 0 && (in->btn & 2)) {
                        if (doorHP > 0) doorHP -= 10;
                    }
                    // 敵への攻撃
                    if (clients[enemyId].active && clients[enemyId].hp > 0) {
                        if (in->btn & 8) clients[enemyId].hp -= 40; 
                        else if (in->btn & 4) clients[enemyId].hp -= 15; 
                        if (clients[enemyId].hp < 0) clients[enemyId].hp = 0;
                    }
                    if (doorHP <= 0) currentGameState = GS_WIN_ATTACKER;
                }

                clients[id].last_btn = in->btn; // ボタン状態保存

                server_pkt_t out;
                out.role = clients[id].role;
                out.doorHP = doorHP;
                out.gameState = currentGameState;
                out.selfHP = clients[id].hp;
                out.remainingTime = remaining;
                out.blockX = blockX; // ★送信
                out.blockY = blockY; // ★送信
                
                if (clients[enemyId].active) {
                    out.x = clients[enemyId].x;
                    out.y = clients[enemyId].y;
                    out.angle = clients[enemyId].angle;
                    out.active = 1;
                    out.enemyHP = clients[enemyId].hp;
                } else {
                    out.active = 0;
                    out.enemyHP = 0;
                }
                sendto(sock, &out, sizeof(out), 0, (struct sockaddr *)&cliaddr, clilen);
            }
        }
    }
    close(sock);
    return 0;
}