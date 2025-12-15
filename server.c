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
    int role;       // 0:攻め, 1:守り
    int gameState;  // 0:プレイ中, 1:攻め勝ち, 2:守り勝ち
    int selfHP;     // 自分のHP
    int enemyHP;    // 敵のHP
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

void init_game() {
    doorHP = MAX_DOOR_HP;
    attacker_id = rand() % 2;
    for(int i=0; i<2; i++) {
        clients[i].hp = MAX_PLAYER_HP;
        if (clients[i].active) {
            clients[i].role = (i == attacker_id) ? 0 : 1;
        }
    }
    printf("Game Reset. Attacker is Player %d\n", attacker_id);
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

    while (1) {
        clilen = sizeof(cliaddr);
        ssize_t n = recvfrom(sock, buf, BUF_SIZE, 0, (struct sockaddr *)&cliaddr, &clilen);

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
                            
                            if (client_count == 2) {
                                init_game();
                            } else {
                                attacker_id = 0; 
                                clients[i].role = (id == attacker_id) ? 0 : 1;
                            }
                            printf("Player %d joined.\n", id);
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

                // ★本来のルールに戻す: 攻め側(role==0)のみ扉を破壊可能
                if (clients[id].role == 0 && (in->btn & 2)) {
                    if (doorHP > 0) {
                        doorHP -= 10;
                        printf("Door Damaged! HP: %d\n", doorHP);
                    }
                }

                // 敵への攻撃
                if (clients[enemyId].active && clients[enemyId].hp > 0) {
                    if (in->btn & 8) {      
                        clients[enemyId].hp -= 40; 
                        printf("Headshot! P%d -> P%d (HP:%d)\n", id, enemyId, clients[enemyId].hp);
                    } else if (in->btn & 4) { 
                        clients[enemyId].hp -= 15; 
                        printf("Hit! P%d -> P%d (HP:%d)\n", id, enemyId, clients[enemyId].hp);
                    }
                    if (clients[enemyId].hp < 0) clients[enemyId].hp = 0;
                }

                // 勝敗判定
                int state = 0;
                if (doorHP <= 0) {
                    state = 1; // 攻め勝ち
                } else if (clients[0].active && clients[1].active) {
                    if (clients[0].role == 0 && clients[0].hp <= 0) state = 2; // 守り勝ち
                    if (clients[1].role == 0 && clients[1].hp <= 0) state = 2; 
                    if (clients[0].role == 1 && clients[0].hp <= 0) state = 1; // 攻め勝ち
                    if (clients[1].role == 1 && clients[1].hp <= 0) state = 1; 
                }

                server_pkt_t out;
                out.role = clients[id].role;
                out.doorHP = doorHP;
                out.gameState = state;
                out.selfHP = clients[id].hp;
                
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