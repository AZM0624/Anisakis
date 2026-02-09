#ifndef SKILL_H
#define SKILL_H

#include "player.h"

void skill_escudo(Player* player);
void skill_heal(Player* p);                       
void skill_shield_activate(Player* p);            
void skill_update(Player* p, float dt);           
int skill_is_shielded(const Player* p);                      
float skill_get_shield_time_remaining(const Player* p);

// バランス調整用の設定値

/* スキル操作（main から呼ぶ） */
void skill_heal(Player* p);                       /* F1: 即時回復 */
void skill_shield_activate(Player* p);            /* F2: シールド開始（Player に設定） */
void skill_dash(Player* p);                /* ダッシュ */
void skill_stealth(Player* p);             /* ステルス */
void skill_update(Player* p, float dt);                    /* 毎フレーム更新 */


#define SKILL_HEAL_CT 20        // 回復スキルのクールタイム(秒)

#define SKILL_REPAIR_AMOUNT 50  // ドア修理量
#define SKILL_REPAIR_CT 15      // 修理スキルのクールタイム(秒)

void skill_logic_heal_generic(int* hp, int max_hp, int heal_amount);
void skill_logic_repair_generic(int* object_hp, int max_object_hp);

#endif
