#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

/* ─────────────────── Data Structures ─────────────────── */

#define MAX_CLIENTS 64

typedef struct {
    int     fd;
    char    name[MAX_NAME];
    int     is_admin;
    int     active;
} Client;

static Client   clients[MAX_CLIENTS];
static int      client_count = 0;
static pthread_mutex_t clients_mtx = PTHREAD_MUTEX_INITIALIZER;

static time_t   server_start;
static int      server_fd;
static FILE    *log_fp;
static pthread_mutex_t log_mtx = PTHREAD_MUTEX_INITIALIZER;

/* ─────────────────── Logging ─────────────────── */

static void log_write(const char *category, const char *message) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", t);

    pthread_mutex_lock(&log_mtx);
    fprintf(log_fp, "[%s] [%s] [%s]\n", ts, category, message);
    fflush(log_fp);
    pthread_mutex_unlock(&log_mtx);
}

/* ─────────────────── Helpers ─────────────────── */

// Kirim packet ke satu fd
static int send_packet(int fd, Packet *pkt) {
    return send(fd, pkt, sizeof(Packet), 0);
}

// Broadcast ke semua client aktif kecuali exclude_fd
static void broadcast(Packet *pkt, int exclude_fd) {
    pthread_mutex_lock(&clients_mtx);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && !clients[i].is_admin &&
            clients[i].fd != exclude_fd) {
            send_packet(clients[i].fd, pkt);
        }
    }
    pthread_mutex_unlock(&clients_mtx);
}

// Cari slot kosong
static int find_slot() {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (!clients[i].active) return i;
    return -1;
}

// Cek apakah nama sudah dipakai
static int name_exists(const char *name) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].active && strcmp(clients[i].name, name) == 0)
            return 1;
    return 0;
}

// Hitung user aktif (bukan admin)
static int count_users() {
    int cnt = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].active && !clients[i].is_admin) cnt++;
    return cnt;
}

// Hapus client dari slot
static void remove_client(int idx) {
    pthread_mutex_lock(&clients_mtx);
    clients[idx].active = 0;
    clients[idx].fd     = -1;
    client_count--;
    pthread_mutex_unlock(&clients_mtx);
}

/* ─────────────────── RPC Handler ─────────────────── */

static void handle_rpc(int client_idx, int rpc_cmd) {
    Client *c = &clients[client_idx];
    Packet resp;
    memset(&resp, 0, sizeof(resp));
    resp.type = MSG_RPC_RESP;
    strcpy(resp.sender, "Server");

    char log_msg[128];

    switch (rpc_cmd) {
        case RPC_GET_USERS: {
            int n = count_users();
            snprintf(resp.body, MAX_MSG,
                     "[RPC] Active entities in The Wired: %d", n);
            snprintf(log_msg, sizeof(log_msg), "RPC_GET_USERS");
            log_write("Admin", log_msg);
            send_packet(c->fd, &resp);
            break;
        }
        case RPC_GET_UPTIME: {
            long uptime = (long)(time(NULL) - server_start);
            long h = uptime / 3600, m = (uptime % 3600) / 60, s = uptime % 60;
            snprintf(resp.body, MAX_MSG,
                     "[RPC] Server uptime: %02ldh %02ldm %02lds", h, m, s);
            snprintf(log_msg, sizeof(log_msg), "RPC_GET_UPTIME");
            log_write("Admin", log_msg);
            send_packet(c->fd, &resp);
            break;
        }
        case RPC_SHUTDOWN: {
            snprintf(resp.body, MAX_MSG,
                     "[RPC] EMERGENCY SHUTDOWN INITIATED");
            snprintf(log_msg, sizeof(log_msg), "RPC_SHUTDOWN");
            log_write("Admin", log_msg);
            log_write("System", "EMERGENCY SHUTDOWN INITIATED");

            // Beritahu semua client
            Packet bye;
            memset(&bye, 0, sizeof(bye));
            bye.type = MSG_SHUTDOWN;
            strcpy(bye.body, "[System] The Wired is shutting down...");
            send_packet(c->fd, &resp);

            pthread_mutex_lock(&clients_mtx);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].active) {
                    send_packet(clients[i].fd, &bye);
                    close(clients[i].fd);
                    clients[i].active = 0;
                }
            }
            pthread_mutex_unlock(&clients_mtx);

            fclose(log_fp);
            close(server_fd);
            exit(0);
            break;
        }
        default:
            snprintf(resp.body, MAX_MSG, "[RPC] Unknown command.");
            send_packet(c->fd, &resp);
    }
}

/* ─────────────────── Client Thread ─────────────────── */

typedef struct { int idx; } ThreadArg;

