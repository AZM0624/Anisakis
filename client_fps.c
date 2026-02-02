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
#include <SDL2/SDL_ttf.h> 

#include "map.h"
#include "player.h"
#include "skill.h" 

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ★★★ サーバーPCのIPアドレスに書き換えてください ★★★
#define SERVER_IP "192.168.1.151" 

#define SERVER_PORT 12345
#define BUF_SIZE 512
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

#define MAX_AMMO 30
#define RELOAD_TIME 60
#define MAX_DOOR_HP 1000
#define MAX_PLAYER_HP 100

#define FIRE_ANIM_TIME 15
#define FLASH_DURATION 10

#define GRAVITY 15.0      
#define JUMP_POWER 4.0    
#define PITCH_SCALE 150.0 

#define GS_WAITING 0
#define GS_COUNTDOWN 1
#define GS_SETUP 5
#define GS_PLAYING 2
#define GS_WIN_ATTACKER 3
#define GS_WIN_DEFENDER 4

#define SKILL1_COOLDOWN 30.0   /* F1: ヒールのクールダウン */
#define SKILL2_COOLDOWN 30.0   /* F2: シールドのクールダウン */
#define SKILL3_COOLDOWN 20.0   // F3: ダッシュ
#define SKILL4_COOLDOWN 30.0   // F4: ステルス

#ifndef MAP_WIDTH
#define MAP_WIDTH 24
#define MAP_HEIGHT 24
#endif

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
    int skill1_remain; 
    int killCount;
    int isStunned;
    int is_stealth;
} server_pkt_t;
#pragma pack(pop)

typedef struct {
    float x, y, angle;
    int active;
    int is_stealth;
} Enemy;

double zBuffer[SCREEN_WIDTH];
TTF_Font *font = NULL;
TTF_Font *fontBig = NULL;

int make_socket_nonblocking(int s) {
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(s, F_SETFL, flags | O_NONBLOCK);
}

void update_block_position(int tx, int ty) {
    for(int x=0; x<MAP_WIDTH; x++) {
        for(int y=0; y<MAP_HEIGHT; y++) {
            if(worldMap[x][y] == 9) worldMap[x][y] = 0;
        }
    }
    if(tx >= 0 && tx < MAP_WIDTH && ty >= 0 && ty < MAP_HEIGHT) {
        worldMap[tx][ty] = 9;
    }
}

void draw_text_bg(SDL_Renderer *ren, TTF_Font* useFont, const char *text, int x, int y, SDL_Color color, SDL_Color bgColor) {
    if (!useFont) return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(useFont, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    SDL_Rect bgRect = {x - 5, y - 5, surf->w + 10, surf->h + 10};
    SDL_RenderFillRect(ren, &bgRect);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    SDL_Rect rect = {x, y, surf->w, surf->h};
    SDL_RenderCopy(ren, tex, NULL, &rect);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

void draw_floor_ceiling(SDL_Renderer* renderer, int pitch) {
    SDL_Rect ceiling = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT / 2 + pitch};
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255); 
    SDL_RenderFillRect(renderer, &ceiling);
    int floorY = SCREEN_HEIGHT / 2 + pitch;
    if (floorY < 0) floorY = 0; if (floorY > SCREEN_HEIGHT) floorY = SCREEN_HEIGHT;
    SDL_Rect floor = {0, floorY, SCREEN_WIDTH, SCREEN_HEIGHT - floorY};
    SDL_SetRenderDrawColor(renderer, 70, 70, 70, 255); 
    SDL_RenderFillRect(renderer, &floor);
}

