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
    int hp;        // hp
    int maxHp;     // 最大hp

        /* ”追加”　スキル関連（シールド） */
    int shield_active;     // 1 = シールド有効, 0 = 無効
    float shield_timer;    // シールドの残り時間（秒）
    // ★追加: ジャンプ用の高さと速度
    double z;          // 高さ（0が地面）
    double vz;         // 垂直方向の速度
} Player;

// プレイヤーに対する操作（関数）
void player_init(Player* player);
void player_handle_input(Player* player, const Uint8* key_state, double deltaTime);

/* ”追加”　ダメージ処理：シールドが有効ならダメージを無効化する実装例 */
/* 戻り値: 1 = ダメージ適用された, 0 = シールドで無効化された */
int player_take_damage(Player* player, int dmg);

// (render と destroy は main.c に移動)


#endif