#ifndef ARENA_H
#define ARENA_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>

/* ========== IPC KEYS ========== */
#define SHM_KEY     0x00001234
#define MSGQ_KEY    0x00005678
#define SEM_KEY     0x00009012

/* ========== CONSTANTS ========== */
#define MAX_PLAYERS     32
#define MAX_WEAPONS     16
#define MAX_HISTORY     50
#define MAX_NAME_LEN    32
#define MAX_PASS_LEN    32
#define MAX_LOG_LEN     128
#define BATTLE_LOGS     5

#define BASE_DAMAGE     10
#define BASE_HEALTH     100
#define BASE_GOLD       150
#define BASE_LEVEL      1
#define BASE_XP         0

#define XP_WIN          50
#define XP_LOSE         15
#define GOLD_WIN        120
#define GOLD_LOSE       30
#define XP_PER_LEVEL    100

#define MATCHMAKING_TIMEOUT 35
#define ATTACK_COOLDOWN     1

/* ========== MESSAGE TYPES ========== */
#define MSG_REGISTER    1
#define MSG_LOGIN       2
#define MSG_LOGOUT      3
#define MSG_MATCHMAKE   4
#define MSG_ATTACK      5
#define MSG_ULTIMATE    6
#define MSG_BUY_WEAPON  7
#define MSG_HISTORY     8
#define MSG_RESPONSE    9
#define MSG_BATTLE_UPDATE 10
#define MSG_CANCEL_MATCH  11

/* ========== PLAYER STATUS ========== */
#define STATUS_OFFLINE      0
#define STATUS_ONLINE       1
#define STATUS_MATCHMAKING  2
#define STATUS_BATTLE       3

/* ========== WEAPON DATA ========== */
typedef struct {
    char name[MAX_NAME_LEN];
    int  bonus_damage;
    int  price;
} Weapon;

static const Weapon WEAPONS[] = {
    {"Iron Sword",   10,  50},
    {"Steel Blade",  25, 100},
    {"Flame Axe",    45, 200},
    {"Thunder Bow",  70, 350},
    {"Shadow Dagger",100, 500},
};
#define WEAPON_COUNT 5

/* ========== STRUCTS ========== */
typedef struct {
    char username[MAX_NAME_LEN];
    char password[MAX_PASS_LEN];
    int  gold;
    int  level;
    int  xp;
    int  status;
    int  weapon_damage;
    int  has_weapon;
    int  battle_opponent_idx;
    char history[MAX_HISTORY][MAX_LOG_LEN];
    int  history_count;
} Player;

typedef struct {
    Player  players[MAX_PLAYERS];
    int     player_count;
    char    battle_logs[MAX_PLAYERS][BATTLE_LOGS][MAX_LOG_LEN];
    int     battle_log_count[MAX_PLAYERS];
    int     hp[MAX_PLAYERS];
    int     in_battle[MAX_PLAYERS];
    time_t  last_attack[MAX_PLAYERS];
} SharedData;

/* ========== MESSAGE QUEUE STRUCT ========== */
typedef struct {
    long mtype;
    char sender[MAX_NAME_LEN];
    char target[MAX_NAME_LEN];
    char payload[256];
    int  ivalue;
    int  client_pid;
} Message;

/* ========== SEMAPHORE HELPERS ========== */
static inline void sem_lock(int semid) {
    struct sembuf sb = {0, -1, SEM_UNDO};
    semop(semid, &sb, 1);
}
static inline void sem_unlock(int semid) {
    struct sembuf sb = {0, 1, SEM_UNDO};
    semop(semid, &sb, 1);
}

/* ========== UTILITY ========== */
static inline int calc_damage(int xp, int weapon_bonus) {
    return BASE_DAMAGE + (xp / 50) + weapon_bonus;
}
static inline int calc_health(int xp) {
    return BASE_HEALTH + (xp / 10);
}
static inline int calc_ultimate(int total_damage) {
    return total_damage * 3;
}

#endif /* ARENA_H */