void draw_walls(SDL_Renderer* renderer, Player* player, int doorHP) {
    int pitch = (int)(player->z * PITCH_SCALE);
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
        int hit = 0, side = 0; int hitType = 0;
        while (hit == 0) {
            if (sideDistX < sideDistY) { sideDistX += deltaDistX; mapX += stepX; side = 0; }
            else                       { sideDistY += deltaDistY; mapY += stepY; side = 1; }
            if (worldMap[mapX][mapY] > 0) { hit = 1; hitType = worldMap[mapX][mapY]; }
        }
        
        if (hitType == 9 && doorHP <= 0) { zBuffer[x] = 1000.0; continue; }

        double perpWallDist = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
        zBuffer[x] = perpWallDist;

        // 壁の本来の高さを計算
        int lineHeight = (int)(SCREEN_HEIGHT / (perpWallDist * cos(rayAngle - player->angle)));
        
        int drawStart, drawEnd;

        // ★ここが変更点！ ブロック(9)の場合は高さを調整する
        if (hitType == 9) {
            // ブロックの高さ倍率（0.5 = 半分の高さ。ここを変えると大きさが変わる）
            float blockScale = 0.5f; 
            int blockHeight = (int)(lineHeight * blockScale);
            
            // 本来の壁の下端（地面の位置）を計算
            drawEnd = lineHeight / 2 + SCREEN_HEIGHT / 2 + pitch;
            // 下端からブロックの高さ分だけ上を計算
            drawStart = drawEnd - blockHeight;

        } else {
            // 通常の壁
            drawStart = -lineHeight / 2 + SCREEN_HEIGHT / 2 + pitch;
            drawEnd = lineHeight / 2 + SCREEN_HEIGHT / 2 + pitch;
        }

        // 画面外にはみ出さないようにクリップ処理（念のため追加）
        if (drawStart < 0) drawStart = 0;
        if (drawEnd >= SCREEN_HEIGHT) drawEnd = SCREEN_HEIGHT - 1;

        if (hitType == 9) { 
             if (side == 1) SDL_SetRenderDrawColor(renderer, 0, 0, 150, 255);
             else           SDL_SetRenderDrawColor(renderer, 0, 0, 200, 255);
        } else { 
             // 壁の色分けロジックを少し整理
             int colorR = 160, colorG = 160, colorB = 160;
             if (hitType == 2) { colorR=50; colorG=50; colorB=200; } // 青柱
             else if (hitType == 3) { colorR=200; colorG=50; colorB=50; } // 赤柱
             else if (hitType == 4) { colorR=150; colorG=100; colorB=50; } // 木の壁
             
             if (side == 1) { // 影をつける
                 colorR = colorR * 2 / 3;
                 colorG = colorG * 2 / 3;
                 colorB = colorB * 2 / 3;
             }
             SDL_SetRenderDrawColor(renderer, colorR, colorG, colorB, 255);
        }
        // 線を描画（クリップしたので安全）
        if (drawEnd > drawStart) {
            SDL_RenderDrawLine(renderer, x, drawStart, x, drawEnd);
        }
    }
}

void draw_skill_icon(SDL_Renderer* ren, TTF_Font* font,
                     int x, int y, int size,
                     const char* label, double cd,
                     SDL_Color readyCol)
{
    SDL_Color white = {255,255,255,255};
    SDL_Color transparent = {0,0,0,0};

    if (cd > 0.01)
        SDL_SetRenderDrawColor(ren, 80, 80, 80, 200);
    else
        SDL_SetRenderDrawColor(ren,
            readyCol.r, readyCol.g, readyCol.b, 220);

    SDL_Rect r = {x, y, size, size};
    SDL_RenderFillRect(ren, &r);

    SDL_SetRenderDrawColor(ren, 255,255,255,255);
    SDL_RenderDrawRect(ren, &r);

    // 1文字ラベル
    draw_text_bg(ren, font, label,
        x + size/2 - 6, y + 4,
        white, transparent);

    // クールダウン秒
    if (cd > 0.01) {
        char buf[8];
        sprintf(buf, "%d", (int)ceil(cd));
        draw_text_bg(ren, font, buf,
            x + size/2 - 6, y + size - 14,
            white, transparent);
    }
}

