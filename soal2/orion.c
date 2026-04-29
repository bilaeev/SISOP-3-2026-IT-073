/*
 * orion.c  — Server "Orion"
 */

#include "arena.h"

static int       shmid, msgqid, semid;
static SharedData *shm;
static volatile int running = 1;

void  handle_register(Message *req);
void  handle_login(Message *req);
void  handle_logout(Message *req);
void  handle_matchmake(Message *req);
void  handle_cancel_match(Message *req);
void  handle_attack(Message *req);
void  handle_ultimate(Message *req);
void  handle_buy_weapon(Message *req);
void  handle_history(Message *req);
void  send_response(long dest_type, const char *msg, int ival);
int   find_player(const char *username);
void *matchmaking_watchdog(void *arg);
void  cleanup(int sig);

int main(void) {
    printf("=== ORION SERVER (Eterion) ===\n");
    printf("Initializing IPC resources...\n");

    shmid = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shmid < 0) { perror("shmget"); exit(1); }
    shm = (SharedData *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) { perror("shmat"); exit(1); }

    memset(shm, 0, sizeof(SharedData));
    for (int i = 0; i < MAX_PLAYERS; i++) {
        shm->hp[i]               = 0;
        shm->in_battle[i]        = 0;
        shm->last_attack[i]      = 0;
        shm->battle_log_count[i] = 0;
    }

    msgqid = msgget(MSGQ_KEY, IPC_CREAT | 0666);
    if (msgqid < 0) { perror("msgget"); exit(1); }

    semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (semid < 0) { perror("semget"); exit(1); }
    semctl(semid, 0, SETVAL, 1);

    signal(SIGINT,  cleanup);
    signal(SIGTERM, cleanup);

    pthread_t wd_tid;
    pthread_create(&wd_tid, NULL, matchmaking_watchdog, NULL);
    pthread_detach(wd_tid);

    printf("Orion is ready. Waiting for Eternal...\n\n");

    Message req;
    while (running) {
        if (msgrcv(msgqid, &req, sizeof(req) - sizeof(long), 0, 0) < 0) {
            if (!running) break;
            perror("msgrcv");
            continue;
        }
        switch (req.mtype) {
            case MSG_REGISTER:     handle_register(&req);     break;
            case MSG_LOGIN:        handle_login(&req);        break;
            case MSG_LOGOUT:       handle_logout(&req);       break;
            case MSG_MATCHMAKE:    handle_matchmake(&req);    break;
            case MSG_CANCEL_MATCH: handle_cancel_match(&req); break;
            case MSG_ATTACK:       handle_attack(&req);       break;
            case MSG_ULTIMATE:     handle_ultimate(&req);     break;
            case MSG_BUY_WEAPON:   handle_buy_weapon(&req);   break;
            case MSG_HISTORY:      handle_history(&req);      break;
            default:
                fprintf(stderr, "[Server] Unknown msg type %ld\n", req.mtype);
        }
    }
    cleanup(0);
    return 0;
}

void send_response(long dest_type, const char *msg, int ival) {
    Message resp;
    memset(&resp, 0, sizeof(resp));
    resp.mtype  = dest_type;
    resp.ivalue = ival;
    strncpy(resp.payload, msg, sizeof(resp.payload) - 1);
    msgsnd(msgqid, &resp, sizeof(resp) - sizeof(long), 0);
}

int find_player(const char *username) {
    for (int i = 0; i < shm->player_count; i++)
        if (strcmp(shm->players[i].username, username) == 0) return i;
    return -1;
}

void add_battle_log(int idx, const char *log) {
    if (shm->battle_log_count[idx] >= BATTLE_LOGS) {
        for (int i = 0; i < BATTLE_LOGS - 1; i++)
            memcpy(shm->battle_logs[idx][i], shm->battle_logs[idx][i+1], MAX_LOG_LEN);
        shm->battle_log_count[idx] = BATTLE_LOGS - 1;
    }
    strncpy(shm->battle_logs[idx][shm->battle_log_count[idx]], log, MAX_LOG_LEN - 1);
    shm->battle_log_count[idx]++;
}

void add_history(int pidx, const char *entry) {
    Player *p = &shm->players[pidx];
    if (p->history_count >= MAX_HISTORY) {
        for (int i = 0; i < MAX_HISTORY - 1; i++)
            memcpy(p->history[i], p->history[i+1], MAX_LOG_LEN);
        p->history_count = MAX_HISTORY - 1;
    }
    strncpy(p->history[p->history_count], entry, MAX_LOG_LEN - 1);
    p->history_count++;
}

