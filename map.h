#ifndef MAP_H
#define MAP_H

#define MAP_WIDTH 24
#define MAP_HEIGHT 24
#define TILE_SIZE 64 // 1タイルのピクセルサイズ（2Dマップ上での話）

// 0 = 床, 1以上 = 壁
extern int worldMap[MAP_WIDTH][MAP_HEIGHT];

#endif