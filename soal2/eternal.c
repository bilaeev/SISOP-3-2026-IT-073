/*
 * eternal.c  — Client "Eternal"
 */

#include "arena.h"

static int        msgqid, shmid, semid;
static SharedData *shm;
static char       current_user[MAX_NAME_LEN] = {0};
static int        my_idx = -1;
static volatile int battle_running = 0;
static volatile int battle_ended   = 0;

void send_to_server(long mtype, const char *sender, const char *target,
                    const char *payload, int ival);
int  recv_from_server(Message *out, long type, int nowait);
void main_menu(void);
void game_menu(void);
void do_register(void);
void do_login(void);
void do_battle(void);
void do_shop(void);
void do_history(void);
void do_profile(void);
void *battle_opponent_thread(void *arg);
void clear_screen(void);
void print_banner(void);
void disable_echo(void);
void restore_echo(void);
int  find_my_idx(void);

static struct termios old_tio;

int main(void) {
    msgqid = msgget(MSGQ_KEY, 0666);
    if (msgqid < 0) {
        fprintf(stderr, "[Client] Server not running! Start orion first.\n");
        exit(1);
    }
    shmid = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (shmid < 0) {
        fprintf(stderr, "[Client] Cannot connect to shared memory.\n");
        exit(1);
    }
    shm = (SharedData *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) { perror("shmat"); exit(1); }

    semid = semget(SEM_KEY, 1, 0666);
    if (semid < 0) { perror("semget"); exit(1); }

    main_menu();
    shmdt(shm);
    return 0;
}

void send_to_server(long mtype, const char *sender, const char *target,
                    const char *payload, int ival) {
    Message m;
    memset(&m, 0, sizeof(m));
    m.mtype      = mtype;
    m.client_pid = (int)getpid();
    if (sender)  strncpy(m.sender,  sender,  MAX_NAME_LEN - 1);
    if (target)  strncpy(m.target,  target,  MAX_NAME_LEN - 1);
    if (payload) strncpy(m.payload, payload, sizeof(m.payload) - 1);
    m.ivalue = ival;
    msgsnd(msgqid, &m, sizeof(m) - sizeof(long), 0);
}

int recv_from_server(Message *out, long type, int nowait) {
    int flags = nowait ? IPC_NOWAIT : 0;
    ssize_t r = msgrcv(msgqid, out, sizeof(*out) - sizeof(long), type, flags);
    return (int)r;
}

void clear_screen(void) { printf("\033[2J\033[H"); fflush(stdout); }

void print_banner(void) {
    printf("╔══════════════════════════════════════╗\n");
    printf("║         ETERION BATTLE ARENA         ║\n");
    printf("║     Where Eternal Meets Orion...     ║\n");
    printf("╚══════════════════════════════════════╝\n\n");
}

