#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 12345
#define BUF_SIZE 512

int main() 
{
    int sock;
    struct sockaddr_in srvaddr, cliaddr;
    socklen_t len = sizeof(cliaddr);
    char buf[BUF_SIZE];

    struct sockaddr_in clients[2];
    int client_count = 0;
    memset(clients, 0, sizeof(clients));

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return 1;
    }

    memset(&srvaddr, 0, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET;
    srvaddr.sin_addr.s_addr = INADDR_ANY;
    srvaddr.sin_port = htons(PORT);

    if(bind(sock, (struct sockaddr*)&srvaddr, sizeof(srvaddr)) < 0) {
        perror("bind");
        close(sock);
        return 1;
    }

    printf("UDP relay server started on port %d\n", PORT);

    while (1) {
        ssize_t n = recvfrom(sock, buf, BUF_SIZE, 0, (struct sockaddr*)&cliaddr, &len);
        if (n < 0) {
            perror("recvfrom");
            continue;
        }

        int known = -1;
        for (int i = 0; i < client_count; ++i) {
            if (clients[i].sin_addr.s_addr == cliaddr.sin_addr.s_addr &&
                clients[i].sin_port == cliaddr.sin_port) {
                known = i;
                break;
            }
        }

        if (known == -1 && client_count < 2) {
            clients[client_count++] = cliaddr;
            char ipstr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &cliaddr.sin_addr, ipstr, sizeof(ipstr));
            printf("Client %d registered; %s:%d\n", client_count, ipstr, ntohs(cliaddr.sin_port));
        }

        // 中継先を決める（2人そろっていれば）
        if (client_count == 2) {
            // 送信元がclients[0]なたclients[1]に、中身逆も同様
            int dest = -1;
            if (clients[0].sin_addr.s_addr == cliaddr.sin_addr.s_addr &&
                clients[0].sin_port == cliaddr.sin_port) dest = 1;
            else if (clients[1].sin_addr.s_addr == cliaddr.sin_addr.s_addr &&
                    clients[1].sin_port == cliaddr.sin_port) dest = 0;

            if (dest != -1) {
                ssize_t sent = sendto(sock, buf, n, 0, (struct sockaddr*)&clients[dest], sizeof(clients[dest]));
                if (sent < 0) perror("sendto");
            }
        }
        // まだ1人だけならACK登録メッセージを返してもよい
    }

    close(sock);
    return 0;
}