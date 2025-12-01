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
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h> // 文字描画用

#include "map.h"
#include "player.h"

#define SERVER_IP "192.168.1.10"
#define SERVER_PORT 12345
#define BUF_SIZE 512
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

// 通信パケット
#pragma pack(push,1)
typedef struct {
    uint32_t seq;
    float x, y;
    float angle;
    uint8_t btn; // ビット0: 発砲フラグ, ビット1: 被弾通知など
} pkt_t;
#pragma pack(pop)

typedef struct {
    float x, y, angle;
    int active;
    uint32_t last_update;
} Enemy;

// グローバル変数
double zBuffer[SCREEN_WIDTH];
TTF_Font *font = NULL;

// 簡易ヘルパー関数
int make_socket_nonblocking(int s) {
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(s, F_SETFL, flags | O_NONBLOCK);
}

// テキスト描画関数
void draw_text(SDL_Renderer *ren, const char *text, int x, int y, SDL_Color color) {
    if (!font) return;
    SDL_Surface *surf = TTF_RenderUTF8_Solid(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_Rect rect = {x, y, surf->w, surf->h};
    SDL_RenderCopy(ren, tex, NULL, &rect);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

// 床・天井描画
void draw_floor_ceiling(SDL_Renderer* renderer) {
    SDL_Rect ceiling = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT / 2};
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255); // 暗いグレー
    SDL_RenderFillRect(renderer, &ceiling);
    SDL_Rect floor = {0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT / 2};
    SDL_SetRenderDrawColor(renderer, 70, 70, 70, 255); // グレー
    SDL_RenderFillRect(renderer, &floor);
}

// 壁描画 (Zバッファ更新)
void draw_walls(SDL_Renderer* renderer, Player* player) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        double cameraX = (double)x / (double)SCREEN_WIDTH * 2.0 - 1.0;
        double rayAngle = player->angle + atan(cameraX * tan(player->fov / 2.0));
        double rayDirX = cos(rayAngle), rayDirY = sin(rayAngle);
        
        int mapX = (int)player->x, mapY = (int)player->y;
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

        double perpWallDist = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
        zBuffer[x] = perpWallDist;

        int lineHeight = (int)(SCREEN_HEIGHT / (perpWallDist * cos(rayAngle - player->angle)));
        int drawStart = -lineHeight / 2 + SCREEN_HEIGHT / 2;
        if (drawStart < 0) drawStart = 0;
        int drawEnd = lineHeight / 2 + SCREEN_HEIGHT / 2;
        if (drawEnd >= SCREEN_HEIGHT) drawEnd = SCREEN_HEIGHT - 1;

        int colorVal = (side == 1) ? 100 : 160;
        SDL_SetRenderDrawColor(renderer, colorVal, colorVal, colorVal, 255);
        SDL_RenderDrawLine(renderer, x, drawStart, x, drawEnd);
    }
}

// 敵描画＆ヒット判定用情報の取得
// 戻り値: 1=画面中央(クロスヘア)に敵がいる, 0=いない
int draw_enemy(SDL_Renderer* renderer, Player* player, Enemy* enemy, SDL_Texture* texture) {
    if (!enemy->active) return 0;

    double spriteX = enemy->x - player->x;
    double spriteY = enemy->y - player->y;

    double invDet = 1.0 / (cos(player->angle) * 0.66 - 0.66 * sin(player->angle)); // 簡易
    double angleToEnemy = atan2(spriteY, spriteX) - player->angle;
    while (angleToEnemy < -M_PI) angleToEnemy += 2 * M_PI;
    while (angleToEnemy > M_PI)  angleToEnemy -= 2 * M_PI;

    if (fabs(angleToEnemy) > player->fov / 1.5) return 0;

    double dist = sqrt(spriteX*spriteX + spriteY*spriteY);
    if (dist < 0.2) return 0;

    double screenX = (0.5 + (angleToEnemy / player->fov)) * SCREEN_WIDTH;
    int spriteHeight = abs((int)(SCREEN_HEIGHT / dist));
    int spriteWidth = spriteHeight / 2; 
    if (texture) {
        int w, h; SDL_QueryTexture(texture, NULL, NULL, &w, &h);
        spriteWidth = (int)(spriteHeight * ((float)w/h));
    }

    int drawStartX = (int)screenX - spriteWidth / 2;
    int drawEndX = drawStartX + spriteWidth;
    int drawStartY = -spriteHeight / 2 + SCREEN_HEIGHT / 2;
    
    // 描画
    if (texture) {
        for (int stripe = drawStartX; stripe < drawEndX; stripe++) {
            if (stripe >= 0 && stripe < SCREEN_WIDTH && dist < zBuffer[stripe]) {
                int w, h; SDL_QueryTexture(texture, NULL, NULL, &w, &h);
                int texX = (int)((stripe - drawStartX) * (float)w / spriteWidth);
                SDL_Rect src = {texX, 0, 1, h};
                SDL_Rect dst = {stripe, drawStartY, 1, spriteHeight};
                SDL_RenderCopy(renderer, texture, &src, &dst);
            }
        }
    } else {
        // テクスチャがない場合は赤い矩形
        SDL_Rect r = {drawStartX, drawStartY, spriteWidth, spriteHeight};
        // 簡易Zチェック（中心点だけ見る）
        int mid = (drawStartX + drawEndX)/2;
        if (mid >=0 && mid < SCREEN_WIDTH && dist < zBuffer[mid]) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &r);
        }
    }

    // クロスヘア（画面中央X=400付近）が敵のスプライト内にあるか判定
    int centerX = SCREEN_WIDTH / 2;
    if (centerX >= drawStartX && centerX <= drawEndX) {
        // さらに壁に隠れていないかチェック
        if (dist < zBuffer[centerX]) {
            return 1; // ターゲット捕捉中
        }
    }
    return 0;
}

