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
    int killCount;
    int isStunned;
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

    int escudo_stock;

    int active_wall_x;
    int active_wall_y;
    int active_wall_hp;

    int killCount;
    time_t stunEndTime;
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
        clients[i].escudo_stock = 3;   // ★追加: ストックを3に設定

        clients[i].killCount = 0;
        clients[i].stunEndTime = 0;

        if (clients[i].active) {
            clients[i].role = (i == attacker_id) ? 0 : 1;
        }
    }
    
    currentGameState = GS_COUNTDOWN;
    phaseStartTime = time(NULL); 
    printf("Game Init! Attacker ID: %d\n", attacker_id);
}

// server.c の main関数の前あたりに追加

// レイキャスト関数：弾が何にぶつかるか調べる
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
                *hitX = cx;
                *hitY = cy;
                return worldMap[cx][cy]; // 壁のIDを返す
            }
        }
    }
    return 0; // 遮蔽なし
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
                    clients[i].stunEndTime = 0;
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
                            clients[i].killCount = 0;
                            clients[i].stunEndTime = 0;
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
                        
                        // 必殺技発動 (ボタン128: Qキー)
                        if ((in->btn & 128) && !(clients[id].last_btn & 128)) {
                            // 防衛側のみ発動可能
                            if (clients[id].role == 1) {
                                // 撃破数が3以上必要
                                if (clients[id].killCount >= 3) {
                                    clients[id].killCount -= 3; // コスト消費
                                    // 敵を5秒間スタン
                                    clients[enemyId].stunEndTime = now + 5;
                                    printf("DEFENDER (P%d) used ULTIMATE! Attacker Stunned!\n", id);
                                } else {
                                    printf("Defender P%d tried Ult but low kills (%d/3)\n", id, clients[id].killCount);
                                }
                            }
                        }

                        // スキルボタン(64)が押された時の処理
                        if ((in->btn & 64) && !(clients[id].last_btn & 64)) {
                            // クールタイム設定（役割別）
                            int ct = (clients[id].role == 0) ? SKILL_HEAL_CT : SKILL_REPAIR_CT;
                            int passed = (int)difftime(now, clients[id].skill1_usedTime);
                            
                            // 判定: クールタイム経過済み かつ ★ストックがあるか
                            if ((clients[id].skill1_usedTime == 0 || passed >= ct) && clients[id].escudo_stock > 0) {
                                clients[id].skill1_usedTime = now;
                                
                                // --- 1. 既存の効果 (回復/修理) ---
                                if (clients[id].role == 0) {
                                    skill_logic_heal_generic(&clients[id].hp, MAX_PLAYER_HP, 40);
                                    printf("Attacker P%d used Skill: Heal\n", id);
                                } else {
                                    skill_logic_repair_generic(&doorHP, MAX_DOOR_HP);
                                    printf("Defender P%d used Skill: Repair\n", id);
                                }

                                // --- 2. ★追加: 壁(Escudo)の生成処理 ---
                                double dist = 2.0;
                                int tx = (int)(clients[id].x + cos(clients[id].angle) * dist);
                                int ty = (int)(clients[id].y + sin(clients[id].angle) * dist);

                                // マップ範囲内かつ床(0)なら設置
                                if (tx >= 0 && tx < MAP_WIDTH && ty >= 0 && ty < MAP_HEIGHT) {
                                    if (worldMap[tx][ty] == 0) {
                                        worldMap[tx][ty] = 2; // 壁ID=2を配置
                                        
                                        // 誰の壁か記録
                                        clients[id].active_wall_x = tx;
                                        clients[id].active_wall_y = ty;
                                        clients[id].active_wall_hp = 200; // 耐久値
                                        
                                        printf("Server: Wall created at (%d, %d)\n", tx, ty);
                                    }
                                }

                                // --- 3. ★追加: ストック消費 ---
                                clients[id].escudo_stock--;
                                printf("Player %d Escudo Stock: %d\n", id, clients[id].escudo_stock);
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
                            
                            // server.c の main関数内の攻撃処理部分
                            
                            // (直前に damage の計算があるはずです)
                            if (damage > 0) {
                                int hx, hy;
                                // ★修正: 壁判定を行う関数を呼ぶ
                                int hitObj = raycast_hit_check(clients[id].x, clients[id].y, clients[enemyId].x, clients[enemyId].y, &hx, &hy);

                                if (hitObj == 0) {
                                    // 遮蔽なし -> そのまま敵にダメージ
                                    clients[enemyId].hp -= damage;
                                    printf("Hit Enemy! Damage: %d\n", damage);
                                    if (clients[enemyId].hp <= 0) {
                                        clients[enemyId].hp = 0;
                                        clients[enemyId].deadTime = time(NULL); 
                                        printf("Player %d Killed!\n", enemyId);

                                        // 敵を倒したらキルカウント+1
                                        clients[id].killCount++;
                                        printf("Player %d Kill Count: %d\n", id, clients[id].killCount);
                                    }
                                } else if (hitObj == 2) {
                                    // スキル壁に命中 -> 壁にダメージ
                                    printf("Hit Wall at (%d, %d)!\n", hx, hy);
                                    for (int k = 0; k < 2; k++) {
                                        // 誰の壁か探してHPを減らす
                                        if (clients[k].active_wall_x == hx && clients[k].active_wall_y == hy) {
                                            clients[k].active_wall_hp -= damage;
                                            printf("Wall HP: %d\n", clients[k].active_wall_hp);
                                            
                                            // HPが尽きたら壁を破壊
                                            if (clients[k].active_wall_hp <= 0) {
                                                worldMap[hx][hy] = 0; // マップから消す
                                                clients[k].active_wall_x = -1; // 座標リセット
                                                printf("Wall Destroyed!\n");
                                            }
                                            break;
                                        }
                                    }
                                }
                                // hitObj == 1 (通常の壁) の場合は何もしない（ブロックされる）
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

                out.killCount = clients[id].killCount;
                out.isStunned = (clients[id].stunEndTime > now) ? 1 : 0;                
                
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