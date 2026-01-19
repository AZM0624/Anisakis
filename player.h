#ifndef PLAYER_H
#define PLAYER_H

#include <SDL2/SDL.h>
#include "map.h"

// プレイヤーが持つ情報（構造体）
typedef struct {
    double x, y;       // 座標 (2Dマップ上の位置)
    double angle;      // 向いている角度 (ラジアン)
    double fov;        // 視野角 (ラジアン)
    double moveSpeed;
    double rotSpeed;   // 回転速度

    // ★追加: ジャンプ用の高さと速度
    double z;          // 高さ（0が地面）
    double vz;         // 垂直方向の速度

    int hp;            // hp
    int maxHp;         // 最大hp
} Player;

// プレイヤーに対する操作（関数）
void player_init(Player* player);
void player_handle_input(Player* player, const Uint8* key_state, double deltaTime);

#endif