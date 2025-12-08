#ifndef PLAYER_H
#define PLAYER_H

#include <SDL2/SDL.h>
#include "map.h"

// プレイヤーが持つ情報（構造体）
typedef struct {
    double x, y;       // 座標 (2Dマップ上の位置)
    double angle;    // 向いている角度 (ラジアン)
    double fov;      // 視野角 (ラジアン)
    double moveSpeed;
    double rotSpeed; // 回転速度

    int hp;        // hp
    int maxHp;     // 最大hp
} Player;

// プレイヤーに対する操作（関数）
void player_init(Player* player);
void player_handle_input(Player* player, const Uint8* key_state, double deltaTime);

// (render と destroy は main.c に移動)

#endif