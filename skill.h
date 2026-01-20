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
// 既存の SKILL_HEAL_AMOUNT (40) は skill.c で定義されていますが、
// 他のスキルも合わせてここで管理する形に移行していくと便利です。

#define SKILL_HEAL_CT 20        // 回復スキルのクールタイム(秒)

#define SKILL_REPAIR_AMOUNT 50  // ドア修理量
#define SKILL_REPAIR_CT 15      // 修理スキルのクールタイム(秒)

// 【汎用ロジック】
// サーバー(Client構造体)でもクライアント(Player構造体)でも使えるように
// 「HPの場所(ポインタ)」だけを受け取って計算する関数です。
void skill_logic_heal_generic(int* hp, int max_hp, int heal_amount);
void skill_logic_repair_generic(int* object_hp, int max_object_hp);

#endif