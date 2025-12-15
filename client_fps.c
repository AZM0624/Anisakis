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

// ★★★ サーバーPCのIPアドレスに書き換えてください ★★★
#define SERVER_IP "192.168.1.15" 

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
} server_pkt_t;
#pragma pack(pop)

typedef struct {
    float x, y, angle;
    int active;
} Enemy;

double zBuffer[SCREEN_WIDTH];
TTF_Font *font = NULL;
TTF_Font *fontBig = NULL;

int make_socket_nonblocking(int s) {
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(s, F_SETFL, flags | O_NONBLOCK);
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

void draw_floor_ceiling(SDL_Renderer* renderer) {
    SDL_Rect ceiling = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT / 2};
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255); 
    SDL_RenderFillRect(renderer, &ceiling);
    SDL_Rect floor = {0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT / 2};
    SDL_SetRenderDrawColor(renderer, 70, 70, 70, 255); 
    SDL_RenderFillRect(renderer, &floor);
}

void draw_walls(SDL_Renderer* renderer, Player* player, int doorHP) {
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
        
        int hit = 0, side = 0, hitType = 0; 
        while (hit == 0) {
            if (sideDistX < sideDistY) { sideDistX += deltaDistX; mapX += stepX; side = 0; }
            else                       { sideDistY += deltaDistY; mapY += stepY; side = 1; }
            if (worldMap[mapX][mapY] > 0) {
                hit = 1; hitType = worldMap[mapX][mapY];
            }
        }
        
        if (hitType == 9 && doorHP <= 0) { zBuffer[x] = 1000.0; continue; }

        double perpWallDist = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
        zBuffer[x] = perpWallDist;

        int lineHeight = (int)(SCREEN_HEIGHT / (perpWallDist * cos(rayAngle - player->angle)));
        int drawStart = -lineHeight / 2 + SCREEN_HEIGHT / 2;
        if (drawStart < 0) drawStart = 0;
        int drawEnd = lineHeight / 2 + SCREEN_HEIGHT / 2;
        if (drawEnd >= SCREEN_HEIGHT) drawEnd = SCREEN_HEIGHT - 1;

        if (hitType == 9) { 
             if (side == 1) SDL_SetRenderDrawColor(renderer, 0, 0, 150, 255);
             else           SDL_SetRenderDrawColor(renderer, 0, 0, 200, 255);
        } else { 
             int colorVal = (side == 1) ? 100 : 160;
             SDL_SetRenderDrawColor(renderer, colorVal, colorVal, colorVal, 255);
        }
        SDL_RenderDrawLine(renderer, x, drawStart, x, drawEnd);
    }
}

int draw_enemy(SDL_Renderer* renderer, Player* player, Enemy* enemy, SDL_Texture* texture, int enemyHP) {
    if (!enemy->active || enemyHP <= 0) return 0; 
    
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
    int drawStartY = -spriteHeight / 2 + SCREEN_HEIGHT / 2;
    
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
        int barW = spriteWidth;
        int barH = 5;
        int barX = drawStartX;
        int barY = drawStartY - 10;
        SDL_Rect bg = {barX, barY, barW, barH};
        SDL_SetRenderDrawColor(renderer, 50, 0, 0, 255);
        SDL_RenderFillRect(renderer, &bg);
        int hpW = (int)((float)barW * (float)enemyHP / MAX_PLAYER_HP);
        SDL_Rect hp = {barX, barY, hpW, barH};
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_RenderFillRect(renderer, &hp);
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
             int myRole, int doorHP, int gameState, int selfHP) {
    SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);
    SDL_RenderDrawLine(ren, SCREEN_WIDTH/2 - 10, SCREEN_HEIGHT/2, SCREEN_WIDTH/2 + 10, SCREEN_HEIGHT/2);
    SDL_RenderDrawLine(ren, SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 10, SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 10);

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

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color red = {255, 50, 50, 255};
    SDL_Color blue = {100, 100, 255, 255};
    SDL_Color green = {50, 255, 50, 255};
    SDL_Color bgCol = {0, 0, 0, 150}; 

    char msg[64];
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

    if (gameState != 0) { 
        if (gameState == 1) { 
             if (myRole == 0) draw_text_bg(ren, fontBig, "VICTORY!!", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 - 50, red, bgCol);
             else             draw_text_bg(ren, fontBig, "DEFEAT...", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 - 50, blue, bgCol);
        } else if (gameState == 2) { 
             if (myRole == 1) draw_text_bg(ren, fontBig, "VICTORY!!", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 - 50, blue, bgCol);
             else             draw_text_bg(ren, fontBig, "DEFEAT...", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 - 50, red, bgCol);
        }
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
    int gameState = 0;
    int selfHP = MAX_PLAYER_HP;
    int enemyHP = MAX_PLAYER_HP;
    int hitTargetStatus = 0; 

    while (running) {
        Uint64 NOW = SDL_GetPerformanceCounter();
        double dt = (double)(NOW - LAST) / (double)SDL_GetPerformanceFrequency();
        LAST = NOW;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_J) {
                if (!isReloading && currentAmmo > 0 && gameState == 0 && selfHP > 0) {
                    isFiring = FIRE_ANIM_TIME; 
                    currentAmmo--;
                }
            }
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_E) {
                skill_escudo(&player);
            }
        }

        if (gameState == 0 && selfHP > 0) {
            int mouseX, mouseY;
            SDL_GetRelativeMouseState(&mouseX, &mouseY);
            player.angle += mouseX * 0.005; 
            const Uint8 *key_state = SDL_GetKeyboardState(NULL);
            if (key_state[SDL_SCANCODE_R] && !isReloading && currentAmmo < MAX_AMMO) {
                isReloading = 1; reloadTimer = RELOAD_TIME;
            }
            if (key_state[SDL_SCANCODE_ESCAPE]) running = 0;

            float moveSpeed = 3.0 * dt;
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
            if (isReloading) {
                reloadTimer--;
                if (reloadTimer <= 0) { isReloading = 0; currentAmmo = MAX_AMMO; }
            }
        } else {
             const Uint8 *key_state = SDL_GetKeyboardState(NULL);
             if (key_state[SDL_SCANCODE_ESCAPE]) running = 0;
        }

        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        draw_floor_ceiling(ren);
        draw_walls(ren, &player, doorHP); 
        hitTargetStatus = draw_enemy(ren, &player, &enemy, enemyTex, enemyHP);

        draw_ui(ren, gunTex, isFiring, currentAmmo, isReloading, myRole, doorHP, gameState, selfHP);

        SDL_RenderPresent(ren);

        pkt_t out; 
        out.seq = htonl(seq++); 
        out.x = player.x; out.y = player.y; out.angle = player.angle; 
        out.btn = 0;
        
        if (isFiring == FIRE_ANIM_TIME) {
            out.btn |= 1; 
            if (hitTargetStatus == 2) out.btn |= 8;      
            else if (hitTargetStatus == 1) out.btn |= 4; 
            
            if (get_target_block(&player) == 9) {
                out.btn |= 2;
            }
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
             selfHP = in.selfHP;
             enemyHP = in.enemyHP;
        }

        if (isFiring > 0) isFiring--;
    }

    if(enemyTex) SDL_DestroyTexture(enemyTex); if(gunTex) SDL_DestroyTexture(gunTex);
    if(font) TTF_CloseFont(font); if(fontBig) TTF_CloseFont(fontBig);
    IMG_Quit(); TTF_Quit(); close(sock); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}