// クロスヘア（照準）の描画
void draw_crosshair(SDL_Renderer* ren) {
    SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);
    int cx = SCREEN_WIDTH / 2;
    int cy = SCREEN_HEIGHT / 2;
    int size = 10;
    SDL_RenderDrawLine(ren, cx - size, cy, cx + size, cy);
    SDL_RenderDrawLine(ren, cx, cy - size, cx, cy + size);
}

// 銃の描画
void draw_gun(SDL_Renderer* ren, SDL_Texture* gunTex, int isFiring) {
    if (gunTex) {
        // 画面右下に表示
        int w, h; SDL_QueryTexture(gunTex, NULL, NULL, &w, &h);
        int scale = 3;
        SDL_Rect dst = {SCREEN_WIDTH - (w*scale) - 50, SCREEN_HEIGHT - (h*scale), w*scale, h*scale};
        
        // 発砲時は少し揺らす
        if (isFiring) {
            dst.y += 20; 
            dst.x -= 10;
        }
        SDL_RenderCopy(ren, gunTex, NULL, &dst);
    } else {
        // テクスチャがない場合
        SDL_SetRenderDrawColor(ren, 100, 100, 100, 255);
        SDL_Rect r = {SCREEN_WIDTH - 200, SCREEN_HEIGHT - 150, 150, 150};
        if (isFiring) r.y += 20;
        SDL_RenderFillRect(ren, &r);
    }
}

