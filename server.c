#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define PORT 12345
#define BUF_SIZE 512
#define MAX_DOOR_HP 1000
#define MAX_PLAYER_HP 100

// ゲーム状態の定義
#define GS_WAITING 0
#define GS_COUNTDOWN 1
#define GS_SETUP 5      // 設置フェーズ
#define GS_PLAYING 2    // 戦闘フェーズ
#define GS_WIN_ATTACKER 3
#define GS_WIN_DEFENDER 4

// 時間設定
#define TIME_SETUP 20   // 設置時間 60秒
#define TIME_MATCH 60  // 試合時間 120秒

// 受信パケット
#pragma pack(push,1)
typedef struct {
    uint32_t seq;
    float x, y;
    float angle;
    uint8_t btn; 
} pkt_t;
#pragma pack(pop)

// 送信パケット
#pragma pack(push,1)
typedef struct {
    float x, y, angle;
    int active;
    int doorHP;
    int role;       
    int gameState;  
    int selfHP;     
    int enemyHP;
    int remainingTime; // ★変更: カウントダウン兼用、残り時間
} server_pkt_t;
#pragma pack(pop)

struct Client {
    struct sockaddr_in addr;
    int active;
    time_t last_seen;
    float x, y, angle;
    int role;
    int hp;
};

struct Client clients[2]; 
int client_count = 0;
int doorHP = MAX_DOOR_HP;
int attacker_id = -1;

// サーバー側の状態管理
int currentGameState = GS_WAITING;
time_t phaseStartTime = 0; // フェーズ開始時刻

void init_game() {
    doorHP = MAX_DOOR_HP;
    attacker_id = rand() % 2;
    
    for(int i=0; i<2; i++) {
        clients[i].hp = MAX_PLAYER_HP;
        if (clients[i].active) {
            clients[i].role = (i == attacker_id) ? 0 : 1;
        }
    }
    
    // カウントダウン開始
    currentGameState = GS_COUNTDOWN;
    phaseStartTime = time(NULL); 
    
    printf("Game Init! Countdown Started. Attacker ID: %d\n", attacker_id);
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

        // --- 時間経過による状態遷移 ---
        time_t now = time(NULL);
        int timeElapsed = (int)difftime(now, phaseStartTime);
        int remaining = 0;

        if (currentGameState == GS_COUNTDOWN) {
            remaining = 3 - timeElapsed;
            if (remaining < 0) {
                // カウントダウン終了 → 設置フェーズへ
                currentGameState = GS_SETUP;
                phaseStartTime = now;
                remaining = TIME_SETUP;
                printf("Phase: SETUP (%d sec)\n", TIME_SETUP);
            }
        } 
        else if (currentGameState == GS_SETUP) {
            remaining = TIME_SETUP - timeElapsed;
            if (remaining < 0) {
                // 設置終了 → 戦闘フェーズへ
                currentGameState = GS_PLAYING;
                phaseStartTime = now;
                remaining = TIME_MATCH;
                printf("Phase: PLAYING (%d sec)\n", TIME_MATCH);
            }
        }
        else if (currentGameState == GS_PLAYING) {
            remaining = TIME_MATCH - timeElapsed;
            if (remaining < 0) {
                // 時間切れ → ディフェンダー勝利（守りきった）
                currentGameState = GS_WIN_DEFENDER;
                remaining = 0;
                printf("Time Up! Defender Wins.\n");
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

            // 新規参加
            if (id == -1) {
                if (client_count < 2) {
                    for(int i=0; i<2; i++) {
                        if(!clients[i].active) {
                            id = i;
                            clients[i].addr = cliaddr;
                            clients[i].active = 1;
                            clients[i].hp = MAX_PLAYER_HP;
                            client_count++;
                            printf("Player %d joined.\n", id);
                            if (client_count == 2) init_game();
                            else clients[i].role = 0;
                            break;
                        }
                    }
                }
            }

            if (id != -1) {
                clients[id].last_seen = time(NULL);
                
                // プレイヤーの移動情報更新
                // ★TODO: セットアップ中、アタッカーは動けないようにする？（今回はクライアント側で制限）
                clients[id].x = in->x;
                clients[id].y = in->y;
                clients[id].angle = in->angle;

                int enemyId = (id == 0) ? 1 : 0;

                // 強制スタート (デバッグ用)
                if (currentGameState == GS_WAITING && (in->btn & 16)) {
                    printf("Force Start Requested by Player %d\n", id);
                    init_game();
                }

                // --- フェーズごとの処理 ---
                if (currentGameState == GS_PLAYING) {
                    // 戦闘中のみダメージ判定
                    if (clients[id].role == 0 && (in->btn & 2)) {
                        if (doorHP > 0) doorHP -= 10;
                    }
                    if (clients[enemyId].active && clients[enemyId].hp > 0) {
                        if (in->btn & 8) clients[enemyId].hp -= 40; 
                        else if (in->btn & 4) clients[enemyId].hp -= 15; 
                        if (clients[enemyId].hp < 0) clients[enemyId].hp = 0;
                    }

                    // 勝利判定: 扉破壊
                    if (doorHP <= 0) currentGameState = GS_WIN_ATTACKER;
                    // ※死亡による勝利判定はリスポーン実装時に削除または変更する必要がありますが、今は残しておきます
                }

                // 返信
                server_pkt_t out;
                out.role = clients[id].role;
                out.doorHP = doorHP;
                out.gameState = currentGameState;
                out.selfHP = clients[id].hp;
                out.remainingTime = remaining; // 残り時間を送信
                
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