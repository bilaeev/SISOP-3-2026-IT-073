#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

static int    sock_fd   = -1;
static int    is_admin  = 0;
static int    running   = 1;
static char   my_name[MAX_NAME];

/* ─────────────────── Thread: Receiver ─────────────────── */

static void *recv_thread(void *arg) {
    (void)arg;
    Packet pkt;
    while (running) {
        memset(&pkt, 0, sizeof(pkt));
        int n = recv(sock_fd, &pkt, sizeof(pkt), 0);
        if (n <= 0) {
            if (running) printf("\n[System] Connection lost.\n");
            running = 0;
            break;
        }

        switch (pkt.type) {
            case MSG_BROADCAST:
                printf("\n%s\n> ", pkt.body);
                fflush(stdout);
                break;
            case MSG_RPC_RESP:
                printf("\n%s\n", pkt.body);
                fflush(stdout);
                break;
            case MSG_SHUTDOWN:
                printf("\n%s\n", pkt.body);
                running = 0;
                break;
            default:
                break;
        }
    }
    return NULL;
}

/* ─────────────────── Signal Handler ─────────────────── */

static void handle_sigint(int sig) {
    (void)sig;
    if (sock_fd >= 0 && running) {
        printf("\n[System] Disconnecting from The Wired...\n");
        Packet pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.type = MSG_EXIT;
        strncpy(pkt.sender, my_name, MAX_NAME - 1);
        send(sock_fd, &pkt, sizeof(pkt), 0);
    }
    running = 0;
    close(sock_fd);
    exit(0);
}

/* ─────────────────── Admin Console ─────────────────── */

static void admin_console() {
    printf("\n=== THE KNIGHTS CONSOLE ===\n");
    printf("1. Check Active Entites (Users)\n");
    printf("2. Check Server Uptime\n");
    printf("3. Execute Emergency Shutdown\n");
    printf("4. Disconnect\n");

    char input[16];
    while (running) {
        printf("Command >> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = 0;

        int cmd = atoi(input);
        if (cmd < 1 || cmd > 4) {
            printf("Invalid command.\n");
            continue;
        }
        if (cmd == 4) break;

        Packet pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.type = MSG_RPC;
        strncpy(pkt.sender, my_name, MAX_NAME - 1);
        snprintf(pkt.body, MAX_MSG, "%d", cmd);
        send(sock_fd, &pkt, sizeof(pkt), 0);

        // Tunggu sebentar agar recv_thread sempat print response
        usleep(300000);

        if (cmd == RPC_SHUTDOWN) {
            running = 0;
            break;
        }
    }
}

/* ─────────────────── Main ─────────────────── */

int main() {
    signal(SIGINT, handle_sigint);

    // Buat socket & connect
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port   = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &srv.sin_addr);

    if (connect(sock_fd, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        perror("connect"); exit(1);
    }

    /* ── Registrasi Nama ── */
    Packet pkt, resp;
    while (1) {
        printf("Enter your name: ");
        fflush(stdout);
        if (!fgets(my_name, MAX_NAME, stdin)) exit(0);
        my_name[strcspn(my_name, "\n")] = 0;
        if (strlen(my_name) == 0) continue;

        memset(&pkt, 0, sizeof(pkt));
        pkt.type = MSG_REGISTER;
        strncpy(pkt.sender, my_name, MAX_NAME - 1);
        send(sock_fd, &pkt, sizeof(pkt), 0);

        memset(&resp, 0, sizeof(resp));
        recv(sock_fd, &resp, sizeof(resp), 0);

        if (resp.type == MSG_NAME_TAKEN) {
            printf("%s\n", resp.body);
            continue;
        }

        // MSG_ACCEPT
        if (strcmp(resp.body, "NEED_PASSWORD") == 0) {
            // Admin: minta password
            char passwd[MAX_MSG];
            printf("Enter Password: ");
            fflush(stdout);
            if (!fgets(passwd, sizeof(passwd), stdin)) exit(0);
            passwd[strcspn(passwd, "\n")] = 0;

            memset(&pkt, 0, sizeof(pkt));
            pkt.type = MSG_ADMIN_AUTH;
            strncpy(pkt.sender, my_name, MAX_NAME - 1);
            strncpy(pkt.body, passwd, MAX_MSG - 1);
            send(sock_fd, &pkt, sizeof(pkt), 0);

            memset(&resp, 0, sizeof(resp));
            recv(sock_fd, &resp, sizeof(resp), 0);

            if (resp.type == MSG_ACCEPT && strcmp(resp.body, "ADMIN_OK") == 0) {
                printf("\n[System] Authentication Successful. Granted Admin privileges.\n");
                is_admin = 1;
                break;
            } else {
                printf("%s\n", resp.body);
                close(sock_fd);
                exit(0);
            }
        } else {
            // User biasa
            printf("--- Welcome to The Wired, %s ---\n", my_name);
            break;
        }
    }

    /* ── Mulai recv thread ── */
    pthread_t rtid;
    pthread_create(&rtid, NULL, recv_thread, NULL);
    pthread_detach(rtid);

    /* ── Admin Console atau Chat ── */
    if (is_admin) {
        admin_console();
    } else {
        // Chat loop (thread utama = sender)
        char input[MAX_MSG];
        printf("> ");
        fflush(stdout);

        while (running && fgets(input, sizeof(input), stdin)) {
            input[strcspn(input, "\n")] = 0;

            if (strcmp(input, "/exit") == 0) {
                printf("[System] Disconnecting from The Wired...\n");
                memset(&pkt, 0, sizeof(pkt));
                pkt.type = MSG_EXIT;
                strncpy(pkt.sender, my_name, MAX_NAME - 1);
                send(sock_fd, &pkt, sizeof(pkt), 0);
                break;
            }

            if (strlen(input) == 0) {
                printf("> ");
                fflush(stdout);
                continue;
            }

            memset(&pkt, 0, sizeof(pkt));
            pkt.type = MSG_CHAT;
            strncpy(pkt.sender, my_name, MAX_NAME - 1);
            strncpy(pkt.body, input, MAX_MSG - 1);
            send(sock_fd, &pkt, sizeof(pkt), 0);

            printf("> ");
            fflush(stdout);
        }
    }

    running = 0;
    close(sock_fd);
    return 0;
}