void handle_register(Message *req) {
    long resp_type = (long)req->client_pid;
    sem_lock(semid);

    if (shm->player_count >= MAX_PLAYERS) {
        sem_unlock(semid);
        send_response(resp_type, "Server full.", 0);
        return;
    }
    if (find_player(req->sender) >= 0) {
        sem_unlock(semid);
        send_response(resp_type, "Username already taken!", 0);
        return;
    }

    int idx = shm->player_count++;
    Player *p = &shm->players[idx];
    memset(p, 0, sizeof(Player));
    strncpy(p->username, req->sender,  MAX_NAME_LEN - 1);
    strncpy(p->password, req->payload, MAX_PASS_LEN  - 1);
    p->gold                = BASE_GOLD;
    p->level               = BASE_LEVEL;
    p->xp                  = BASE_XP;
    p->status              = STATUS_OFFLINE;
    p->has_weapon          = 0;
    p->weapon_damage       = 0;
    p->battle_opponent_idx = -1;
    p->history_count       = 0;

    sem_unlock(semid);
    printf("[Server] Registered player: %s\n", p->username);
    send_response(resp_type, "Account created!", 1);
}

void handle_login(Message *req) {
    long resp_type = (long)req->client_pid;
    sem_lock(semid);

    int idx = find_player(req->sender);
    if (idx < 0) {
        sem_unlock(semid);
        send_response(resp_type, "Username not found.", 0);
        return;
    }
    Player *p = &shm->players[idx];
    if (strcmp(p->password, req->payload) != 0) {
        sem_unlock(semid);
        send_response(resp_type, "Wrong password.", 0);
        return;
    }
    if (p->status != STATUS_OFFLINE) {
        sem_unlock(semid);
        send_response(resp_type, "Account already logged in.", 0);
        return;
    }
    p->status = STATUS_ONLINE;
    sem_unlock(semid);

    printf("[Server] Player logged in: %s\n", p->username);
    char buf[256];
    snprintf(buf, sizeof(buf), "Welcome! Gold:%d Lvl:%d XP:%d",
             p->gold, p->level, p->xp);
    send_response(resp_type, buf, 1);
}

void handle_logout(Message *req) {
    long resp_type = (long)req->client_pid;
    sem_lock(semid);
    int idx = find_player(req->sender);
    if (idx >= 0 && shm->players[idx].status == STATUS_ONLINE)
        shm->players[idx].status = STATUS_OFFLINE;
    sem_unlock(semid);
    printf("[Server] Player logged out: %s\n", req->sender);
    send_response(resp_type, "Logged out.", 1);
}

void handle_matchmake(Message *req) {
    long resp_type = (long)req->client_pid;
    sem_lock(semid);

    int idx = find_player(req->sender);
    if (idx < 0) { sem_unlock(semid); send_response(resp_type, "Not found.", 0); return; }

    shm->players[idx].status = STATUS_MATCHMAKING;
    shm->players[idx].battle_opponent_idx = req->client_pid;

    int opp = -1;
    for (int i = 0; i < shm->player_count; i++) {
        if (i == idx) continue;
        if (shm->players[i].status == STATUS_MATCHMAKING) { opp = i; break; }
    }

    if (opp >= 0) {
        shm->players[idx].status = STATUS_BATTLE;
        shm->players[opp].status = STATUS_BATTLE;

        int hp_idx = calc_health(shm->players[idx].xp);
        int hp_opp = calc_health(shm->players[opp].xp);
        shm->hp[idx] = hp_idx;
        shm->hp[opp] = hp_opp;
        shm->in_battle[idx] = 1;
        shm->in_battle[opp] = 1;
        shm->battle_log_count[idx] = 0;
        shm->battle_log_count[opp] = 0;

        long opp_pid = shm->players[opp].battle_opponent_idx;
        shm->players[idx].battle_opponent_idx = opp;
        shm->players[opp].battle_opponent_idx = idx;
        shm->last_attack[idx] = 0;
        shm->last_attack[opp] = 0;

        sem_unlock(semid);

        char buf[256];
        snprintf(buf, sizeof(buf), "MATCHED:%s HP:%d", shm->players[opp].username, hp_idx);
        send_response(resp_type, buf, 1);

        snprintf(buf, sizeof(buf), "MATCHED:%s HP:%d", shm->players[idx].username, hp_opp);
        send_response(opp_pid, buf, 1);

        printf("[Server] Battle started: %s vs %s\n",
               shm->players[idx].username, shm->players[opp].username);
    } else {
        sem_unlock(semid);
        send_response(resp_type, "SEARCHING", 2);
    }
}

