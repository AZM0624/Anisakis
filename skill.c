#include "skill.h"
#include "map.h"
#include <math.h>
#include <stdio.h>

#define SKILL_SHIELD_DURATION 5.0f

#define DASH_DURATION 5.0f
#define DASH_COOLDOWN 20.0f

#define STEALTH_DURATION 3.0f
#define STEALTH_COOLDOWN 30.0f

void skill_heal(Player* p)
{
    if (!p) return;

    p->hp = p->maxHp; 
    SDL_Log("Skill: Full Heal applied. Current HP: %d/%d", p->hp, p->maxHp);
}

void skill_dash(Player* p)
{
    if (!p) return;
    if (p->dash_cooldown > 0.0f) return;

    p->dash_active = 1;
    p->dash_timer = DASH_DURATION;
    p->dash_cooldown = DASH_COOLDOWN;

    SDL_Log("Skill: Dash activated");
}

void skill_stealth(Player* p)
{
    if (!p) return;
    if (p->stealth_cooldown > 0.0f) return;

    p->stealth_active = 1;
    p->stealth_timer = STEALTH_DURATION;
    p->stealth_cooldown = STEALTH_COOLDOWN;

    SDL_Log("Skill: Stealth activated");
}

void skill_shield_activate(Player* p)
{
    if (!p) return;
    p->shield_active = 1;
    p->shield_timer = SKILL_SHIELD_DURATION;
    SDL_Log("Skill: Shield activated for %.1f seconds", SKILL_SHIELD_DURATION);
}

void skill_update(Player* p, float dt)
{
    if (!p) return;
/* ---- エスクード（追加） ---- */
    if (p->escudo_cooldown > 0.0f) {
        p->escudo_cooldown -= dt;
    }

    /* ---- シールド ---- */
    if (p->shield_timer > 0.0f) {
        p->shield_timer -= dt;
        if (p->shield_timer <= 0.0f) {
            p->shield_timer = 0.0f;
            p->shield_active = 0;
            SDL_Log("Skill: Shield expired");
        }
    }

    /* ---- ダッシュ ---- */
    if (p->dash_active) {
        p->dash_timer -= dt;
        if (p->dash_timer <= 0.0f) {
            p->dash_active = 0;
            SDL_Log("Skill: Dash ended");
        }
    }
    if (p->dash_cooldown > 0.0f) {
        p->dash_cooldown -= dt;
        if (p->dash_cooldown < 0.0f) p->dash_cooldown = 0.0f;
    }

    /* ---- ステルス ---- */
    if (p->stealth_active) {
        p->stealth_timer -= dt;
        if (p->stealth_timer <= 0.0f) {
            p->stealth_active = 0;
            SDL_Log("Skill: Stealth ended");
        }
    }
    if (p->stealth_cooldown > 0.0f) {
        p->stealth_cooldown -= dt;
        if (p->stealth_cooldown < 0.0f) p->stealth_cooldown = 0.0f;
        
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

// skill_escudo関数を修正
void skill_escudo(Player* player) {
    // クールタイム判定
    if (player->escudo_cooldown > 0.0f) {
        printf("Escudo is cooling down...\n");
        return;
    }

    // ★追加: 回数制限のチェック
    if (player->escudo_stock <= 0) {
        printf("Escudo out of stock!\n");
        return;
    }

    double distance = 2.0;
    int targetX = (int)(player->x + cos(player->angle) * distance);
    int targetY = (int)(player->y + sin(player->angle) * distance);

    if (targetX >= 0 && targetX < MAP_WIDTH && targetY >= 0 && targetY < MAP_HEIGHT) {
        if (worldMap[targetX][targetY] == 0) {
            worldMap[targetX][targetY] = 2; // 壁を設置
            
            // プレイヤー情報更新
            player->active_wall_x = targetX;
            player->active_wall_y = targetY;
            player->active_wall_hp = 200;
            
            player->escudo_cooldown = 10.0f; // クールタイムは連発防止用として残す
            
            player->escudo_stock--; // ★追加: ストックを1つ減らす
            
            printf("Escudo activated at (%d, %d). Remaining: %d\n", targetX, targetY, player->escudo_stock);
        } else {
            printf("Cannot place Escudo here (Blocked).\n");
        }
    }
}

// 汎用回復ロジック
// サーバー側で呼ぶときは skill_logic_heal_generic(&clients[id].hp, 100, 40); のように使います
void skill_logic_heal_generic(int* hp, int max_hp, int heal_amount) {
    if (!hp) return;
    if (*hp <= 0) return; // 死亡時は回復不可

    int old_hp = *hp;
    *hp += heal_amount;
    if (*hp > max_hp) *hp = max_hp;

    printf("[Skill Logic] Heal: %d -> %d\n", old_hp, *hp);
}

// 汎用修理ロジック
void skill_logic_repair_generic(int* object_hp, int max_object_hp) {
    if (!object_hp) return;
    if (*object_hp <= 0) return; // 破壊済みなら直せない

    int old_hp = *object_hp;
    *object_hp += SKILL_REPAIR_AMOUNT; // skill.hで定義した値(50)
    if (*object_hp > max_object_hp) *object_hp = max_object_hp;

    printf("[Skill Logic] Repair: %d -> %d\n", old_hp, *object_hp);
}