void disable_echo(void) {
    struct termios new_tio;
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_echo(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
}

int find_my_idx(void) {
    for (int i = 0; i < shm->player_count; i++)
        if (strcmp(shm->players[i].username, current_user) == 0) return i;
    return -1;
}

void main_menu(void) {
    while (1) {
        clear_screen();
        print_banner();
        printf("  1. Register\n");
        printf("  2. Login\n");
        printf("  3. Exit\n\n");
        printf("  Choice: ");
        fflush(stdout);

        int choice;
        if (scanf("%d", &choice) != 1) { while(getchar()!='\n'); continue; }
        while(getchar()!='\n');

        switch(choice) {
            case 1: do_register(); break;
            case 2: do_login();    break;
            case 3: printf("Farewell, warrior.\n"); return;
            default: printf("Invalid choice.\n"); sleep(1);
        }
    }
}

void do_register(void) {
    clear_screen();
    printf("╔══════════════════╗\n");
    printf("║   CREATE ACCOUNT ║\n");
    printf("╚══════════════════╝\n\n");

    char uname[MAX_NAME_LEN], pass[MAX_PASS_LEN];
    printf("  Username: "); fflush(stdout);
    fgets(uname, sizeof(uname), stdin);
    uname[strcspn(uname, "\n")] = 0;

    printf("  Password: "); fflush(stdout);
    fgets(pass, sizeof(pass), stdin);
    pass[strcspn(pass, "\n")] = 0;

    if (strlen(uname) == 0 || strlen(pass) == 0) {
        printf("  Username/password cannot be empty.\n"); sleep(2); return;
    }

    send_to_server(MSG_REGISTER, uname, NULL, pass, 0);
    Message resp;
    recv_from_server(&resp, (long)getpid(), 0);
    printf("\n  %s\n", resp.payload);
    sleep(2);
}

void do_login(void) {
    clear_screen();
    printf("╔══════════════════╗\n");
    printf("║      LOGIN       ║\n");
    printf("╚══════════════════╝\n\n");

    char uname[MAX_NAME_LEN], pass[MAX_PASS_LEN];
    printf("  Username: "); fflush(stdout);
    fgets(uname, sizeof(uname), stdin);
    uname[strcspn(uname, "\n")] = 0;

    printf("  Password: "); fflush(stdout);
    fgets(pass, sizeof(pass), stdin);
    pass[strcspn(pass, "\n")] = 0;

    send_to_server(MSG_LOGIN, uname, NULL, pass, 0);
    Message resp;
    recv_from_server(&resp, (long)getpid(), 0);
    printf("\n  %s\n", resp.payload);
    sleep(2);

    if (resp.ivalue == 1) {
        strncpy(current_user, uname, MAX_NAME_LEN - 1);
        my_idx = find_my_idx();
        game_menu();
        memset(current_user, 0, MAX_NAME_LEN);
        my_idx = -1;
    }
}

void game_menu(void) {
    while (1) {
        my_idx = find_my_idx();
        if (my_idx < 0) { printf("Session error.\n"); return; }
        Player *p = &shm->players[my_idx];

        clear_screen();
        print_banner();
        printf("  Welcome, %s!\n", current_user);
        printf("  Gold: %d  |  Lvl: %d  |  XP: %d\n\n", p->gold, p->level, p->xp);
        printf("  1. Battle\n");
        printf("  2. Shop (Armory)\n");
        printf("  3. History\n");
        printf("  4. Profile\n");
        printf("  5. Logout\n\n");
        printf("  Choice: ");
        fflush(stdout);

        int choice;
        if (scanf("%d", &choice) != 1) { while(getchar()!='\n'); continue; }
        while(getchar()!='\n');

        switch(choice) {
            case 1: do_battle();  break;
            case 2: do_shop();    break;
            case 3: do_history(); break;
            case 4: do_profile(); break;
            case 5: {
                send_to_server(MSG_LOGOUT, current_user, NULL, "", 0);
                Message r; recv_from_server(&r, (long)getpid(), 0);
                printf("  %s\n", r.payload); sleep(1);
                return;
            }
            default: printf("Invalid.\n"); sleep(1);
        }
    }
}

void do_profile(void) {
    my_idx = find_my_idx();
    if (my_idx < 0) return;
    Player *p = &shm->players[my_idx];

    clear_screen();
    printf("╔══════════════════════╗\n");
    printf("║       PROFILE        ║\n");
    printf("╚══════════════════════╝\n\n");
    printf("  Username : %s\n",  p->username);
    printf("  Level    : %d\n",  p->level);
    printf("  XP       : %d\n",  p->xp);
    printf("  Gold     : %d\n",  p->gold);
    printf("  Weapon   : %s\n",  p->has_weapon ? "Yes" : "None");
    if (p->has_weapon)
        printf("  Bonus DMG: +%d\n", p->weapon_damage);
    int dmg = calc_damage(p->xp, p->weapon_damage);
    int hp  = calc_health(p->xp);
    printf("  Damage   : %d\n",  dmg);
    printf("  Max HP   : %d\n",  hp);
    if (p->has_weapon)
        printf("  Ultimate : %d\n",  calc_ultimate(dmg));
    printf("\n  [Press Enter to go back]");
    fflush(stdout);
    getchar();
}

void do_shop(void) {
    my_idx = find_my_idx();
    if (my_idx < 0) return;

    while (1) {
        Player *p = &shm->players[my_idx];
        clear_screen();
        printf("╔══════════════════════════════╗\n");
        printf("║        ARMORY SHOP           ║\n");
        printf("╚══════════════════════════════╝\n\n");
        printf("  Your Gold: %d\n\n", p->gold);
        printf("  %-3s %-18s %-8s %-8s\n", "#", "Weapon", "Price", "Bonus DMG");
        printf("  %-3s %-18s %-8s %-8s\n", "---", "------------------", "--------", "---------");
        for (int i = 0; i < WEAPON_COUNT; i++)
            printf("  %-3d %-18s %-8d +%-7d\n",
                   i+1, WEAPONS[i].name, WEAPONS[i].price, WEAPONS[i].bonus_damage);
        printf("\n  0. Back\n\n  Buy #: ");
        fflush(stdout);

        int c;
        if (scanf("%d", &c) != 1) { while(getchar()!='\n'); continue; }
        while(getchar()!='\n');

        if (c == 0) return;
        if (c < 1 || c > WEAPON_COUNT) { printf("Invalid.\n"); sleep(1); continue; }

        send_to_server(MSG_BUY_WEAPON, current_user, NULL, "", c - 1);
        Message resp;
        recv_from_server(&resp, (long)getpid(), 0);
        printf("\n  %s\n", resp.payload);
        sleep(2);
    }
}

void do_history(void) {
    send_to_server(MSG_HISTORY, current_user, NULL, "", 0);
    Message resp;
    recv_from_server(&resp, (long)getpid(), 0);

    clear_screen();
    printf("╔══════════════════════════╗\n");
    printf("║      BATTLE HISTORY      ║\n");
    printf("╚══════════════════════════╝\n\n");

    if (resp.ivalue == 0) {
        printf("  %s\n", resp.payload);
    } else {
        int count = resp.ivalue;
        for (int i = 0; i < count; i++) {
            Message entry;
            recv_from_server(&entry, (long)getpid(), 0);
            printf("  [%2d] %s\n", i+1, entry.payload);
        }
    }
    printf("\n  [Press Enter]"); fflush(stdout); getchar();
}

typedef struct {
    int  my_idx;
    long my_type;
    long pid_type;
} BattleThreadArg;

void *battle_opponent_thread(void *arg) {
    BattleThreadArg *a = (BattleThreadArg *)arg;
    Message m;
    while (battle_running) {
        if (recv_from_server(&m, a->my_type, 1) > 0) {
            if (strncmp(m.payload, "BATTLE_END:", 11) == 0) {
                battle_ended   = 1;
                battle_running = 0;
                printf("\n\n  *** %s ***\n", m.payload + 11);
                fflush(stdout);
            } else if (strncmp(m.payload, "ATK:", 4) == 0) {
                int dmg, my_hp, opp_hp;
                sscanf(m.payload, "ATK:%d MY_HP:%d OPP_HP:%d", &dmg, &opp_hp, &my_hp);
                printf("\r  [OPP ATTACKS for %d!] Your HP: %d          \n", dmg, my_hp);
                fflush(stdout);
            }
        }
        usleep(100000);
    }
    free(a);
    return NULL;
}

void print_battle_logs(int idx) {
    printf("  ┌─────── BATTLE LOG (last %d) ───────┐\n", BATTLE_LOGS);
    for (int i = 0; i < shm->battle_log_count[idx]; i++)
        printf("  │ %-38s│\n", shm->battle_logs[idx][i]);
    for (int i = shm->battle_log_count[idx]; i < BATTLE_LOGS; i++)
        printf("  │ %-38s│\n", "");
    printf("  └────────────────────────────────────┘\n");
}

void do_battle(void) {
    my_idx = find_my_idx();
    if (my_idx < 0) return;

    clear_screen();
    print_banner();
    printf("  Entering matchmaking...\n");
    printf("  (Waiting up to %d seconds for opponent)\n\n", MATCHMAKING_TIMEOUT);
    fflush(stdout);

    send_to_server(MSG_MATCHMAKE, current_user, NULL, "", 0);

    char opp_name[MAX_NAME_LEN] = {0};
    int  my_hp = 0;
    int  found = 0;
    time_t start = time(NULL);

    while (time(NULL) - start < MATCHMAKING_TIMEOUT + 5) {
        Message resp;
        if (recv_from_server(&resp, (long)getpid(), 1) > 0) {
            if (strncmp(resp.payload, "MATCHED:", 8) == 0) {
                sscanf(resp.payload + 8, "%31s HP:%d", opp_name, &my_hp);
                found = 1; break;
            }
        }
        my_idx = find_my_idx();
        if (my_idx >= 0) {
            if (recv_from_server(&resp, 10000 + my_idx, 1) > 0) {
                if (strncmp(resp.payload, "MATCHED:", 8) == 0) {
                    sscanf(resp.payload + 8, "%31s HP:%d", opp_name, &my_hp);
                    found = 1; break;
                }
            }
        }
        printf("."); fflush(stdout);
        sleep(1);
    }

    if (!found) {
        printf("\n  Matchmaking timed out. Returning to menu.\n");
        send_to_server(MSG_CANCEL_MATCH, current_user, NULL, "", 0);
        Message tmp; recv_from_server(&tmp, (long)getpid(), 0);
        sleep(2); return;
    }

    my_idx = find_my_idx();
    if (my_idx < 0) { printf("  Error.\n"); sleep(2); return; }

    battle_running = 1;
    battle_ended   = 0;

    BattleThreadArg *targ = malloc(sizeof(BattleThreadArg));
    targ->my_idx   = my_idx;
    targ->my_type  = 10000 + my_idx;
    targ->pid_type = (long)getpid();
    pthread_t opp_tid;
    pthread_create(&opp_tid, NULL, battle_opponent_thread, targ);

    disable_echo();
    int flags_orig = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags_orig | O_NONBLOCK);

    clear_screen();
    printf("╔══════════════════════════════════════╗\n");
    printf("║           BATTLE START!              ║\n");
    printf("║  [a]=Attack  [u]=Ultimate  [q]=Quit  ║\n");
    printf("╚══════════════════════════════════════╝\n\n");
    printf("  You: %s  vs  Opponent: %s\n\n", current_user, opp_name);
    fflush(stdout);

    time_t last_redraw = 0;
    while (battle_running) {
        char key = 0;
        int rd = read(STDIN_FILENO, &key, 1);

        if (rd > 0) {
            if (key == 'a') {
                send_to_server(MSG_ATTACK, current_user, NULL, "", 0);
                Message r;
                if (recv_from_server(&r, (long)getpid(), 0) > 0) {
                    if (strncmp(r.payload, "ATK:", 4) == 0) {
                        int dmg, myhp, opphp;
                        sscanf(r.payload, "ATK:%d MY_HP:%d OPP_HP:%d", &dmg, &myhp, &opphp);
                        printf("\r  [YOU HIT for %d] Your HP: %d | Opp HP: %d     \n",
                               dmg, myhp, opphp);
                    } else if (strncmp(r.payload, "BATTLE_END:", 11) == 0) {
                        battle_ended   = 1;
                        battle_running = 0;
                        restore_echo();
                        fcntl(STDIN_FILENO, F_SETFL, flags_orig);
                        printf("\n\n  *** %s ***\n", r.payload + 11);
                        fflush(stdout);
                        break;
                    } else {
                        printf("\r  %s     \n", r.payload);
                    }
                    fflush(stdout);
                }
            } else if (key == 'u') {
                send_to_server(MSG_ULTIMATE, current_user, NULL, "", 0);
                Message r;
                if (recv_from_server(&r, (long)getpid(), 0) > 0) {
                    if (strncmp(r.payload, "ATK:", 4) == 0) {
                        int dmg, myhp, opphp;
                        sscanf(r.payload, "ATK:%d MY_HP:%d OPP_HP:%d", &dmg, &myhp, &opphp);
                        printf("\r  [ULTIMATE for %d!!] Your HP: %d | Opp HP: %d  \n",
                               dmg, myhp, opphp);
                    } else {
                        printf("\r  %s     \n", r.payload);
                    }
                    fflush(stdout);
                }
            } else if (key == 'q') {
                battle_running = 0; break;
            }
        }

        time_t now = time(NULL);
        if (now != last_redraw) {
            last_redraw = now;
            my_idx = find_my_idx();
            if (my_idx >= 0 && shm->in_battle[my_idx]) {
                int opp_idx = shm->players[my_idx].battle_opponent_idx;
                printf("\r  HP [ You: %-3d | %s: %-3d ]  (a=attack, u=ultimate, q=quit)  ",
                       shm->hp[my_idx],
                       opp_name,
                       opp_idx >= 0 ? shm->hp[opp_idx] : 0);
                printf("\n");
                print_battle_logs(my_idx);
                printf("\033[%dA", BATTLE_LOGS + 2);
                fflush(stdout);
            }
        }
        usleep(50000);
    }

    restore_echo();
    fcntl(STDIN_FILENO, F_SETFL, flags_orig);
    pthread_join(opp_tid, NULL);

    printf("\n\n");
    if (battle_ended) {
        my_idx = find_my_idx();
        if (my_idx >= 0) {
            Player *p = &shm->players[my_idx];
            printf("  Updated Stats -> Gold: %d | Lvl: %d | XP: %d\n",
                   p->gold, p->level, p->xp);
        }
    }
    printf("  [Press Enter to continue]\n"); fflush(stdout);
    sleep(1);
    int old_fl = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, old_fl & ~O_NONBLOCK);
    getchar();
}
