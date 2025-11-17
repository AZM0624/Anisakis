#ifndef UI_H
#define UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

// UI の状態構造体
typedef struct UIState {
    SDL_Texture *skill_icons[4];   // スキルアイコン
    SDL_Rect skill_rects[4];       // 各アイコンの位置

    SDL_Texture *hp_bar;           // HPバー画像
    SDL_Rect hp_rect;              // HPバー表示領域

    TTF_Font *font;                // フォント
    SDL_Texture *text_texture;     // FPS やステータス表示用テキスト
    SDL_Rect text_rect;

    int hp;                        // 現在 HP
    float cooldown[4];             // 各スキルのクールダウン

} UIState;

// UI 初期化
int ui_init(UIState *ui, SDL_Renderer *renderer);

// UI 解放
void ui_destroy(UIState *ui);

// UI イベント処理（クリックなどが必要なら）
void ui_handle_event(UIState *ui, SDL_Event *event);

// UI 更新（HP やクールダウンの更新）
void ui_update(UIState *ui, float deltaTime);

// UI 描画
void ui_render(UIState *ui, SDL_Renderer *renderer);

#endif