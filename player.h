#ifndef PLAYER_H
#define PLAYER_H

#include <SDL2/SDL.h>

// プレイヤーが持つ情報（構造体）
typedef struct {
    float x, y;       // 座標
    int w, h;         // 幅と高さ
    float speed;      // 移動速度
    SDL_Texture* texture; // プレイヤーの画像（今回は文字を描画）
} Player;

// プレイヤーに対する操作（関数）
void player_init(Player* player, SDL_Renderer* renderer);
void player_handle_input(Player* player, const Uint8* key_state);
void player_update(Player* player);
void player_render(Player* player, SDL_Renderer* renderer);
void player_destroy(Player* player);

#endif