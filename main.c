#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h> // TTFライブラリのヘッダ
#include "player.h"

int main(int argc, char* argv[]) {
    // 1. SDLとSDL_ttfの初期化
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    if (TTF_Init() == -1) return 1;

    // 2. ウィンドウとレンダラー（描画担当）の作成
    SDL_Window* window = SDL_CreateWindow("2D Game Foundation", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // 3. プレイヤーの作成と初期化
    Player player;
    player_init(&player, renderer);

    // 4. ゲームループ
    int running = 1;
    while (running) {
        SDL_Event event;
        // イベント処理 (例: ウィンドウの×ボタンが押されたら終了)
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
        }

        // --- 入力・更新・描画 ---
        // (1) 入力
        const Uint8* key_state = SDL_GetKeyboardState(NULL);
        player_handle_input(&player, key_state);

        // (2) 描画の準備 (画面を黒でクリア)
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // (3) 描画
        player_render(&player, renderer); // プレイヤーを描画

        // (4) 画面に反映
        SDL_RenderPresent(renderer);

        SDL_Delay(16); // 簡易的な60FPS維持
    }

    // 5. 終了処理
    player_destroy(&player);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}