void handle_cancel_match(Message *req) {
    long resp_type = (long)req->client_pid;
    sem_lock(semid);
    int idx = find_player(req->sender);
    if (idx >= 0 && shm->players[idx].status == STATUS_MATCHMAKING) {
        shm->players[idx].status = STATUS_ONLINE;
        shm->players[idx].battle_opponent_idx = -1;
    }
    sem_unlock(semid);
    send_response(resp_type, "Matchmaking cancelled.", 0);
}

static void do_attack_logic(int attacker_idx, int defender_idx, int is_ultimate, long resp_type) {
    Player *atk = &shm->players[attacker_idx];
    Player *def = &shm->players[defender_idx];

    time_t now = time(NULL);
    if (now - shm->last_attack[attacker_idx] < ATTACK_COOLDOWN) {
        send_response(resp_type, "COOLDOWN", 0);
        return;
    }
    shm->last_attack[attacker_idx] = now;

    int dmg = calc_damage(atk->xp, atk->weapon_damage);
    if (is_ultimate) {
        if (!atk->has_weapon) { send_response(resp_type, "No weapon for Ultimate!", 0); return; }
        dmg = calc_ultimate(dmg);
    }

    shm->hp[defender_idx] -= dmg;
    if (shm->hp[defender_idx] < 0) shm->hp[defender_idx] = 0;

    char log[MAX_LOG_LEN];
    snprintf(log, sizeof(log), "%s %s %s for %d dmg (HP: %d)",
             atk->username,
             is_ultimate ? "ULTIMATE->" : "attacks",
             def->username,
             dmg,
             shm->hp[defender_idx]);

    add_battle_log(attacker_idx, log);
    add_battle_log(defender_idx, log);

    char buf[256];
    snprintf(buf, sizeof(buf), "ATK:%d MY_HP:%d OPP_HP:%d",
             dmg, shm->hp[attacker_idx], shm->hp[defender_idx]);
    send_response(resp_type, buf, 1);

    if (shm->hp[defender_idx] <= 0) {
        shm->in_battle[attacker_idx] = 0;
        shm->in_battle[defender_idx] = 0;
        atk->status = STATUS_ONLINE;
        def->status = STATUS_ONLINE;
        atk->battle_opponent_idx = -1;
        def->battle_opponent_idx = -1;

        atk->xp   += XP_WIN;
        atk->gold += GOLD_WIN;
        def->xp   += XP_LOSE;
        def->gold += GOLD_LOSE;
        atk->level = 1 + atk->xp / XP_PER_LEVEL;
        def->level = 1 + def->xp / XP_PER_LEVEL;

        char hist[MAX_LOG_LEN];
        snprintf(hist, sizeof(hist), "WIN vs %s | +%d XP +%d Gold", def->username, XP_WIN, GOLD_WIN);
        add_history(attacker_idx, hist);
        snprintf(hist, sizeof(hist), "LOSE vs %s | +%d XP +%d Gold", atk->username, XP_LOSE, GOLD_LOSE);
        add_history(defender_idx, hist);

        snprintf(buf, sizeof(buf), "BATTLE_END:WIN XP:%d Gold:%d Lvl:%d",
                 atk->xp, atk->gold, atk->level);
        send_response(resp_type, buf, 2);

        snprintf(buf, sizeof(buf), "BATTLE_END:LOSE XP:%d Gold:%d Lvl:%d",
                 def->xp, def->gold, def->level);
        Message end_msg;
        memset(&end_msg, 0, sizeof(end_msg));
        end_msg.mtype = 10000 + defender_idx;
        strncpy(end_msg.payload, buf, sizeof(end_msg.payload) - 1);
        end_msg.ivalue = 2;
        msgsnd(msgqid, &end_msg, sizeof(end_msg) - sizeof(long), 0);

        printf("[Server] Battle ended: %s wins!\n", atk->username);
    }
}

void handle_attack(Message *req) {
    long resp_type = (long)req->client_pid;
    sem_lock(semid);
    int idx = find_player(req->sender);
    if (idx < 0 || !shm->in_battle[idx]) {
        sem_unlock(semid); send_response(resp_type, "Not in battle.", 0); return;
    }
    int opp = shm->players[idx].battle_opponent_idx;
    if (opp < 0) { sem_unlock(semid); send_response(resp_type, "No opponent.", 0); return; }
    do_attack_logic(idx, opp, 0, resp_type);
    sem_unlock(semid);
}

void handle_ultimate(Message *req) {
    long resp_type = (long)req->client_pid;
    sem_lock(semid);
    int idx = find_player(req->sender);
    if (idx < 0 || !shm->in_battle[idx]) {
        sem_unlock(semid); send_response(resp_type, "Not in battle.", 0); return;
    }
    int opp = shm->players[idx].battle_opponent_idx;
    if (opp < 0) { sem_unlock(semid); send_response(resp_type, "No opponent.", 0); return; }
    do_attack_logic(idx, opp, 1, resp_type);
    sem_unlock(semid);
}