int draw_enemy(SDL_Renderer* renderer, Player* player, Enemy* enemy, SDL_Texture* texture, int enemyHP) {
    if (!enemy->active || enemyHP <= 0) return 0; 

    // ★ ここでステルス中なら敵を描画しない
    if (player->stealth_active) return 0;

    int pitch = (int)(player->z * PITCH_SCALE);
    double spriteX = enemy->x - player->x;
    double spriteY = enemy->y - player->y;
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
    int drawStartY = -spriteHeight / 2 + SCREEN_HEIGHT / 2 + pitch;
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
        SDL_Rect r = {drawStartX, drawStartY, spriteWidth, spriteHeight};
        int mid = (drawStartX + drawEndX)/2;
        if (mid >=0 && mid < SCREEN_WIDTH && dist < zBuffer[mid]) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderFillRect(renderer, &r);
        }
    }
    if (drawStartX < SCREEN_WIDTH && drawEndX > 0) {
        int barW = spriteWidth; int barH = 5;
        int barX = drawStartX; int barY = drawStartY - 10;
        SDL_Rect bg = {barX, barY, barW, barH};
        SDL_SetRenderDrawColor(renderer, 50, 0, 0, 255); SDL_RenderFillRect(renderer, &bg);
        int hpW = (int)((float)barW * (float)enemyHP / MAX_PLAYER_HP);
        SDL_Rect hp = {barX, barY, hpW, barH};
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); SDL_RenderFillRect(renderer, &hp);
    }
    int centerX = SCREEN_WIDTH / 2;
    if (centerX >= drawStartX && centerX <= drawEndX) {
        if (dist < zBuffer[centerX]) {
            double diff = angleToEnemy; 
            if (fabs(diff) < 0.025) return 2; 
            return 1; 
        }
    }
    return 0;
}

int get_target_block(Player* player) {
    float rayX = cos(player->angle), rayY = sin(player->angle);
    float x = player->x, y = player->y;
    int mapX = (int)x, mapY = (int)y;
    float deltaX = fabs(1.0/rayX), deltaY = fabs(1.0/rayY);
    int stepX = (rayX < 0) ? -1 : 1, stepY = (rayY < 0) ? -1 : 1;
    float sideX = (rayX < 0) ? (x - mapX) * deltaX : (mapX + 1.0 - x) * deltaX;
    float sideY = (rayY < 0) ? (y - mapY) * deltaY : (mapY + 1.0 - y) * deltaY;
    for(int i=0; i<20; i++) {
        if(sideX < sideY) { sideX += deltaX; mapX += stepX; }
        else              { sideY += deltaY; mapY += stepY; }
        if(worldMap[mapX][mapY] > 0) return worldMap[mapX][mapY];
    }
    return 0;
}

void draw_ui(SDL_Renderer* ren, SDL_Texture* gunTex, int isFiring, int currentAmmo, int isReloading, 
             int myRole, int doorHP, int gameState, int selfHP, int remainingTime, double skill1_cd, double skill2_cd,double skill3_cd,double skill4_cd, double shield_remain,int hp, int maxHp, const char* hitMsg, int hitTimer,int respawnTime, int killCount, int isStunned) {
    SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);
    SDL_RenderDrawLine(ren, SCREEN_WIDTH/2 - 10, SCREEN_HEIGHT/2, SCREEN_WIDTH/2 + 10, SCREEN_HEIGHT/2);
    SDL_RenderDrawLine(ren, SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 10, SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 10);
    SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);
    SDL_RenderDrawLine(ren, SCREEN_WIDTH/2 - 10, SCREEN_HEIGHT/2, SCREEN_WIDTH/2 + 10, SCREEN_HEIGHT/2);
    SDL_RenderDrawLine(ren, SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 10, SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 10);
    
        // 1. 照準（クロスヘア）の描画
    SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);
    int cx = SCREEN_WIDTH / 2, cy = SCREEN_HEIGHT / 2;
    SDL_RenderDrawLine(ren, cx - 10, cy, cx + 10, cy);
    SDL_RenderDrawLine(ren, cx, cy - 10, cx, cy + 10);

    // 2. HPバーの描画
    int barWidth = 200;
    int barHeight = 20;
    int barX = 20;
    int barY = SCREEN_HEIGHT - 40;

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color transparent = {0, 0, 0, 0};

    // 背景（暗いグレー、完全不透明）
    SDL_Rect bgRect = {barX, barY, barWidth, barHeight};
    SDL_SetRenderDrawColor(ren, 50, 50, 50, 255); 
    SDL_RenderFillRect(ren, &bgRect);
    
    /*
    if (font) {
        // 文字 "HP"
        draw_text_bg(ren, font, "HP", barX, barY - 30, white, transparent);
        
        // HP数値をテキストで表示
        char hpText[32];
        sprintf(hpText, "%d/%d", hp, maxHp);
        draw_text_bg(ren, font, hpText, barX + barWidth + 10, barY, white, transparent);
    }
     */
    