int main(int argc, char **argv) {
    // --- 初期化 ---
    int sock; struct sockaddr_in srvaddr;
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) return 1;
    make_socket_nonblocking(sock);
    memset(&srvaddr, 0, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET; srvaddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &srvaddr.sin_addr);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    if (IMG_Init(IMG_INIT_PNG) == 0) printf("IMG_Init Warning: %s\n", IMG_GetError());
    if (TTF_Init() != 0) printf("TTF_Init Warning: %s\n", TTF_GetError());

    SDL_Window *win = SDL_CreateWindow("FPS Client", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    // リソース読み込み
    SDL_Texture* enemyTex = IMG_LoadTexture(ren, "enemy.png");
    SDL_Texture* gunTex = IMG_LoadTexture(ren, "gun.png");
    font = TTF_OpenFont("font.ttf", 48); // フォントがないと文字は出ません

    // FPSモード設定 (マウスカーソルを消して中央に固定)
    SDL_SetRelativeMouseMode(SDL_TRUE);

    Player player; player_init(&player);
    Enemy enemy = {0};
    uint32_t seq = 0;
    int running = 1;
    Uint64 LAST = SDL_GetPerformanceCounter();

    // ゲーム状態
    int isFiring = 0;
    int fireTimer = 0;
    char hitMsg[64] = "";
    int hitTimer = 0;

    // --- メインループ ---
    while (running) {
        Uint64 NOW = SDL_GetPerformanceCounter();
        double dt = (double)(NOW - LAST) / (double)SDL_GetPerformanceFrequency();
        LAST = NOW;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            // マウス入力 (発砲)
            if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
                isFiring = 5; // 発砲アニメーション用フレーム数
            }
        }

        // --- (A) 入力 (マウスルック & 移動) ---
        int mouseX, mouseY;
        SDL_GetRelativeMouseState(&mouseX, &mouseY);
        player.angle += mouseX * 0.005; // 感度調整

        const Uint8 *key_state = SDL_GetKeyboardState(NULL);
        // player_handle_input(player.c) は回転(A,D)を含むが、FPSなのでストレイフ(カニ歩き)に改造したいところ
        // ここでは簡易的に既存関数を使いつつ、マウス回転を優先
        // 既存のplayer.cの回転処理と競合しないよう注意
        // 以下の独自移動処理で上書きします
        float moveSpeed = 3.0 * dt;
        if (key_state[SDL_SCANCODE_W]) {
            float nx = player.x + cos(player.angle)*moveSpeed;
            float ny = player.y + sin(player.angle)*moveSpeed;
            if(worldMap[(int)nx][(int)player.y]==0) player.x = nx;
            if(worldMap[(int)player.x][(int)ny]==0) player.y = ny;
        }
        if (key_state[SDL_SCANCODE_S]) {
            float nx = player.x - cos(player.angle)*moveSpeed;
            float ny = player.y - sin(player.angle)*moveSpeed;
            if(worldMap[(int)nx][(int)player.y]==0) player.x = nx;
            if(worldMap[(int)player.x][(int)ny]==0) player.y = ny;
        }
        // ストレイフ（左右移動）
        if (key_state[SDL_SCANCODE_A]) {
            float nx = player.x + cos(player.angle - M_PI/2)*moveSpeed;
            float ny = player.y + sin(player.angle - M_PI/2)*moveSpeed;
            if(worldMap[(int)nx][(int)player.y]==0) player.x = nx;
            if(worldMap[(int)player.x][(int)ny]==0) player.y = ny;
        }
        if (key_state[SDL_SCANCODE_D]) {
            float nx = player.x + cos(player.angle + M_PI/2)*moveSpeed;
            float ny = player.y + sin(player.angle + M_PI/2)*moveSpeed;
            if(worldMap[(int)nx][(int)player.y]==0) player.x = nx;
            if(worldMap[(int)player.x][(int)ny]==0) player.y = ny;
        }
        // ESCキーで終了（マウスロック解除用）
        if (key_state[SDL_SCANCODE_ESCAPE]) running = 0;


        // --- (B) 描画 ---
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);

        draw_floor_ceiling(ren);
        draw_walls(ren, &player);

        // 敵描画 & 照準判定
        int isTargeted = draw_enemy(ren, &player, &enemy, enemyTex);

        // --- (C) 射撃処理とヘッドショット判定 ---
        if (isFiring > 0) {
            isFiring--;
            // 発砲した瞬間のフレーム(例えば5)でのみヒット判定
            if (isFiring == 4) {
                if (isTargeted) {
                    // 角度差を厳密に計算してヘッドショット判定
                    double dx = enemy.x - player.x;
                    double dy = enemy.y - player.y;
                    double angleToEnemy = atan2(dy, dx);
                    double diff = angleToEnemy - player.angle;
                    // 正規化
                    while (diff < -M_PI) diff += 2*M_PI;
                    while (diff > M_PI) diff -= 2*M_PI;

                    // 許容誤差: 距離が遠いほど厳しくなるが、角度差(ラジアン)で判定すれば一定の「見た目の幅」になる
                    // 0.02ラジアン以内ならヘッドショットとする
                    if (fabs(diff) < 0.025) {
                        sprintf(hitMsg, "HEADSHOT!!");
                        hitTimer = 60; // 1秒間表示
                    } else {
                        sprintf(hitMsg, "HIT!");
                        hitTimer = 30; // 0.5秒間表示
                    }
                }
            }
        }

        // UI描画
        draw_crosshair(ren);
        draw_gun(ren, gunTex, isFiring > 0);

        if (hitTimer > 0) {
            SDL_Color col = {255, 0, 0, 255};
            if (strcmp(hitMsg, "HEADSHOT!!") == 0) col.g = 255; // 黄色っぽくしたいが簡易
            draw_text(ren, hitMsg, SCREEN_WIDTH/2 - 100, 100, col);
            hitTimer--;
        }

        SDL_RenderPresent(ren);

        // --- (D) 通信 ---
        pkt_t out;
        out.seq = htonl(seq++);
        out.x = player.x; out.y = player.y; out.angle = player.angle;
        out.btn = (isFiring > 0) ? 1 : 0; // 発砲情報を送る
        sendto(sock, &out, sizeof(out), 0, (struct sockaddr*)&srvaddr, sizeof(srvaddr));

        char buf[BUF_SIZE];
        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        while (recvfrom(sock, buf, BUF_SIZE, 0, (struct sockaddr*)&from, &fromlen) > 0) {
             pkt_t in; memcpy(&in, buf, sizeof(pkt_t));
             enemy.x = in.x; enemy.y = in.y; enemy.angle = in.angle; enemy.active = 1;
        }
    }

    // 終了処理
    if(enemyTex) SDL_DestroyTexture(enemyTex);
    if(gunTex) SDL_DestroyTexture(gunTex);
    if(font) TTF_CloseFont(font);
    IMG_Quit();
    TTF_Quit();
    close(sock);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}