void handle_buy_weapon(Message *req) {
    long resp_type = (long)req->client_pid;
    sem_lock(semid);
    int idx = find_player(req->sender);
    if (idx < 0) { sem_unlock(semid); send_response(resp_type, "Not found.", 0); return; }

    int wid = req->ivalue;
    if (wid < 0 || wid >= WEAPON_COUNT) {
        sem_unlock(semid); send_response(resp_type, "Invalid weapon.", 0); return;
    }

    Player *p = &shm->players[idx];
    if (p->gold < WEAPONS[wid].price) {
        sem_unlock(semid);
        char buf[128];
        snprintf(buf, sizeof(buf), "Not enough gold! Need %d, have %d.",
                 WEAPONS[wid].price, p->gold);
        send_response(resp_type, buf, 0);
        return;
    }

    p->gold -= WEAPONS[wid].price;
    if (WEAPONS[wid].bonus_damage > p->weapon_damage)
        p->weapon_damage = WEAPONS[wid].bonus_damage;
    p->has_weapon = 1;

    char buf[128];
    snprintf(buf, sizeof(buf), "Bought %s! Bonus DMG +%d | Gold left: %d",
             WEAPONS[wid].name, WEAPONS[wid].bonus_damage, p->gold);
    sem_unlock(semid);
    send_response(resp_type, buf, 1);
}

void handle_history(Message *req) {
    long resp_type = (long)req->client_pid;
    sem_lock(semid);
    int idx = find_player(req->sender);
    if (idx < 0) { sem_unlock(semid); send_response(resp_type, "Not found.", 0); return; }

    Player *p = &shm->players[idx];
    if (p->history_count == 0) {
        sem_unlock(semid);
        send_response(resp_type, "No battle history yet.", 0);
        return;
    }
    int count = p->history_count;
    sem_unlock(semid);

    send_response(resp_type, "HIST_START", count);
    for (int i = 0; i < count; i++)
        send_response(resp_type, shm->players[idx].history[i], i);
}

void *matchmaking_watchdog(void *arg) {
    (void)arg;
    while (running) {
        sleep(1);
        sem_lock(semid);
        time_t now = time(NULL);
        for (int i = 0; i < shm->player_count; i++) {
            Player *p = &shm->players[i];
            if (p->status != STATUS_MATCHMAKING) continue;
            if (shm->last_attack[i] == 0) { shm->last_attack[i] = now; continue; }
            if (now - shm->last_attack[i] < MATCHMAKING_TIMEOUT) continue;

            p->status = STATUS_BATTLE;
            int bot_idx = MAX_PLAYERS - 1;
            memset(&shm->players[bot_idx], 0, sizeof(Player));
            strncpy(shm->players[bot_idx].username, "BOT_Golem", MAX_NAME_LEN - 1);
            shm->players[bot_idx].xp     = 50;
            shm->players[bot_idx].level  = 2;
            shm->players[bot_idx].status = STATUS_BATTLE;
            shm->players[bot_idx].battle_opponent_idx = i;

            shm->hp[i]       = calc_health(p->xp);
            shm->hp[bot_idx] = 200;
            shm->in_battle[i]       = 1;
            shm->in_battle[bot_idx] = 1;
            p->battle_opponent_idx  = bot_idx;
            shm->last_attack[i]     = 0;
            shm->battle_log_count[i]= 0;

            char buf[256];
            snprintf(buf, sizeof(buf), "MATCHED:BOT_Golem HP:%d", shm->hp[i]);
            Message m;
            memset(&m, 0, sizeof(m));
            m.mtype = 10000 + i;
            strncpy(m.payload, buf, sizeof(m.payload) - 1);
            m.ivalue = 1;
            msgsnd(msgqid, &m, sizeof(m) - sizeof(long), 0);

            printf("[Server] Player %s matched with BOT (timeout)\n", p->username);
        }
        sem_unlock(semid);
    }
    return NULL;
}

void cleanup(int sig) {
    (void)sig;
    running = 0;
    printf("\n[Server] Shutting down...\n");
    if (shm && shm != (void *)-1) {
        sem_lock(semid);
        for (int i = 0; i < shm->player_count; i++)
            shm->players[i].status = STATUS_OFFLINE;
        sem_unlock(semid);
        shmdt(shm);
    }
    printf("[Server] IPC preserved. Run 'make clear_ipc' to remove.\n");
    exit(0);
}