// スキルUIを右下・1文字アイコン化
int iconSize = 36;
int padding  = 12;

int anchorX = SCREEN_WIDTH - padding;
int anchorY = padding;

// 右から左
draw_skill_icon(ren, font,
    anchorX - (iconSize + padding) * 1,
    anchorY, iconSize, "H", skill1_cd,
    (SDL_Color){0,180,0,255});

draw_skill_icon(ren, font,
    anchorX - (iconSize + padding) * 2,
    anchorY, iconSize, "S", skill2_cd,
    (SDL_Color){50,120,255,255});

draw_skill_icon(ren, font,
    anchorX - (iconSize + padding) * 3,
    anchorY, iconSize, "D", skill3_cd,
    (SDL_Color){255,180,50,255});

draw_skill_icon(ren, font,
    anchorX - (iconSize + padding) * 4,
    anchorY, iconSize, "T", skill4_cd,
    (SDL_Color){180,100,255,255});

    // 3. 銃と弾薬の描画

    int gunX = SCREEN_WIDTH - 200, gunY = SCREEN_HEIGHT - 150;
    if (gunTex) {
        int w, h; SDL_QueryTexture(gunTex, NULL, NULL, &w, &h);
        int scale = 3;
        SDL_Rect dst = {SCREEN_WIDTH - (w*scale) - 50, SCREEN_HEIGHT - (h*scale), w*scale, h*scale};
        gunX = dst.x; gunY = dst.y;
        if (isFiring > 0) { dst.y += 20; dst.x -= 10; }
        if (isReloading) { dst.y += 100; }
        SDL_RenderCopy(ren, gunTex, NULL, &dst);
    }
    if (isFiring > (FIRE_ANIM_TIME - FLASH_DURATION)) { 
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD); 
        SDL_SetRenderDrawColor(ren, 255, 200, 50, 255);
        SDL_Rect flash = {gunX - 20, gunY - 30, 80, 80}; 
        SDL_RenderFillRect(ren, &flash);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }

    SDL_Color red = {255, 50, 50, 255};
    SDL_Color blue = {100, 100, 255, 255};
    SDL_Color green = {50, 255, 50, 255};
    SDL_Color yellow = {255, 255, 0, 255}; 
    SDL_Color magenta = {255, 0, 255, 255}; 
    SDL_Color bgCol = {0, 0, 0, 150}; 

    char msg[64];
    
    if (respawnTime > 0) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 50, 0, 0, 180); 
        SDL_Rect screen = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(ren, &screen);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

        sprintf(msg, "RESPAWN IN %d", respawnTime);
        draw_text_bg(ren, fontBig, msg, SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 - 50, white, bgCol);
        draw_text_bg(ren, font, "YOU ARE DEAD", SCREEN_WIDTH/2 - 80, SCREEN_HEIGHT/2 - 100, red, bgCol);
        return;
    }

    if (gameState == GS_WAITING) {
        draw_text_bg(ren, fontBig, "Waiting for Player...", SCREEN_WIDTH/2 - 200, SCREEN_HEIGHT/2 - 50, white, bgCol);
        draw_text_bg(ren, font, "(Press P to Force Start)", SCREEN_WIDTH/2 - 120, SCREEN_HEIGHT/2 + 20, white, bgCol);
        return; 
    } else if (gameState == GS_COUNTDOWN) {
        if (remainingTime > 0) sprintf(msg, "%d", remainingTime);
        else sprintf(msg, "START!");
        draw_text_bg(ren, fontBig, msg, SCREEN_WIDTH/2 - 20, SCREEN_HEIGHT/2 - 50, red, bgCol);
        return; 
    } else if (gameState == GS_SETUP) {
        sprintf(msg, "SETUP PHASE: %d", remainingTime);
        draw_text_bg(ren, fontBig, msg, SCREEN_WIDTH/2 - 200, 20, (remainingTime < 10)?red:white, bgCol);
        
        if (myRole == 1) {
            draw_text_bg(ren, font, "DEFENDER: Move Block [F]", SCREEN_WIDTH/2 - 150, 80, blue, bgCol);
        } else {
            // ★変更: アタッカーは動けないことを明示
            draw_text_bg(ren, font, "ATTACKER: Wait... (Locked)", SCREEN_WIDTH/2 - 120, 80, red, bgCol);
        }
    } else if (gameState == GS_PLAYING) {
        sprintf(msg, "TIME: %d", remainingTime);
        draw_text_bg(ren, fontBig, msg, SCREEN_WIDTH/2 - 100, 20, (remainingTime < 10)?red:white, bgCol);
    }

    if (myRole == 0) {
        sprintf(msg, "ATTACKER");
        draw_text_bg(ren, fontBig, msg, 20, 20, red, bgCol);
    } else {
        sprintf(msg, "DEFENDER");
        draw_text_bg(ren, fontBig, msg, 20, 20, blue, bgCol);
    }
    sprintf(msg, "Door HP: %d", doorHP);
    draw_text_bg(ren, font, msg, 20, 70, white, bgCol);
    
    sprintf(msg, "HP: %d", selfHP);
    draw_text_bg(ren, fontBig, msg, 20, SCREEN_HEIGHT - 70, (selfHP < 30)?red:green, bgCol);

    if (isReloading) {
        draw_text_bg(ren, font, "RELOADING...", SCREEN_WIDTH - 220, SCREEN_HEIGHT - 60, red, bgCol);
    } else {
        sprintf(msg, "%d / %d", currentAmmo, MAX_AMMO);
        draw_text_bg(ren, fontBig, msg, SCREEN_WIDTH - 200, SCREEN_HEIGHT - 70, (currentAmmo==0)?red:white, bgCol);
    }

    // 必殺技ゲージ（防衛側のみ表示）
    if (myRole == 1) { 
        if (killCount >= 3) {
            sprintf(msg, "ULTIMATE READY [Q]");
            draw_text_bg(ren, fontBig, msg, SCREEN_WIDTH - 320, SCREEN_HEIGHT - 130, yellow, bgCol);
        } else {
            sprintf(msg, "Ult: %d / 3", killCount);
            draw_text_bg(ren, font, msg, SCREEN_WIDTH - 200, SCREEN_HEIGHT - 130, white, bgCol);
        }
    }

    // スタン状態の警告（全員表示される可能性があるため）
    if (isStunned) {
         draw_text_bg(ren, fontBig, "STUNNED!", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 - 20, magenta, bgCol);
         draw_text_bg(ren, font, "CANNOT MOVE", SCREEN_WIDTH/2 - 80, SCREEN_HEIGHT/2 + 40, white, bgCol);
    }

    if (gameState == GS_WIN_ATTACKER) { 
        if (myRole == 0) draw_text_bg(ren, fontBig, "VICTORY!!", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 - 50, red, bgCol);
        else             draw_text_bg(ren, fontBig, "DEFEAT...", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 - 50, blue, bgCol);
    } else if (gameState == GS_WIN_DEFENDER) { 
        if (myRole == 1) draw_text_bg(ren, fontBig, "VICTORY!!", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 - 50, blue, bgCol);
        else             draw_text_bg(ren, fontBig, "DEFEAT...", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 - 50, red, bgCol);
    }
}

