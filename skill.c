#include "skill.h"
#include "map.h"
#include <math.h>
#include <stdio.h>

void skill_escudo(Player* player) {
    // 目の前2.0タイル先の座標を計算
    double distance = 2.0;
    int targetX = (int)(player->x + cos(player->angle) * distance);
    int targetY = (int)(player->y + sin(player->angle) * distance);

    // マップ範囲であるか確認
    if (targetX >= 0 && targetX < MAP_WIDTH && targetY >= 0 && targetY < MAP_HEIGHT) {
        // 何もない場所なら₍0₎ならエスクード₍2₎を設置
        if (worldMap[targetX][targetY] == 0) {
            worldMap[targetX][targetY] = 2;
            printf("Escudo activated at (%d, %d)\n", targetX, targetY);
        } else {
            printf("Cannot place Escudo here (Blocked).\n");
        }
    }
}