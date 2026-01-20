#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <math.h> 

// ★ここが重要：skill.cの関数を使うためにインクルード
#include "skill.h" 

#define PORT 12345
#define BUF_SIZE 512
#define MAX_DOOR_HP 1000
#define MAX_PLAYER_HP 100

#define RESPAWN_TIME_ATTACKER 10
#define RESPAWN_TIME_DEFENDER 5

#define TIME_SETUP 60
#define TIME_MATCH 120

#define GS_WAITING 0
#define GS_COUNTDOWN 1
#define GS_SETUP 5
#define GS_PLAYING 2
#define GS_WIN_ATTACKER 3
#define GS_WIN_DEFENDER 4

#define MAP_WIDTH 24
#define MAP_HEIGHT 24

// マップデータ（サーバー側）
// 0=床, 1=外壁, 2=柱(青), 3=柱(赤), 4=壁(木), 9=破壊目標
int worldMap[MAP_WIDTH][MAP_HEIGHT] = {
    // 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, // 0
    {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // 1 (Attacker Spawn = 1,1)
    {1, 0, 0, 0, 2, 0, 2, 0, 1, 0, 0, 0, 3, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // 2
    {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // 3 (左上の遮蔽物)
    {1, 0, 0, 0, 2, 0, 2, 0, 0, 0, 0, 0, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // 4
    {1, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 3, 0, 0, 0, 4, 4, 4, 2, 2, 1}, // 5
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 3, 0, 0, 0, 4, 0, 4, 0, 0, 1}, // 6 (中央ルートの激戦区)
    {1, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 4, 0, 0, 1}, // 7
    {1, 0, 0, 4, 0, 4, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // 8 (★9=初期ブロック位置付近は空ける)
    {1, 0, 0, 4, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // 9
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // 10
    {1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // 11 (左側を壁で分断)
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 1}, // 12
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 1}, // 13
    {1, 0, 0, 0, 0, 0, 0, 0, 5, 5, 5, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 1}, // 14
    {1, 0, 0, 3, 0, 0, 0, 0, 5, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // 15
    {1, 0, 0, 3, 0, 0, 0, 0, 5, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // 16
    {1, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1}, // 17 (右側を壁で分断)
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // 18
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // 19
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 2, 2, 0, 0, 0, 1}, // 20 (Defender Spawn = 20,20)
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 1}, // 21
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // 22
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}  // 23
};

#pragma pack(push,1)
typedef struct {
    uint32_t seq;
    float x, y;
    float angle;
    uint8_t btn; 
} pkt_t;
#pragma pack(pop)

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
    int blockX;
    int blockY;
    int respawnTime; 
    int skill1_remain; // スキル1の残り待ち時間
} server_pkt_t;
#pragma pack(pop)

struct Client {
    struct sockaddr_in addr;
    int active;
    time_t last_seen;
    float x, y, angle;
    int role;
    int hp;
    uint8_t last_btn; 
    time_t deadTime;
    time_t skill1_usedTime; // スキル使用時刻
};

struct Client clients[2]; 
int client_count = 0;
int doorHP = MAX_DOOR_HP;
int attacker_id = -1;

int currentGameState = GS_WAITING;
time_t phaseStartTime = 0;

int blockX = 8; 
int blockY = 12; 
int isBlockCarried = 0; 

void init_game() {
    doorHP = MAX_DOOR_HP;
    attacker_id = rand() % 2;
    blockX = 8; blockY = 12; isBlockCarried = 0;

    for(int i=0; i<2; i++) {
        clients[i].hp = MAX_PLAYER_HP;
        clients[i].deadTime = 0; 
        clients[i].skill1_usedTime = 0;
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
                isBlockCarried = 0; 
                printf("Phase: PLAYING\n");
            }
        } else if (currentGameState == GS_PLAYING) {
            remaining = TIME_MATCH - timeElapsed;
            if (remaining < 0) {
                currentGameState = GS_WIN_DEFENDER;
                remaining = 0;
            }
        }

        // リスポーン管理
        for(int i=0; i<2; i++) {
            if(clients[i].active && clients[i].deadTime > 0) {
                int interval = (clients[i].role == 0) ? RESPAWN_TIME_ATTACKER : RESPAWN_TIME_DEFENDER;
                if(difftime(now, clients[i].deadTime) >= interval) {
                    clients[i].hp = MAX_PLAYER_HP;
                    clients[i].deadTime = 0;
                    clients[i].skill1_usedTime = 0;
                    printf("Player %d Respawned!\n", i);
                }
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
                            clients[i].deadTime = 0;
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
                
                if (clients[id].deadTime == 0) {
                    clients[id].x = in->x;
                    clients[id].y = in->y;
                    clients[id].angle = in->angle;
                }

                int enemyId = (id == 0) ? 1 : 0;

                if (currentGameState == GS_WAITING && (in->btn & 16)) init_game();

                if (currentGameState == GS_SETUP && clients[id].role == 1 && clients[id].hp > 0) {
                    if ((in->btn & 32) && !(clients[id].last_btn & 32)) {
                        if (isBlockCarried) {
                            isBlockCarried = 0;
                        } else {
                            float dx = clients[id].x - blockX;
                            float dy = clients[id].y - blockY;
                            if (sqrt(dx*dx + dy*dy) < 2.0) isBlockCarried = 1;
                        }
                    }
                    if (isBlockCarried) {
                        int tx = (int)(clients[id].x + cos(clients[id].angle) * 2.0);
                        int ty = (int)(clients[id].y + sin(clients[id].angle) * 2.0);
                        if(tx < 1) tx = 1; if(tx > 22) tx = 22;
                        if(ty < 1) ty = 1; if(ty > 22) ty = 22;
                        if (worldMap[tx][ty] == 0) { blockX = tx; blockY = ty; }
                    }
                }

                if (currentGameState == GS_PLAYING) {
                    if (clients[id].hp > 0) {
                        
                        // ★ここが修正ポイント：skill.cの共通関数を使ってHP計算
                        if ((in->btn & 64) && !(clients[id].last_btn & 64)) {
                            // クールタイム（skill.hで定義した定数を使用）
                            int ct = (clients[id].role == 0) ? SKILL_HEAL_CT : SKILL_REPAIR_CT;
                            int passed = (int)difftime(now, clients[id].skill1_usedTime);
                            
                            if (clients[id].skill1_usedTime == 0 || passed >= ct) {
                                clients[id].skill1_usedTime = now;
                                
                                if (clients[id].role == 0) {
                                    // アタッカー: 自己治療 (汎用関数)
                                    skill_logic_heal_generic(&clients[id].hp, MAX_PLAYER_HP, 40);
                                    printf("Attacker P%d used Skill: Heal\n", id);
                                } else {
                                    // ディフェンダー: ドア補強 (汎用関数)
                                    skill_logic_repair_generic(&doorHP, MAX_DOOR_HP);
                                    printf("Defender P%d used Skill: Repair\n", id);
                                }
                            }
                        }

                        // 通常攻撃
                        if (clients[id].role == 0 && (in->btn & 2)) {
                            if (doorHP > 0) doorHP -= 10;
                        }
                        if (clients[enemyId].active && clients[enemyId].hp > 0) {
                            int damage = 0;
                            if (in->btn & 8) damage = 40; 
                            else if (in->btn & 4) damage = 15; 
                            
                            if (damage > 0) {
                                clients[enemyId].hp -= damage;
                                if (clients[enemyId].hp <= 0) {
                                    clients[enemyId].hp = 0;
                                    clients[enemyId].deadTime = time(NULL); 
                                    printf("Player %d Killed!\n", enemyId);
                                }
                            }
                        }
                    }
                    if (doorHP <= 0) currentGameState = GS_WIN_ATTACKER;
                }

                clients[id].last_btn = in->btn;

                server_pkt_t out;
                out.role = clients[id].role;
                out.doorHP = doorHP;
                out.gameState = currentGameState;
                out.selfHP = clients[id].hp;
                out.remainingTime = remaining;
                out.blockX = blockX;
                out.blockY = blockY;
                
                out.respawnTime = 0;
                if (clients[id].deadTime > 0) {
                    int interval = (clients[id].role == 0) ? RESPAWN_TIME_ATTACKER : RESPAWN_TIME_DEFENDER;
                    int passed = (int)difftime(now, clients[id].deadTime);
                    out.respawnTime = interval - passed;
                    if (out.respawnTime < 0) out.respawnTime = 0;
                }

                out.skill1_remain = 0;
                if (clients[id].skill1_usedTime > 0) {
                    int ct = (clients[id].role == 0) ? SKILL_HEAL_CT : SKILL_REPAIR_CT;
                    int passed = (int)difftime(now, clients[id].skill1_usedTime);
                    out.skill1_remain = ct - passed;
                    if (out.skill1_remain < 0) out.skill1_remain = 0;
                }

                if (clients[enemyId].active) {
                    out.x = clients[enemyId].x;
                    out.y = clients[enemyId].y;
                    out.angle = clients[enemyId].angle;
                    if (clients[enemyId].deadTime > 0) out.active = 0; 
                    else out.active = 1;
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