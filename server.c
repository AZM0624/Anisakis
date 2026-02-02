#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <math.h> 
#include "skill.h" 
#include "map.h" 

#define PORT 12345
#define BUF_SIZE 512
#define MAX_DOOR_HP 1000
#define MAX_PLAYER_HP 100

#define RESPAWN_TIME_ATTACKER 10
#define RESPAWN_TIME_DEFENDER 5

#define TIME_SETUP 30
#define TIME_MATCH 120

#define GS_WAITING 0
#define GS_COUNTDOWN 1
#define GS_SETUP 5
#define GS_PLAYING 2
#define GS_WIN_ATTACKER 3
#define GS_WIN_DEFENDER 4

#pragma pack(push,1)
typedef struct {
    uint32_t seq;
    float x, y;
    float angle;
    uint8_t btn; 
    uint8_t is_stealth; 
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
    int skill1_remain;
    int killCount;
    int isStunned;
    int is_stealth;
    int wallX; // ★追加
    int wallY; // ★追加
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
    time_t skill1_usedTime;
    int escudo_stock;
    int active_wall_x;
    int active_wall_y;
    int active_wall_hp;
    uint8_t is_stealth;
    int killCount;
    int isStunned;
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
        clients[i].escudo_stock = 3;
        clients[i].active_wall_x = -1; // 初期化
        clients[i].active_wall_y = -1; // 初期化
        clients[i].is_stealth = 0;
        clients[i].killCount = 0;
        clients[i].isStunned = 0;
        if (clients[i].active) {
            clients[i].role = (i == attacker_id) ? 0 : 1;
        }
    }
    
    currentGameState = GS_COUNTDOWN;
    phaseStartTime = time(NULL); 
    printf("Game Init! Attacker ID: %d\n", attacker_id);
}

int raycast_hit_check(float x1, float y1, float x2, float y2, int* hitX, int* hitY) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dist = sqrt(dx*dx + dy*dy);
    int steps = (int)(dist * 2.0f);
    if (steps < 1) steps = 1;
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        int cx = (int)(x1 + dx * t);
        int cy = (int)(y1 + dy * t);
        if (cx >= 0 && cx < MAP_WIDTH && cy >= 0 && cy < MAP_HEIGHT) {
            if (worldMap[cx][cy] != 0) {
                *hitX = cx; *hitY = cy;
                return worldMap[cx][cy];
            }
        }
    }
    return 0;
}

