#include "player.h"
#include <SDL2/SDL_ttf.h> // 文字を描画するために追加

// プレイヤーを初期化する
void player_init(Player* player, SDL_Renderer* renderer) {
    player->x = 100.0f;
    player->y = 100.0f;
    player->w = 32;
    player->h = 32;
    player->speed = 200.0f; // 1秒間に200ピクセル動く

    // --- 今回は画像ではなく、「P」という文字を描画してテクスチャにする ---
    TTF_Font* font = TTF_OpenFont("arial.ttf", 24); // PCに入っているフォントを指定
    SDL_Color color = {255, 255, 255, 255}; // 白色
    SDL_Surface* surface = TTF_RenderText_Solid(font, "P", color);
    player->texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    TTF_CloseFont(font);
    // ---
}

// キーボード入力に応じてプレイヤーを動かす
void player_handle_input(Player* player, const Uint8* key_state) {
    if (key_state[SDL_SCANCODE_W]) {
        player->y -= player->speed * (1.0f / 60.0f); // 60FPSを想定した移動量
    }
    if (key_state[SDL_SCANCODE_S]) {
        player->y += player->speed * (1.0f / 60.0f);
    }
    if (key_state[SDL_SCANCODE_A]) {
        player->x -= player->speed * (1.0f / 60.0f);
    }
    if (key_state[SDL_SCANCODE_D]) {
        player->x += player->speed * (1.0f / 60.0f);
    }
}

// プレイヤーを描画する
void player_render(Player* player, SDL_Renderer* renderer) {
    SDL_Rect dest_rect = {(int)player->x, (int)player->y, player->w, player->h};
    SDL_RenderCopy(renderer, player->texture, NULL, &dest_rect);
}

// プレイヤーが使ったメモリを解放する
void player_destroy(Player* player) {
    SDL_DestroyTexture(player->texture);
}