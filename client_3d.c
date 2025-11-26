#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <SDL2/SDL.h>

// 既存のヘッダを利用
#include "map.h"
#include "player.h"

// サーバー設定（環境に合わせて変更してください）
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 12345
#define BUF_SIZE 512

// 画面サイズ
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

// 通信パケット構造体（角度 angle を追加）
#pragma pack(push,1)
typedef struct {
    uint32_t seq;
    float x, y;
    float angle; // 相手に向きを伝えるために追加
    uint8_t btn;
} pkt_t;
#pragma pack(pop)

// 敵プレイヤー管理用
typedef struct {
    float x, y, angle;
    int active;     // データを受信したら 1
    uint32_t last_update; // タイムアウト処理用
} Enemy;

// Zバッファ（壁の距離を記録し、敵の描画判定に使う）
double zBuffer[SCREEN_WIDTH];

// ソケットの非ブロッキング化
int make_socket_nonblocking(int s) {
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(s, F_SETFL, flags | O_NONBLOCK);
}

// 床と天井の描画
void draw_floor_ceiling(SDL_Renderer* renderer) {
    // 天井
    SDL_Rect ceiling = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT / 2};
    SDL_SetRenderDrawColor(renderer, 50, 50, 80, 255);
    SDL_RenderFillRect(renderer, &ceiling);
    // 床
    SDL_Rect floor = {0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT / 2};
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(renderer, &floor);
}

// 壁の描画（main.c から移植し、Zバッファへの書き込みを追加）
void draw_walls(SDL_Renderer* renderer, Player* player) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        double cameraX = (double)x / (double)SCREEN_WIDTH * 2.0 - 1.0;
        double rayAngle = player->angle + atan(cameraX * tan(player->fov / 2.0));
        
        double rayDirX = cos(rayAngle);
        double rayDirY = sin(rayAngle);

        int mapX = (int)player->x;
        int mapY = (int)player->y;

        double deltaDistX = (rayDirX == 0) ? 1e30 : fabs(1.0 / rayDirX);
        double deltaDistY = (rayDirY == 0) ? 1e30 : fabs(1.0 / rayDirY);
        double sideDistX, sideDistY;
        int stepX, stepY;

        if (rayDirX < 0) { stepX = -1; sideDistX = (player->x - mapX) * deltaDistX; }
        else             { stepX = 1;  sideDistX = (mapX + 1.0 - player->x) * deltaDistX; }
        if (rayDirY < 0) { stepY = -1; sideDistY = (player->y - mapY) * deltaDistY; }
        else             { stepY = 1;  sideDistY = (mapY + 1.0 - player->y) * deltaDistY; }

        int hit = 0, side = 0;
        while (hit == 0) {
            if (sideDistX < sideDistY) { sideDistX += deltaDistX; mapX += stepX; side = 0; }
            else                       { sideDistY += deltaDistY; mapY += stepY; side = 1; }
            if (worldMap[mapX][mapY] > 0) hit = 1;
        }

        double perpWallDist;
        if (side == 0) perpWallDist = (sideDistX - deltaDistX);
        else           perpWallDist = (sideDistY - deltaDistY);

        // ★Zバッファに距離を保存（敵の隠蔽処理用）
        zBuffer[x] = perpWallDist;

        // 描画
        double correctedDist = perpWallDist * cos(rayAngle - player->angle);
        int lineHeight = (int)(SCREEN_HEIGHT / correctedDist);
        int drawStart = -lineHeight / 2 + SCREEN_HEIGHT / 2;
        if (drawStart < 0) drawStart = 0;
        int drawEnd = lineHeight / 2 + SCREEN_HEIGHT / 2;
        if (drawEnd >= SCREEN_HEIGHT) drawEnd = SCREEN_HEIGHT - 1;

        if (side == 1) SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        else           SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
        
        SDL_RenderDrawLine(renderer, x, drawStart, x, drawEnd);
    }
}

