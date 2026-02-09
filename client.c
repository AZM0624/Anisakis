#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include <SDL2/SDL.h>

#define SERVER_IP "127.0.0.1"   // サーバーのIPをここで指定
#define SERVER_PORT 12345
#define BUF_SIZE 512
#define WIN_W 640
#define WIN_H 480

// 簡易パケット構造
#pragma pack(push,1)
typedef struct {
    uint32_t seq;
    float x, y;
    float angle;
    uint8_t btn;
} pkt_t;
#pragma pack(pop)

int make_socket_nonblocking(int s) {
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(s, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char **argv) {
    int sock;
    struct sockaddr_in srvaddr;
    socklen_t slen = sizeof(srvaddr);
 
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return 1;
    }

    // 非ブロッキングにする
    make_socket_nonblocking(sock);

    memset(&srvaddr,  0, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET;
    srvaddr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &srvaddr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP\n"); return 1;
    }

    // SDL 初期化
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1;
    }
    SDL_Window *win = SDL_CreateWindow("1v1 Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    float px = WIN_W/4.0f, py = WIN_H/2.0f;           // 自分
    float ox = 3*WIN_W/4.0f, oy = WIN_H/2.0f;         // 相手
    uint32_t seq = 0;

    // 最初に一個登録パケットを送る
    pkt_t p;
    p.seq = htonl(seq++);
    float nx = px, ny = py;
    uint32_t netx, nety;
    memcpy(&netx, &nx, sizeof(float));
    memcpy(&nety, &ny, sizeof(float));
    // We'll send floats in network order by copying raw bytes (no standard ntohf)
    // Simpler: send in host order and accept local testing; but use raw bytes for portability
    // Here we send as IEEE float raw bytes (little/big endianness remains; for production, serialize)
    p.x = px; p.y = py; p.btn = 0;
    sendto(sock, &p, sizeof(p), 0, (struct sockaddr*)&srvaddr, slen);

    int running = 1;
    Uint32 last = SDL_GetTicks();
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
        }

        const Uint8 *state = SDL_GetKeyboardState(NULL);
        float speed = 200.0f; // px/s
        Uint32 now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f;
        last = now;

        if (state[SDL_SCANCODE_UP])    py -= speed * dt;
        if (state[SDL_SCANCODE_DOWN])  py += speed * dt;
        if (state[SDL_SCANCODE_LEFT])  px -= speed * dt;
        if (state[SDL_SCANCODE_RIGHT]) px += speed * dt;

        // 画面内に留める
        if (px < 0) px = 0; if (px > WIN_W) px = WIN_W;
        if (py < 0) py = 0; if (py > WIN_H) py = WIN_H;

        // パケット送信（毎フレーム）
        pkt_t out;
        out.seq = htonl(seq++);
        // 注意: float のバイトオーダー扱いは環境依存だが、簡易プロトタイプとして送る
        // For real product: convert float to int or use defined serialization.
        out.x = px; out.y = py; out.angle = 0.0f; out.btn = 0;
        sendto(sock, &out, sizeof(out), 0, (struct sockaddr*)&srvaddr, slen);

        // 受信ポーリング（非ブロッキング）
        char buf[BUF_SIZE];
        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        ssize_t n;
        while ((n = recvfrom(sock, buf, BUF_SIZE, 0, (struct sockaddr*)&from, &fromlen)) > 0) {
            if (n >= (ssize_t)sizeof(pkt_t)) {
                pkt_t in;
                memcpy(&in, buf, sizeof(pkt_t));
                uint32_t rseq = ntohl(in.seq);
                // 直接floatを受け取って代入
                ox = in.x; oy = in.y;
                //printf("recv seq %u pos(%.1f,%.1f)\n", rseq, ox, oy);
            }
            fromlen = sizeof(from); // reset for next
        }
        // n < 0 and errno EWOULDBLOCK/EAGAIN -> no more data
        // ignore other errors for brevity

        // 描画
        SDL_SetRenderDrawColor(ren, 20, 20, 20, 255);
        SDL_RenderClear(ren);

        // 自分（青）
        SDL_Rect r1 = { (int)px - 10, (int)py - 10, 20, 20 };
        SDL_SetRenderDrawColor(ren, 0, 0, 255, 255);
        SDL_RenderFillRect(ren, &r1);

        // 相手（赤）
        SDL_Rect r2 = { (int)ox - 10, (int)oy - 10, 20, 20 };
        SDL_SetRenderDrawColor(ren, 255, 0, 0, 255);
        SDL_RenderFillRect(ren, &r2);

        // 簡易HUD
        SDL_RenderPresent(ren);

        SDL_Delay(16); // ~60 FPS
    }

    close(sock);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

