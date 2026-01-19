#include "skill.h"
#include "map.h"
#include <math.h>
#include <stdio.h>

/* 追加*/
#define SKILL_HEAL_AMOUNT 40
#define SKILL_SHIELD_DURATION 5.0f

void skill_heal(Player* p)
{
    if (!p) return;
    p->hp += SKILL_HEAL_AMOUNT;
    if (p->hp > p->maxHp) p->hp = p->maxHp;
    SDL_Log("Skill: Heal applied (+%d). Current HP: %d/%d", SKILL_HEAL_AMOUNT, p->hp, p->maxHp);
}

void skill_shield_activate(Player* p)
{
    if (!p) return;
    p->shield_active = 1;
    p->shield_timer = SKILL_SHIELD_DURATION;
    SDL_Log("Skill: Shield activated for %.1f seconds", SKILL_SHIELD_DURATION);
}

/* 毎フレーム呼ぶ。Player のシールドタイマーを減らす */
void skill_update(Player* p, float dt)
{
    if (!p) return;
    if (p->shield_timer > 0.0f) {
        p->shield_timer -= dt;
        if (p->shield_timer <= 0.0f) {
            p->shield_timer = 0.0f;
            p->shield_active = 0;
            SDL_Log("Skill: Shield expired");
        }
    }
}

int skill_is_shielded(const Player* p)
{
    if (!p) return 0;
    return (p->shield_active && p->shield_timer > 0.0f) ? 1 : 0;
}

float skill_get_shield_time_remaining(const Player* p)
{
    if (!p) return 0.0f;
    return p->shield_timer;
}
//ここまで追加

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