// 敵プレイヤーの描画（ビルボード方式）
void draw_enemy(SDL_Renderer* renderer, Player* player, Enemy* enemy) {
    if (!enemy->active) return;

    // プレイヤーから見た敵の相対位置
    double spriteX = enemy->x - player->x;
    double spriteY = enemy->y - player->y;

    // 座標変換（プレイヤーの向きに対する回転行列の逆行列を適用）
    // これにより、カメラ手前がX軸、カメラ左右がY軸のような空間に変換
    // 定石的な計算式を使用（魚眼補正なしの生データ）
    double invDet = 1.0 / (cos(player->angle) * 0.66 - 0.66 * sin(player->angle)); // 0.66はFOV関連の定数(plane)の簡易値
    // 簡易実装のため、もっと直感的な「角度差」アプローチを使います
    
    // 1. 敵への角度を計算
    double angleToEnemy = atan2(spriteY, spriteX) - player->angle;
    while (angleToEnemy < -M_PI) angleToEnemy += 2 * M_PI;
    while (angleToEnemy > M_PI)  angleToEnemy -= 2 * M_PI;

    // 2. 視野内かチェック
    if (fabs(angleToEnemy) > player->fov / 1.5) return; 

    double dist = sqrt(spriteX*spriteX + spriteY*spriteY);
    if (dist < 0.5) return; // 近すぎる場合は描画しない

    // 3. 画面上のX座標を計算
    // スクリーン中心(0.5) から 角度のズレ分移動させる
    double screenX = (0.5 * (1.0 - angleToEnemy / (player->fov / 2.0))) * SCREEN_WIDTH;
    // 上記計算だと左右逆になることがあるため、単純化して調整:
    screenX = (0.5 + (angleToEnemy / player->fov)) * SCREEN_WIDTH; // 簡易近似
    // ※厳密なRaycasting Sprite描画は複雑なので、ここでは「角度による簡易配置」を行います

    // 4. サイズ計算
    int spriteHeight = abs((int)(SCREEN_HEIGHT / dist)); 
    int spriteWidth = spriteHeight / 2; 

    int drawStartX = (int)screenX - spriteWidth / 2;
    int drawEndX = drawStartX + spriteWidth;
    int drawStartY = -spriteHeight / 2 + SCREEN_HEIGHT / 2;
    int drawEndY = drawStartY + spriteHeight;

    // 画面外クリップ
    if (drawStartX < 0) drawStartX = 0;
    if (drawEndX >= SCREEN_WIDTH) drawEndX = SCREEN_WIDTH - 1;
    if (drawStartY < 0) drawStartY = 0;
    if (drawEndY >= SCREEN_HEIGHT) drawEndY = SCREEN_HEIGHT - 1;

    // 5. 描画ループ (Zバッファ判定付き)
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // 赤色
    for (int stripe = drawStartX; stripe < drawEndX; stripe++) {
        // 現在の列が画面内か
        if (stripe >= 0 && stripe < SCREEN_WIDTH) {
            // 壁の手前にいる場合のみ描画
            if (dist < zBuffer[stripe]) {
                SDL_RenderDrawLine(renderer, stripe, drawStartY, stripe, drawEndY);
            }
        }
    }
}

int main(int argc, char **argv) {
    // 1. 通信設定
    int sock;
    struct sockaddr_in srvaddr;
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) return 1;
    make_socket_nonblocking(sock); // 非ブロッキング
    
    memset(&srvaddr, 0, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET;
    srvaddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &srvaddr.sin_addr);

    // 2. SDL初期化
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    SDL_Window *win = SDL_CreateWindow("Pseudo-3D FPS Client", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    // 3. プレイヤー初期化 (player.c の関数を使用)
    Player player;
    player_init(&player); 

    // 敵データ
    Enemy enemy = {0};
    uint32_t seq = 0;

    int running = 1;
    Uint64 LAST = SDL_GetPerformanceCounter();

    // 4. メインループ
    while (running) {
        Uint64 NOW = SDL_GetPerformanceCounter();
        double deltaTime = (double)(NOW - LAST) / (double)SDL_GetPerformanceFrequency();
        LAST = NOW;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
        }

        // --- (A) 入力と移動 ---
        const Uint8 *key_state = SDL_GetKeyboardState(NULL);
        player_handle_input(&player, key_state, deltaTime); // player.c の関数

        // --- (B) 送信 ---
        pkt_t out;
        out.seq = htonl(seq++);
        out.x = player.x; 
        out.y = player.y; 
        out.angle = (float)player.angle; // 角度も送る
        out.btn = 0;
        sendto(sock, &out, sizeof(out), 0, (struct sockaddr*)&srvaddr, sizeof(srvaddr));

        // --- (C) 受信 ---
        char buf[BUF_SIZE];
        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        ssize_t n;
        while ((n = recvfrom(sock, buf, BUF_SIZE, 0, (struct sockaddr*)&from, &fromlen)) > 0) {
            if (n >= (ssize_t)sizeof(pkt_t)) {
                pkt_t in;
                memcpy(&in, buf, sizeof(pkt_t));
                // 自分以外のパケットを敵として処理（簡易的にIP/Portチェック省略）
                // 本来はIPチェックが必要ですが、1v1前提で受信データ＝敵とみなします
                enemy.x = in.x;
                enemy.y = in.y;
                enemy.angle = in.angle;
                enemy.active = 1;
                enemy.last_update = SDL_GetTicks();
            }
            fromlen = sizeof(from);
        }

        // --- (D) 描画 ---
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);

        draw_floor_ceiling(ren);         // 床と天井
        draw_walls(ren, &player);        // 壁 (ここでZバッファ構築)
        draw_enemy(ren, &player, &enemy);// 敵 (Zバッファ参照)

        SDL_RenderPresent(ren);
    }

    close(sock);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}