int main() {
    int sock;
    struct sockaddr_in srvaddr, cliaddr;
    socklen_t clilen;
    char buf[BUF_SIZE];

    srand(time(NULL));

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { perror("socket failed"); return 1; }

    memset(&srvaddr, 0, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET;
    srvaddr.sin_addr.s_addr = INADDR_ANY;
    srvaddr.sin_port = htons(PORT);

    if (bind(sock, (const struct sockaddr *)&srvaddr, sizeof(srvaddr)) < 0) { perror("bind failed"); return 1; }

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
            if (remaining < 0) { currentGameState = GS_SETUP; phaseStartTime = now; remaining = TIME_SETUP; printf("Phase: SETUP\n"); }
        } else if (currentGameState == GS_SETUP) {
            remaining = TIME_SETUP - timeElapsed;
            if (remaining < 0) { currentGameState = GS_PLAYING; phaseStartTime = now; remaining = TIME_MATCH; isBlockCarried = 0; printf("Phase: PLAYING\n"); }
        } else if (currentGameState == GS_PLAYING) {
            remaining = TIME_MATCH - timeElapsed;
            if (remaining < 0) { currentGameState = GS_WIN_DEFENDER; remaining = 0; }
        }

        for(int i=0; i<2; i++) {
            if(clients[i].active && clients[i].deadTime > 0) {
                int interval = (clients[i].role == 0) ? RESPAWN_TIME_ATTACKER : RESPAWN_TIME_DEFENDER;
                if(difftime(now, clients[i].deadTime) >= interval) {
                    clients[i].hp = MAX_PLAYER_HP; clients[i].deadTime = 0; clients[i].skill1_usedTime = 0;
                    printf("Player %d Respawned!\n", i);
                }
            }
        }

        if (n > 0) {
            pkt_t *in = (pkt_t*)buf;
            int id = -1;
            for (int i = 0; i < 2; i++) {
                if (clients[i].active && clients[i].addr.sin_addr.s_addr == cliaddr.sin_addr.s_addr && clients[i].addr.sin_port == cliaddr.sin_port) {
                    id = i; break;
                }
            }

            if (id == -1) {
                if (client_count < 2) {
                    for(int i=0; i<2; i++) {
                        if(!clients[i].active) {
                            id = i; clients[i].addr = cliaddr; clients[i].active = 1;
                            clients[i].hp = MAX_PLAYER_HP; clients[i].deadTime = 0;
                            client_count++;
                            if (client_count == 2) init_game(); else clients[i].role = 0;
                            break;
                        }
                    }
                }
            }

            if (id != -1) {
                clients[id].last_seen = time(NULL);
                if (clients[id].deadTime == 0) {
                    clients[id].x = in->x; clients[id].y = in->y; clients[id].angle = in->angle;
                }
                
                clients[id].is_stealth = in->is_stealth;

                int enemyId = (id == 0) ? 1 : 0;

                if (currentGameState == GS_WAITING && (in->btn & 16)) init_game();

                if (currentGameState == GS_SETUP && clients[id].role == 1 && clients[id].hp > 0) {
                    if ((in->btn & 32) && !(clients[id].last_btn & 32)) {
                        if (isBlockCarried) isBlockCarried = 0;
                        else {
                            float dx = clients[id].x - blockX; float dy = clients[id].y - blockY;
                            if (sqrt(dx*dx + dy*dy) < 2.0) isBlockCarried = 1;
                        }
                    }
                    if (isBlockCarried) {
                        int tx = (int)(clients[id].x + cos(clients[id].angle) * 2.0);
                        int ty = (int)(clients[id].y + sin(clients[id].angle) * 2.0);
                        if(tx>=1 && tx<=22 && ty>=1 && ty<=22 && worldMap[tx][ty] == 0) { blockX = tx; blockY = ty; }
                    }
                }

                if (currentGameState == GS_PLAYING) {
                    if (clients[id].hp > 0) {
                        if ((in->btn & 64) && !(clients[id].last_btn & 64)) {
                            int ct = (clients[id].role == 0) ? SKILL_HEAL_CT : SKILL_REPAIR_CT;
                            int passed = (int)difftime(now, clients[id].skill1_usedTime);
                            
                            if (clients[id].skill1_usedTime == 0 || passed >= ct) {
                                clients[id].skill1_usedTime = now;
                                
                                if (clients[id].role == 0) {
                                    // アタッカー: 回復のみ
                                    skill_logic_heal_generic(&clients[id].hp, MAX_PLAYER_HP, 40);
                                } else {
                                    // ★修正: ディフェンダーのみ壁設置 (ID:8)
                                    skill_logic_repair_generic(&doorHP, MAX_DOOR_HP);
                                    if (clients[id].escudo_stock > 0) {
                                        double dist = 2.0;
                                        int tx = (int)(clients[id].x + cos(clients[id].angle) * dist);
                                        int ty = (int)(clients[id].y + sin(clients[id].angle) * dist);
                                        if (tx >= 0 && tx < MAP_WIDTH && ty >= 0 && ty < MAP_HEIGHT && worldMap[tx][ty] == 0) {
                                            worldMap[tx][ty] = 8; // ID:8 (エスクード)
                                            clients[id].active_wall_x = tx; 
                                            clients[id].active_wall_y = ty; 
                                            clients[id].active_wall_hp = 200;
                                            printf("Server: Wall created at (%d, %d)\n", tx, ty);
                                        }
                                        clients[id].escudo_stock--;
                                    }
                                }
                            }
                        }

                        if (clients[id].role == 0 && (in->btn & 2)) {
                            if (doorHP > 0) doorHP -= 10;
                        }
                        if (clients[enemyId].active && clients[enemyId].hp > 0) {
                            int damage = 0;
                            if (in->btn & 8) damage = 40; else if (in->btn & 4) damage = 15;
                            if (damage > 0) {
                                int hx, hy;
                                int hitObj = raycast_hit_check(clients[id].x, clients[id].y, clients[enemyId].x, clients[enemyId].y, &hx, &hy);
                                if (hitObj == 0) {
                                    clients[enemyId].hp -= damage;
                                    if (clients[enemyId].hp <= 0) {
                                        clients[enemyId].hp = 0; clients[enemyId].deadTime = time(NULL);
                                        clients[id].killCount++;
                                        printf("Player %d Killed!\n", enemyId);
                                    }
                                } else if (hitObj == 8) { // ★修正: 壁ID 8 の当たり判定
                                    for (int k = 0; k < 2; k++) {
                                        if (clients[k].active_wall_x == hx && clients[k].active_wall_y == hy) {
                                            clients[k].active_wall_hp -= damage;
                                            if (clients[k].active_wall_hp <= 0) {
                                                worldMap[hx][hy] = 0; 
                                                clients[k].active_wall_x = -1;
                                                printf("Wall Destroyed!\n");
                                            }
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if (doorHP <= 0) currentGameState = GS_WIN_ATTACKER;
                }

                clients[id].last_btn = in->btn;

                server_pkt_t out;
                out.role = clients[id].role; out.doorHP = doorHP; out.gameState = currentGameState;
                out.selfHP = clients[id].hp; out.remainingTime = remaining;
                out.blockX = blockX; out.blockY = blockY;
                out.respawnTime = 0;
                out.killCount = clients[id].killCount;
                out.isStunned = clients[id].isStunned;
                out.is_stealth = clients[id].is_stealth;
                
                // ★追加: 存在する壁の位置を送信 (なければ -1)
                // どちらかのプレイヤーが出した壁を全員に同期する
                out.wallX = -1; out.wallY = -1;
                if (clients[0].active_wall_x != -1) { 
                    out.wallX = clients[0].active_wall_x; out.wallY = clients[0].active_wall_y; 
                } else if (clients[1].active_wall_x != -1) {
                    out.wallX = clients[1].active_wall_x; out.wallY = clients[1].active_wall_y;
                }

                if (clients[id].deadTime > 0) {
                    int interval = (clients[id].role == 0) ? RESPAWN_TIME_ATTACKER : RESPAWN_TIME_DEFENDER;
                    int passed = (int)difftime(now, clients[id].deadTime);
                    out.respawnTime = interval - passed; if (out.respawnTime < 0) out.respawnTime = 0;
                }
                out.skill1_remain = 0;
                if (clients[id].skill1_usedTime > 0) {
                    int ct = (clients[id].role == 0) ? SKILL_HEAL_CT : SKILL_REPAIR_CT;
                    int passed = (int)difftime(now, clients[id].skill1_usedTime);
                    out.skill1_remain = ct - passed; if (out.skill1_remain < 0) out.skill1_remain = 0;
                }

                if (clients[enemyId].active) {
                    int visible = 1;
                    if (clients[enemyId].deadTime > 0) visible = 0;
                    if (clients[enemyId].is_stealth) visible = 0;

                    if (visible) {
                        out.active = 1; out.x = clients[enemyId].x; out.y = clients[enemyId].y;
                        out.angle = clients[enemyId].angle; out.enemyHP = clients[enemyId].hp;
                    } else {
                        out.active = 0; out.enemyHP = 0;
                    }
                } else {
                    out.active = 0; out.enemyHP = 0;
                }
                sendto(sock, &out, sizeof(out), 0, (struct sockaddr *)&cliaddr, clilen);
            }
        }
    }
    close(sock);
    return 0;
}