static void *client_thread(void *arg) {
    int idx = ((ThreadArg *)arg)->idx;
    free(arg);
    Client *c = &clients[idx];
    int fd    = c->fd;

    Packet pkt;

    /* ── Fase 1: Registrasi nama ── */
    while (1) {
        memset(&pkt, 0, sizeof(pkt));
        if (recv(fd, &pkt, sizeof(pkt), 0) <= 0) {
            close(fd);
            remove_client(idx);
            return NULL;
        }
        if (pkt.type != MSG_REGISTER) continue;

        pthread_mutex_lock(&clients_mtx);
        int taken = name_exists(pkt.sender);
        if (!taken) {
            strncpy(c->name, pkt.sender, MAX_NAME - 1);
        }
        pthread_mutex_unlock(&clients_mtx);

        Packet resp;
        memset(&resp, 0, sizeof(resp));

        if (taken) {
            resp.type = MSG_NAME_TAKEN;
            strcpy(resp.body,
                   "[System] The identity is already synchronized in The Wired.");
            send_packet(fd, &resp);
            // loop lagi → client akan kirim nama baru
        } else {
            // Cek apakah admin
            if (strcmp(c->name, ADMIN_NAME) == 0) {
                /* Minta password */
                resp.type = MSG_ACCEPT;
                strcpy(resp.body, "NEED_PASSWORD");
                send_packet(fd, &resp);

                /* Terima password */
                memset(&pkt, 0, sizeof(pkt));
                recv(fd, &pkt, sizeof(pkt), 0);

                if (pkt.type == MSG_ADMIN_AUTH &&
                    strcmp(pkt.body, ADMIN_PASSWORD) == 0) {
                    c->is_admin = 1;
                    resp.type   = MSG_ACCEPT;
                    strcpy(resp.body, "ADMIN_OK");
                    send_packet(fd, &resp);

                    char log_msg[128];
                    snprintf(log_msg, sizeof(log_msg),
                             "User '%s' connected", c->name);
                    log_write("System", log_msg);
                } else {
                    resp.type = MSG_NAME_TAKEN; // gunakan sebagai "auth failed"
                    strcpy(resp.body, "[System] Authentication failed.");
                    send_packet(fd, &resp);
                    close(fd);
                    remove_client(idx);
                    return NULL;
                }
            } else {
                resp.type = MSG_ACCEPT;
                strcpy(resp.body, "OK");
                send_packet(fd, &resp);

                char log_msg[128];
                snprintf(log_msg, sizeof(log_msg),
                         "User '%s' connected", c->name);
                log_write("System", log_msg);

                // Broadcast join
                Packet notif;
                memset(&notif, 0, sizeof(notif));
                notif.type = MSG_BROADCAST;
                strcpy(notif.sender, "System");
                snprintf(notif.body, MAX_MSG,
                         "[System] %s has entered The Wired.", c->name);
                broadcast(&notif, fd);
            }
            break;
        }
    }

    /* ── Fase 2: Terima pesan ── */
    while (1) {
        memset(&pkt, 0, sizeof(pkt));
        int n = recv(fd, &pkt, sizeof(pkt), 0);
        if (n <= 0) {
            // Koneksi putus tiba-tiba
            goto disconnect;
        }

        if (pkt.type == MSG_EXIT) {
            goto disconnect;
        }

        if (pkt.type == MSG_RPC && c->is_admin) {
            int cmd = atoi(pkt.body);
            handle_rpc(idx, cmd);
            if (cmd == RPC_SHUTDOWN) return NULL; // server sudah exit
            continue;
        }

        if (pkt.type == MSG_CHAT && !c->is_admin) {
            // Format broadcast
            Packet bcast;
            memset(&bcast, 0, sizeof(bcast));
            bcast.type = MSG_BROADCAST;
            strncpy(bcast.sender, c->name, MAX_NAME - 1);
            snprintf(bcast.body, MAX_MSG, "[%s]: %.440s", c->name, pkt.body);
            broadcast(&bcast, fd);

            // Log
            char log_msg[MAX_MSG + MAX_NAME + 8];
            snprintf(log_msg, sizeof(log_msg),
                     "[%s]: %s", c->name, pkt.body);
            log_write("User", log_msg);
        }
    }

disconnect:
    {
        char log_msg[128];
        snprintf(log_msg, sizeof(log_msg),
                 "User '%s' disconnected", c->name);
        log_write("System", log_msg);

        // Broadcast leave (hanya non-admin)
        if (!c->is_admin) {
            Packet notif;
            memset(&notif, 0, sizeof(notif));
            notif.type = MSG_BROADCAST;
            strcpy(notif.sender, "System");
            snprintf(notif.body, MAX_MSG,
                     "[System] %s has left The Wired.", c->name);
            broadcast(&notif, fd);
        }

        close(fd);
        remove_client(idx);
    }
    return NULL;
}

/* ─────────────────── Signal Handler ─────────────────── */

static void handle_sigint(int sig) {
    (void)sig;
    printf("\n[Server] Shutting down...\n");
    log_write("System", "SERVER OFFLINE");
    fclose(log_fp);
    close(server_fd);
    exit(0);
}

/* ─────────────────── Main ─────────────────── */

int main() {
    server_start = time(NULL);

    // Buka log
    log_fp = fopen("history.log", "a");
    if (!log_fp) { perror("fopen log"); exit(1); }
    log_write("System", "SERVER ONLINE");

    // Signal
    signal(SIGINT, handle_sigint);

    // Buat socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(SERVER_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(server_fd, 10) < 0) { perror("listen"); exit(1); }

    printf("[The Wired] Server online on port %d\n", SERVER_PORT);

    // Inisialisasi slot
    memset(clients, 0, sizeof(clients));

    // Accept loop
    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int cli_fd = accept(server_fd,
                            (struct sockaddr *)&cli_addr, &cli_len);
        if (cli_fd < 0) { perror("accept"); continue; }

        pthread_mutex_lock(&clients_mtx);
        int slot = find_slot();
        if (slot < 0) {
            pthread_mutex_unlock(&clients_mtx);
            close(cli_fd);
            continue;
        }
        clients[slot].fd       = cli_fd;
        clients[slot].active   = 1;
        clients[slot].is_admin = 0;
        memset(clients[slot].name, 0, MAX_NAME);
        client_count++;
        pthread_mutex_unlock(&clients_mtx);

        ThreadArg *targ = malloc(sizeof(ThreadArg));
        targ->idx = slot;

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, targ);
        pthread_detach(tid);  // tidak perlu join
    }

    return 0;
}
