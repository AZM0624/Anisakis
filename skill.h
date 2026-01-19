#ifndef SKILL_H
#define SKILL_H

#include "player.h"

// エスクードスキル
void skill_escudo(Player* player);

/* ”追加”　スキル操作（main から呼ぶ） */
void skill_heal(Player* p);                       /* F1: 即時回復 */
void skill_shield_activate(Player* p);            /* F2: シールド開始（Player に設定） */
void skill_update(Player* p, float dt);                    /* 毎フレーム更新（タイマー消化） */

/* ステータス取得 */
int skill_is_shielded(const Player* p);                      
float skill_get_shield_time_remaining(const Player* p);/* シールド残り時間（秒） */
//ここまで追加

#endif