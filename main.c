#include <stdio.h>
#include <math.h>
#include <SDL2/SDL.h>
#include "player.h"
#include "map.h" // マップ情報
#include "skill.h" // スキル情報
#include <SDL2/SDL_mixer.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

// デルタタイム計算用のグローバル変数
Uint64 LAST = 0;
double deltaTime = 0;

void draw_walls(SDL_Renderer* renderer, Player* player) {
    // 画面のピクセル列 (x) ごとにループ
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        // 1. レイ（光線）の角度を計算
        // -fov/2 から +fov/2 まで、スクリーン幅に応じてスキャン
        double cameraX = (double)x / (double)SCREEN_WIDTH * 2.0 - 1.0; // -1 to +1
        double rayAngle = player->angle + atan(cameraX * tan(player->fov / 2.0));
        
        double rayDirX = cos(rayAngle);
        double rayDirY = sin(rayAngle);

        // 2. DDA (Digital Differential Analysis) アルゴリズムで壁に当たるまでレイを飛ばす
        int mapX = (int)player->x;
        int mapY = (int)player->y;

        // レイのステップごとの移動距離
        double deltaDistX = (rayDirX == 0) ? 1e30 : fabs(1.0 / rayDirX);
        double deltaDistY = (rayDirY == 0) ? 1e30 : fabs(1.0 / rayDirY);

        // レイの現在地から次のマス境界までの距離
        double sideDistX, sideDistY;

        int stepX, stepY; // 進む方向 (+1 or -1)

        if (rayDirX < 0) {
            stepX = -1;
            sideDistX = (player->x - mapX) * deltaDistX;
        } else {
            stepX = 1;
            sideDistX = (mapX + 1.0 - player->x) * deltaDistX;
        }
        if (rayDirY < 0) {
            stepY = -1;
            sideDistY = (player->y - mapY) * deltaDistY;
        } else {
            stepY = 1;
            sideDistY = (mapY + 1.0 - player->y) * deltaDistY;
        }

        int hit = 0;
        int side = 0; // 0=X軸の壁, 1=Y軸の壁
        int wallType = 0; // 壁の種類を保存する変数（エスクード用）
        
        // 3. レイを飛ばす
        while (hit == 0) {
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }
            // 壁に当たったかチェック
            if (worldMap[mapX][mapY] > 0) hit = 1;
        }

        // 4. 壁までの距離を計算 (魚眼レンズ補正)
        double perpWallDist;
        if (side == 0) {
            perpWallDist = (sideDistX - deltaDistX);
        } else {
            perpWallDist = (sideDistY - deltaDistY);
        }
        // プレイヤーの角度とレイの角度の差を使って魚眼レンズ補正
        perpWallDist *= cos(rayAngle - player->angle);


        // 5. 画面に描画する壁の高さを計算
        int lineHeight = (int)(SCREEN_HEIGHT / perpWallDist);

        int drawStart = -lineHeight / 2 + SCREEN_HEIGHT / 2;
        if (drawStart < 0) drawStart = 0;
        int drawEnd = lineHeight / 2 + SCREEN_HEIGHT / 2;
        if (drawEnd >= SCREEN_HEIGHT) drawEnd = SCREEN_HEIGHT - 1;

        // 6. 壁の色を決定（X軸かY軸かで色を変えて立体感を出す）
        if (wallType == 2) {
            // エスクード（青緑色)
            if (side == 1) {
                SDL_SetRenderDrawColor(renderer, 0, 100, 100, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 0, 150, 150, 255);
            }
        } else {
            // 通常の壁（灰色）
            if (side == 1) {
                SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); // 暗い色
            } else {
                SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255); // 明るい色
            }
        }
        // 7. 垂直線を描画
        SDL_RenderDrawLine(renderer, x, drawStart, x, drawEnd);
    }
}

void draw_floor_ceiling(SDL_Renderer* renderer) {
    // 天井 (画面の上半分)
    SDL_Rect ceiling = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT / 2};
    SDL_SetRenderDrawColor(renderer, 50, 50, 80, 255); // 暗い青
    SDL_RenderFillRect(renderer, &ceiling);

    // 床 (画面の下半分)
    SDL_Rect floor = {0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT / 2};
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255); // 暗いグレー
    SDL_RenderFillRect(renderer, &floor);
}

Mix_Chunk *gunSound = NULL;

int main(int argc, char* argv[]) {
    // 1. SDLの初期化
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;

    // 【追加】SDL_mixerの初期化（周波数44100Hz, ステレオ）
    // MP3を使用する場合、Mix_InitでMIX_INIT_MP3を指定するのが確実ですが、
    // 最近のバージョンではMix_OpenAudioだけで再生できる場合も多いです。
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("SDL_mixer Error: %s\n", Mix_GetError());
        return 1;
    }

    // 【追加】音声ファイルの読み込み
    // SE（効果音）として扱うため Mix_LoadWAV を使用します（MP3も読み込めます）
    // ファイル名「拳銃を撃つ.mp3」は実行ファイルと同じ場所に置いてください
    gunSound = Mix_LoadWAV("拳銃を撃つ.mp3");
    if (gunSound == NULL) {
        printf("Failed to load sound! SDL_mixer Error: %s\n", Mix_GetError());
    }

    // 2. ウィンドウとレンダラーの作成
    SDL_Window* window = SDL_CreateWindow("Pseudo-3D FPS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // 3. プレイヤーの作成と初期化
    Player player;
    player_init(&player);

    LAST = SDL_GetPerformanceCounter(); // デルタタイム初期化

    // 4. ゲームループ
    int running = 1;
    while (running) {
        // デルタタイムの計算 (フレームレートに依存しない移動のため)
        Uint64 NOW = SDL_GetPerformanceCounter();
        deltaTime = (double)(NOW - LAST) / (double)SDL_GetPerformanceFrequency();
        LAST = NOW;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
            // 【追加】発砲イベント（マウス左クリック または スペースキー）
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    // チャンネル-1（空いている場所）で再生、ループなし(0)
                    if (gunSound != NULL) {
                        Mix_PlayChannel(-1, gunSound, 0);
                    }
                }
            }
            else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_SPACE) {
                     if (gunSound != NULL) {
                        Mix_PlayChannel(-1, gunSound, 0);
                    }
                }
            }
        }
        

        // --- 入力・更新・描画 ---
        // (1) 入力
        const Uint8* key_state = SDL_GetKeyboardState(NULL);
        player_handle_input(&player, key_state, deltaTime);

        // (2) 描画の準備 (画面をクリア)
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // (3) 描画 (床・天井)
        draw_floor_ceiling(renderer);

        // (4) 描画 (壁 - Raycasting)
        draw_walls(renderer, &player);

        // (5) 画面に反映
        SDL_RenderPresent(renderer);
    }

    // 5. 終了処理
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}