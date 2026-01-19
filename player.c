#include "player.h"
#include <math.h>
#include <string.h> //追加
#include <SDL2/SDL.h> //追加

// プレイヤーを初期化する
void player_init(Player* player) {
    player->x = 12.0f; // マップの中央付近 (2Dマップ上の座標)
    player->y = 12.0f;
    player->angle = 0.0; // Y軸マイナス方向（上）を向く
    player->fov = M_PI / 3.0; // 60度の視野角
    player->moveSpeed = 3.0;  // 1秒間に3タイル動く
    player->rotSpeed = 2.0;   // 1秒間に2ラジアン（約114度）回転

    player->maxHp = 100; // 最大hpを100に設定
    player->hp = 100;

    player->shield_active = 0; //追加
    player->shield_timer = 0.0f; //追加
}

// キーボード入力に応じてプレイヤーを動かす
void player_handle_input(Player* player, const Uint8* key_state, double deltaTime) {
    
    double moveStep = player->moveSpeed * deltaTime;
    double rotStep = player->rotSpeed * deltaTime;

    // 前後の移動 (W, S)
    if (key_state[SDL_SCANCODE_W]) {
        double nextX = player->x + cos(player->angle) * moveStep;
        double nextY = player->y + sin(player->angle) * moveStep;
        // 壁との衝突判定 (簡易)
        if (worldMap[(int)nextX][(int)player->y] == 0) player->x = nextX;
        if (worldMap[(int)player->x][(int)nextY] == 0) player->y = nextY;
    }
    if (key_state[SDL_SCANCODE_S]) {
        double nextX = player->x - cos(player->angle) * moveStep;
        double nextY = player->y - sin(player->angle) * moveStep;
        if (worldMap[(int)nextX][(int)player->y] == 0) player->x = nextX;
        if (worldMap[(int)player->x][(int)nextY] == 0) player->y = nextY;
    }

    // 左右の回転 (A, D) - FPSでは通常、左右は回転
    if (key_state[SDL_SCANCODE_A]) {
        player->angle -= rotStep;
    }
    if (key_state[SDL_SCANCODE_D]) {
        player->angle += rotStep;
    }
    // TODO: ストレイフ（左右平行移動）も追加すると良い
}

    /* "追加"ダメージ処理：シールドが有効ならダメージを無効化 */
int player_take_damage(Player* player, int dmg) {
    if (!player) return 0;
    if (player->shield_active && player->shield_timer > 0.0f) {
        SDL_Log("Damage prevented by shield (dmg=%d). HP=%d/%d", dmg, player->hp, player->maxHp);
        return 0;
    }
    player->hp -= dmg;
    if (player->hp < 0) player->hp = 0;
    SDL_Log("Player took %d damage. HP=%d/%d", dmg, player->hp, player->maxHp);
    return 1;
}
//追加ここまで