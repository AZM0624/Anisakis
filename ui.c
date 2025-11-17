#include "ui.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

// UI 状態管理構造体
static TTF_Font *uiFont = NULL;
static SDL_Texture *hpBarTex = NULL;
static SDL_Renderer *gRenderer = NULL;

// HPバーのサイズ
static const int HP_BAR_WIDTH = 200;
static const int HP_BAR_HEIGHT = 20;

// テクスチャ生成
static SDL_Texture *CreateColorTexture(SDL_Renderer *renderer, SDL_Color col, int w, int h) {
    SDL_Surface *surf = SDL_CreateRGBSurface(0, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    SDL_FillRect(surf, NULL, SDL_MapRGBA(surf->format, col.r, col.g, col.b, col.a));
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    return tex;
}

// 初期化
int UI_Init(SDL_Renderer *renderer) {
    gRenderer = renderer;

    if (TTF_Init() < 0) return -1;

    uiFont = TTF_OpenFont("./font.ttf", 24);
    if (!uiFont) return -1;

    hpBarTex = CreateColorTexture(renderer, (SDL_Color){255, 0, 0, 255}, HP_BAR_WIDTH, HP_BAR_HEIGHT);

    return 0;
}

// HP 表示
void UI_DrawHP(SDL_Renderer *renderer, int hp, int maxHp) {
    float rate = (float)hp / (float)maxHp;
    int w = (int)(HP_BAR_WIDTH * rate);

    SDL_Rect back = {10, 10, HP_BAR_WIDTH, HP_BAR_HEIGHT};
    SDL_Rect front = {10, 10, w, HP_BAR_HEIGHT};

    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderFillRect(renderer, &back);

    SDL_RenderCopy(renderer, hpBarTex, NULL, &front);
}

// テキスト描画
void UI_DrawText(SDL_Renderer *renderer, const char *text, int x, int y) {
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *surf = TTF_RenderUTF8_Blended(uiFont, text, white);
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};

    SDL_FreeSurface(surf);
    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}

// 全 UI 描画呼び出し
void UI_Render(SDL_Renderer *renderer, int hp, int maxHp) {
    UI_DrawHP(renderer, hp, maxHp);
}

// 終了処理
void UI_Destroy() {
    if (hpBarTex) SDL_DestroyTexture(hpBarTex);
    if (uiFont) TTF_CloseFont(uiFont);
    TTF_Quit();
}