int main(int argc, char **argv) {
    int sock; struct sockaddr_in srvaddr;
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) return 1;
    make_socket_nonblocking(sock);
    memset(&srvaddr, 0, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET; srvaddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &srvaddr.sin_addr);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    IMG_Init(IMG_INIT_PNG); TTF_Init();

    SDL_Window *win = SDL_CreateWindow("FPS Client", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    SDL_Texture* enemyTex = IMG_LoadTexture(ren, "enemy.png");
    SDL_Texture* gunTex = IMG_LoadTexture(ren, "gun.png");
    font = TTF_OpenFont("font.ttf", 24); 
    fontBig = TTF_OpenFont("font.ttf", 48); 

    SDL_SetRelativeMouseMode(SDL_TRUE);

    Player player; player_init(&player);
    player.z = 0.0; player.vz = 0.0; 

    Enemy enemy = {0};
    uint32_t seq = 0;
    int running = 1;
    Uint64 LAST = SDL_GetPerformanceCounter();

    int isFiring = 0;
    int currentAmmo = MAX_AMMO;
    int isReloading = 0;
    int reloadTimer = 0;

    int myRole = -1;
    int doorHP = MAX_DOOR_HP;
    int gameState = GS_WAITING; 
    int selfHP = MAX_PLAYER_HP;
    int enemyHP = MAX_PLAYER_HP;
    int hitTargetStatus = 0; 
    int remainingTime = 0; 
    int respawnTime = 0; 
    int wasDead = 0; 
    int forceStart = 0; 
    int sendFKey = 0; 
    int myKillCount = 0;
    int isStunned = 0;
    
    // ★初期位置セットフラグ
    int initialPosSet = 0;

    double skill1_cd_timer = 0.0; 
    double skill2_cd_timer = 0.0; 
    double skill3_cd_timer = 0.0; 
    double skill4_cd_timer = 0.0;

    /* ★★★ HIT表示用（ここに追加）★★★ */
   char hitMsg[64] = "";
   int hitTimer = 0;

    while (running) {
        Uint64 NOW = SDL_GetPerformanceCounter();
        double dt = (double)(NOW - LAST) / (double)SDL_GetPerformanceFrequency();
        LAST = NOW;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            
            if (gameState == GS_WAITING) {
                if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_P) {
                    forceStart = 1;
                }
            }
            
            // ★操作許可判定: 試合中 OR (セットアップ中かつDefender)
            int canControl = 0;
            if (gameState == GS_PLAYING) canControl = 1;
            else if (gameState == GS_SETUP && myRole == 1) canControl = 1;

            // スタン中は操作不能にする (キー入力全般を無視する場合)
            // ただし、視点移動を許すかどうかによる。今回は「移動も操作も不可」とする
            if (isStunned) canControl = 0;
            
            if (canControl && selfHP > 0 && respawnTime == 0) {
                if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_J) {
                    if (!isReloading && currentAmmo > 0) {
                        isFiring = FIRE_ANIM_TIME; 
                        currentAmmo--;
                    }
                }
                
                if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                    if (player.z <= 0) player.vz = JUMP_POWER;
                }

            // ★修正: F1キーでヒール (アタッカー専用)
                if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_F1) {
                    if (myRole == 0) { // アタッカーのみ
                        if (skill1_cd_timer <= 0.0) {
                            skill1_cd_timer = SKILL1_COOLDOWN;
                            SDL_Log("Heal Skill Used!");
                        } else {
                            SDL_Log("Heal on cooldown: %.1f", skill1_cd_timer);
                        }
                    }
                }

                if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_F) {
                    sendFKey = 1;
                }

            // F2: シールド発動
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_F2) {
                if (skill2_cd_timer <= 0.0) {
                    skill_shield_activate(&player);
                    skill2_cd_timer = SKILL2_COOLDOWN;
                } else {
                    SDL_Log("F2 on cooldown: %.2f", skill2_cd_timer);
                }
            }

            // F3: ダッシュ
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_F3) {
                if (skill3_cd_timer <= 0.0) {
                    skill_dash(&player);
                    skill3_cd_timer = SKILL3_COOLDOWN;
                } else {
                    SDL_Log("F3 on cooldown: %.2f", skill3_cd_timer);
                }
            }

            // F4: ステルス
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_F4) {
                if (skill4_cd_timer <= 0.0) {
                    skill_stealth(&player);
                    skill4_cd_timer = SKILL4_COOLDOWN;
                } else {
                    SDL_Log("F4 on cooldown: %.2f", skill4_cd_timer);
                    }
                }

            // スキル・エスクード（Eキー） 
                if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_E) {
                if (myRole == 1) { // 防衛側のみ実行
                    skill_escudo(&player);
                }
        }
    }
}
    

        // ★移動許可判定
        int canMove = 0;
        if (gameState == GS_PLAYING) canMove = 1;
        else if (gameState == GS_SETUP && myRole == 1) canMove = 1;

        // ★追加: スタン中は移動不可
        if (isStunned) canMove = 0;

        if (canMove && selfHP > 0 && respawnTime == 0) {
            int mouseX, mouseY;
            SDL_GetRelativeMouseState(&mouseX, &mouseY);
            player.angle += mouseX * 0.005; 
            const Uint8 *key_state = SDL_GetKeyboardState(NULL);
            if (key_state[SDL_SCANCODE_R] && !isReloading && currentAmmo < MAX_AMMO) {
                isReloading = 1; reloadTimer = RELOAD_TIME;
            }
            if (key_state[SDL_SCANCODE_ESCAPE]) running = 0;

            float moveSpeed = 3.0 * dt;
            if (player.dash_active) {
                moveSpeed *= 1.5f;
            }
            
            if (key_state[SDL_SCANCODE_W]) {
                float nx = player.x + cos(player.angle)*moveSpeed; float ny = player.y + sin(player.angle)*moveSpeed;
                if(worldMap[(int)nx][(int)player.y]==0) player.x = nx; if(worldMap[(int)player.x][(int)ny]==0) player.y = ny;
            }
            if (key_state[SDL_SCANCODE_S]) {
                float nx = player.x - cos(player.angle)*moveSpeed; float ny = player.y - sin(player.angle)*moveSpeed;
                if(worldMap[(int)nx][(int)player.y]==0) player.x = nx; if(worldMap[(int)player.x][(int)ny]==0) player.y = ny;
            }
            if (key_state[SDL_SCANCODE_A]) {
                float nx = player.x + cos(player.angle - M_PI/2)*moveSpeed; float ny = player.y + sin(player.angle - M_PI/2)*moveSpeed;
                if(worldMap[(int)nx][(int)player.y]==0) player.x = nx; if(worldMap[(int)player.x][(int)ny]==0) player.y = ny;
            }
            if (key_state[SDL_SCANCODE_D]) {
                float nx = player.x + cos(player.angle + M_PI/2)*moveSpeed; float ny = player.y + sin(player.angle + M_PI/2)*moveSpeed;
                if(worldMap[(int)nx][(int)player.y]==0) player.x = nx; if(worldMap[(int)player.x][(int)ny]==0) player.y = ny;
            }
            
            player.vz -= GRAVITY * dt;
            player.z += player.vz * dt;
            if (player.z < 0) { player.z = 0; player.vz = 0; }

            if (isReloading) {
                reloadTimer--;
                if (reloadTimer <= 0) { isReloading = 0; currentAmmo = MAX_AMMO; }
            }
        } else {
             const Uint8 *key_state = SDL_GetKeyboardState(NULL);
             if (key_state[SDL_SCANCODE_ESCAPE]) running = 0;
        }
        
        if (skill1_cd_timer > 0.0) skill1_cd_timer -= dt;
        if (skill2_cd_timer > 0.0) skill2_cd_timer -= dt;
        if (skill3_cd_timer > 0.0) skill3_cd_timer -= dt;
        if (skill4_cd_timer > 0.0) skill4_cd_timer -= dt;
        
        if (skill1_cd_timer < 0.0) skill1_cd_timer = 0.0;
        if (skill2_cd_timer < 0.0) skill2_cd_timer = 0.0;
        if (skill3_cd_timer < 0.0) skill3_cd_timer = 0.0;
        if (skill4_cd_timer < 0.0) skill4_cd_timer = 0.0;

        /* 追加 */
        skill_update(&player, (float)dt);

        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        
        int pitch = (int)(player.z * PITCH_SCALE);
        
        draw_floor_ceiling(ren, pitch); 
        draw_walls(ren, &player, doorHP); 
        hitTargetStatus = draw_enemy(ren, &player, &enemy, enemyTex, enemyHP);

        /* 追加*/
        float shield_remain = skill_get_shield_time_remaining(&player);

        draw_ui(ren, gunTex, isFiring, currentAmmo, isReloading, myRole, doorHP, gameState, selfHP, remainingTime, skill1_cd_timer, skill2_cd_timer, skill3_cd_timer,skill4_cd_timer,shield_remain, player.hp, player.maxHp, hitMsg, hitTimer,respawnTime, myKillCount, isStunned);
        if (hitTimer > 0) hitTimer--;

        SDL_RenderPresent(ren);

        pkt_t out; 
        out.seq = htonl(seq++); 
        out.x = player.x; out.y = player.y; out.angle = player.angle; 
        out.btn = 0;
        
        if (forceStart) { out.btn |= 16; forceStart = 0; }
        if (sendFKey) { out.btn |= 32; sendFKey = 0; } 
        
        // ★追加: Qキーで必殺技発動リクエスト (防衛側のみ)
        const Uint8 *ks = SDL_GetKeyboardState(NULL);
        if (ks[SDL_SCANCODE_Q]) {
            out.btn |= 128; // 必殺技ビット
        }

        // 防衛側 (Role 1) は Eキー でエスクード (Bit 64)
        if (myRole == 1 && ks[SDL_SCANCODE_E]) {
            out.btn |= 64; 
        }

        // アタッカー (Role 0) は F1キー でヒール (Bit 64)
        if (myRole == 0 && ks[SDL_SCANCODE_F1]) {
            out.btn |= 64; 
        }

        if (isFiring == FIRE_ANIM_TIME) {
            out.btn |= 1; 
            if (hitTargetStatus == 2) out.btn |= 8;      
            else if (hitTargetStatus == 1) out.btn |= 4; 
            if (get_target_block(&player) == 9) out.btn |= 2;
        }
        sendto(sock, &out, sizeof(out), 0, (struct sockaddr*)&srvaddr, sizeof(srvaddr));

        char buf[BUF_SIZE]; struct sockaddr_in from; socklen_t fromlen = sizeof(from);
        while (recvfrom(sock, buf, BUF_SIZE, 0, (struct sockaddr*)&from, &fromlen) > 0) {
             server_pkt_t in; memcpy(&in, buf, sizeof(server_pkt_t));
             if (in.active) {
                 enemy.x = in.x; enemy.y = in.y; enemy.angle = in.angle; enemy.active = 1;
             } else {
                 enemy.active = 0;
             }
             doorHP = in.doorHP;
             myRole = in.role;
             gameState = in.gameState;
             
             // サーバーから撃破数とスタン情報を受け取る
             myKillCount = in.killCount;
             isStunned = in.isStunned;

             // 初期位置設定（役割が決まったら一度だけ飛ぶ）
             if (!initialPosSet && myRole != -1) {
                 if (myRole == 0) { // Attacker -> 左上
                     player.x = 1.5; player.y = 1.5; 
                 } else { // Defender -> 右下
                     player.x = 20.5; player.y = 20.5; 
                     player.angle = M_PI; // 向きを反転
                 }
                 initialPosSet = 1;
             }

             selfHP = in.selfHP;
             enemyHP = in.enemyHP;
             remainingTime = in.remainingTime;
             
             respawnTime = in.respawnTime;
             if (respawnTime > 0) {
                 wasDead = 1;
             } else if (wasDead) {
                 wasDead = 0;
                 if (myRole == 0) { player.x = 1.5; player.y = 1.5; }
                 else             { player.x = 20.5; player.y = 20.5; }
             }

             update_block_position(in.blockX, in.blockY);
        }
        
        if (isFiring > 0) isFiring--;
    }

    if(enemyTex) SDL_DestroyTexture(enemyTex); if(gunTex) SDL_DestroyTexture(gunTex);
    if(font) TTF_CloseFont(font); if(fontBig) TTF_CloseFont(fontBig);
    IMG_Quit(); TTF_Quit(); close(sock); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
