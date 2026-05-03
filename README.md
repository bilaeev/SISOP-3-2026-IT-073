## Soal 1
## Penjelasan kode soal 1
Ketiga file membentuk satu sistem client-server dengan protokol komunikasi yang sama.
> protocol.h berfungsi sebagai definisi aturan komunikasi (protokol), berisi konstanta tipe pesan, RPC, dan struktur data Packet yang digunakan oleh server dan client.

> Wired.c adalah server yang menerima koneksi dari banyak client, mengelola data client, melakukan broadcast pesan, menjalankan RPC untuk admin, serta mencatat log.

> Navi.c adalah client yang terhubung ke server, mengirim pesan, menerima pesan secara asynchronous, serta menyediakan interface admin jika login sebagai “The Knights”.

> Semua komunikasi antara client dan server menggunakan socket dan dikirim dalam bentuk struct Packet. Ini merupakan implementasi message passing pada IPC (Modul 3).  
## Penjelasan protocol.h
File ini berisi definisi protokol komunikasi.  
Bagian alamat server:
```c
#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 9000
```
Digunakan client untuk melakukan koneksi ke server.  
Tipe pesan client ke server:
```c
#define MSG_REGISTER    1
#define MSG_CHAT        2
#define MSG_EXIT        3
#define MSG_ADMIN_AUTH  4
#define MSG_RPC         5
```
Tipe pesan server ke client:
```c
#define MSG_ACCEPT      10
#define MSG_NAME_TAKEN  11
#define MSG_BROADCAST   12
#define MSG_RPC_RESP    13
#define MSG_SHUTDOWN    14
````
RPC command:
```c
#define RPC_GET_USERS   1
#define RPC_GET_UPTIME  2
#define RPC_SHUTDOWN    3
```
Struktur data utama:
```c
typedef struct {
    int  type;
    char sender[MAX_NAME];
    char body[MAX_MSG];
} Packet;
```
Struct ini digunakan dalam semua komunikasi. Ini adalah implementasi message passing (Modul 3).
## Penjelasan Wired.c (Server)
File ini adalah server utama yang menangani banyak client secara bersamaan.  
Struktur data client:
```c
typedef struct {
    int     fd;
    char    name[MAX_NAME];
    int     is_admin;
    int     active;
} Client;
```
Digunakan untuk menyimpan informasi tiap client.
```c
pthread_mutex_t clients_mtx = PTHREAD_MUTEX_INITIALIZER;
```
Digunakan untuk menghindari race condition saat banyak thread mengakses data client.  
Contoh penggunaan:
```c
pthread_mutex_lock(&clients_mtx);
...
pthread_mutex_unlock(&clients_mtx);
Broadcast (Message Passing – Modul 3)
```
```c
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
```
Server mengirim pesan ke semua client aktif kecuali pengirim. -Multithreading (Modul 3)
```c
pthread_create(&tid, NULL, client_thread, targ);
pthread_detach(tid);
```
Setiap client ditangani oleh thread sendiri sehingga server tidak terblokir oleh satu client. Fungsi client_thread, fungsi ini menangani seluruh komunikasi dengan satu client.  
Registrasi nama:
```c
if (pkt.type != MSG_REGISTER) continue;

int taken = name_exists(pkt.sender);
```
Jika nama sudah digunakan, server mengirim MSG_NAME_TAKEN. Jika tidak, nama diterima.  
Autentikasi admin:
```c
if (strcmp(c->name, ADMIN_NAME) == 0)
```
```c
if (pkt.type == MSG_ADMIN_AUTH &&
    strcmp(pkt.body, ADMIN_PASSWORD) == 0)
```
Hanya admin yang bisa mengakses RPC.  
Chat:
```c
if (pkt.type == MSG_CHAT && !c->is_admin)
```
```c
broadcast(&bcast, fd);
```
Pesan dari client dikirim ke semua client lain.  
Exit:
```c
if (pkt.type == MSG_EXIT)
```
Client keluar secara normal. -RPC (Modul 3)
```c
if (pkt.type == MSG_RPC && c->is_admin)
```
```c
handle_rpc(idx, cmd);
```
Menjalankan perintah admin seperti: jumlah user aktif, uptime server, shutdown server.  
Contoh RPC:  
```c
case RPC_GET_USERS:
    int n = count_users();
```
```c
case RPC_GET_UPTIME:
    long uptime = (long)(time(NULL) - server_start);
```
```c
case RPC_SHUTDOWN:
    exit(0);
Logging (Modul 2 – File Handling)
```
```c
log_fp = fopen("history.log", "a");
```
```c
fprintf(log_fp, "[%s] [%s] [%s]\n", ts, category, message);
Setiap aktivitas disimpan ke file.
Socket Programming (Modul 3)
```
```c
server_fd = socket(AF_INET, SOCK_STREAM, 0);
bind(...)
listen(...)
accept(...)
```
Server menerima koneksi dari client.
## Penjelasan Navi.c (Client)
Client digunakan untuk berkomunikasi dengan server.  -Socket Connection (Modul 3)
```c
connect(sock_fd, (struct sockaddr *)&srv, sizeof(srv))
```
Client terhubung ke server. -Thread Receiver (Asynchronous – Modul 3)
```c
pthread_create(&rtid, NULL, recv_thread, NULL);
```
Fungsi:
```c
int n = recv(sock_fd, &pkt, sizeof(pkt), 0);
```
Thread ini terus menerima pesan dari server tanpa mengganggu input user.  
Registrasi nama
```c
pkt.type = MSG_REGISTER;
send(sock_fd, &pkt, sizeof(pkt), 0);
```
Client mengirim nama ke server dan menunggu respon.  
Admin login
```c
if (strcmp(resp.body, "NEED_PASSWORD") == 0)
```
```c
pkt.type = MSG_ADMIN_AUTH;
```
Jika nama adalah “The Knights”, client diminta password.  
Chat
```c
pkt.type = MSG_CHAT;
send(sock_fd, &pkt, sizeof(pkt), 0);
```
Mengirim pesan ke server.  
Exit
```c
pkt.type = MSG_EXIT;
Client keluar dari server.
Admin Console (RPC)
```
```c
pkt.type = MSG_RPC;
snprintf(pkt.body, MAX_MSG, "%d", cmd);
```
Mengirim perintah RPC ke server, mapping ke Modul

## Output
1. menjalankan server  
   <img width="467" height="40" alt="Screenshot 2026-05-03 190855" src="https://github.com/user-attachments/assets/21b34da7-561b-44b3-bfb0-ae8adc3044a4" />  
2. connect ke tiap client  
   <img width="492" height="76" alt="Screenshot 2026-05-03 190956" src="https://github.com/user-attachments/assets/36dad913-d10f-4bde-bfb8-db4c93478e2d" />  
3. error handling untuk pemanggilan navi apabila server mati  
   <img width="444" height="57" alt="Screenshot 2026-05-03 191113" src="https://github.com/user-attachments/assets/ec3d1bb2-fddd-453d-8470-c0f57319e9c1" />  
4. error handling apabila server sudah online tapi ada pemanggilan wired lagi di terminal lain  
   <img width="500" height="75" alt="Screenshot 2026-05-03 191327" src="https://github.com/user-attachments/assets/314f0e3f-8b8e-458d-9bb3-72aa786ea8c2" />  
5. mendapatkan pesan dari client lain yang aktif  
   TERMINAL 1  
   <img width="498" height="155" alt="Screenshot 2026-05-03 192709" src="https://github.com/user-attachments/assets/48071b60-d6e0-4d27-a0d5-2f1aae9a0caf" />  
   TERMINAL 2  
   <img width="500" height="75" alt="Screenshot 2026-05-03 191327" src="https://github.com/user-attachments/assets/49276999-8af3-4a36-92ba-d90935cbfbab" />  
6. error handling apabila server mati  
   TERMINAL CLIENT  
   <img width="511" height="96" alt="Screenshot 2026-05-03 193021" src="https://github.com/user-attachments/assets/e6816be7-f327-4389-8548-f0bbcb6207bf" />  
   TERMINAL SERVER  
   <img width="456" height="82" alt="Screenshot 2026-05-03 193101" src="https://github.com/user-attachments/assets/df88cbcc-922c-4e40-950b-51cda9445da2" />
7. client kirim pesan kosong (prompt muncul lagi, tidak crash, tidak dikirim)  
   <img width="515" height="186" alt="Screenshot 2026-05-03 193359" src="https://github.com/user-attachments/assets/e18119eb-26e6-46b4-9a9d-77cecda8ae5a" />  
8. salah satu client exit  
   TERMINAL CLIENT YANG KELUAR SERVER  
   <img width="439" height="40" alt="Screenshot 2026-05-03 193644" src="https://github.com/user-attachments/assets/c199fda2-ca46-41d6-be04-98a5f288640b" />
   TERMINAL CLIENT LAIN  
   <img width="343" height="23" alt="Screenshot 2026-05-03 193722" src="https://github.com/user-attachments/assets/c77d1158-db46-4366-87b8-985fb047627e" />
9. nama duplikat ditolak dan langsung meminta input nama lain lagi  
    <img width="602" height="97" alt="Screenshot 2026-05-03 193910" src="https://github.com/user-attachments/assets/af1b7b9d-a640-46e0-b31a-8723c8611ef7" />
10. error handling enter nama kosong  
    <img width="495" height="122" alt="Screenshot 2026-05-03 194024" src="https://github.com/user-attachments/assets/113d7834-b610-4685-b2e9-de95da0d2807" />
11. password The Knights salah  
    <img width="488" height="78" alt="Screenshot 2026-05-03 194118" src="https://github.com/user-attachments/assets/09fdfdb2-1fae-4476-9265-965c83f6a60c" />
12. output name=The Knights  
    <img width="620" height="422" alt="Screenshot 2026-05-03 194244" src="https://github.com/user-attachments/assets/91d305e2-f62e-46be-a1ae-c410f46c3a4f" />
    command 3 membuat server mati, command 4 tidak memunculkan output apapun tetapi The Knights keluar dari server & server dan client lain tetap jalan
13. error handling apabila input command The Knights selain 1,2,3,4  
    <img width="318" height="220" alt="Screenshot 2026-05-03 194557" src="https://github.com/user-attachments/assets/f9ed3a30-d8d5-43c4-ac03-a7e58e76b778" />  
14. isi history.log  
    <img width="588" height="404" alt="Screenshot 2026-05-03 194719" src="https://github.com/user-attachments/assets/9b65e7cc-111b-498a-9b78-1100391b855c" />

## Soal 2
## Penjelasan kode soal 2
Keempat file membentuk satu sistem client-server berbasis IPC untuk arena pertempuran real-time.
> arena.h berfungsi sebagai header utama yang mendefinisikan seluruh konstanta, struct, key IPC, serta fungsi utilitas (calc_damage, calc_health, calc_ultimate, sem_lock, sem_unlock) yang digunakan bersama oleh server dan client.

> orion.c adalah server yang menginisialisasi resource IPC (Shared Memory, Message Queue, Semaphore), mengelola data seluruh pemain, menangani matchmaking, battle real-time, pembelian senjata, serta menyimpan history pertempuran.

> eternal.c adalah client yang terhubung ke server melalui IPC, menyediakan antarmuka main menu, login/register, battle real-time dengan thread asynchronous, shop senjata, profile, dan history pertempuran.

> Semua komunikasi antara client dan server menggunakan Message Queue (msgget, msgsnd, msgrcv) dan pertukaran data kondisi game menggunakan Shared Memory. Ini merupakan implementasi IPC (Modul 4).

## Penjelasan arena.h
File ini berisi seluruh definisi yang digunakan bersama oleh orion.c dan eternal.c.  
Bagian IPC Keys:
```c
#define SHM_KEY     0x00001234
#define MSGQ_KEY    0x00005678
#define SEM_KEY     0x00009012
```
Digunakan untuk membuat dan mengakses resource IPC yang sama antara server dan client.  
Konstanta game:
```c
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
```
Mendefinisikan semua nilai default dan aturan game secara terpusat.  
Tipe pesan untuk Message Queue:
```c
#define MSG_REGISTER    1
#define MSG_LOGIN       2
#define MSG_LOGOUT      3
#define MSG_MATCHMAKE   4
#define MSG_ATTACK      5
#define MSG_ULTIMATE    6
#define MSG_BUY_WEAPON  7
#define MSG_HISTORY     8
#define MSG_CANCEL_MATCH  11
```
Digunakan sebagai mtype pada Message Queue untuk membedakan jenis permintaan dari client ke server.  
Status pemain:
```c
#define STATUS_OFFLINE      0
#define STATUS_ONLINE       1
#define STATUS_MATCHMAKING  2
#define STATUS_BATTLE       3
```
Melacak kondisi setiap pemain agar server dapat mengelola matchmaking dan battle dengan benar.  
Struct Weapon dan data senjata:
```c
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
```
Mendefinisikan 5 senjata yang tersedia di armory beserta harga dan bonus damage-nya.  
Struct Player:
```c
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
```
Menyimpan seluruh data persistent pemain di Shared Memory sehingga data tetap ada selama server berjalan.  
Struct SharedData:
```c
typedef struct {
    Player  players[MAX_PLAYERS];
    int     player_count;
    char    battle_logs[MAX_PLAYERS][BATTLE_LOGS][MAX_LOG_LEN];
    int     battle_log_count[MAX_PLAYERS];
    int     hp[MAX_PLAYERS];
    int     in_battle[MAX_PLAYERS];
    time_t  last_attack[MAX_PLAYERS];
} SharedData;
```
Blok utama Shared Memory yang diakses langsung oleh server dan client untuk membaca kondisi game secara real-time.  
Struct Message untuk Message Queue:
```c
typedef struct {
    long mtype;
    char sender[MAX_NAME_LEN];
    char target[MAX_NAME_LEN];
    char payload[256];
    int  ivalue;
    int  client_pid;
} Message;
```
Digunakan dalam semua komunikasi antar proses. Field mtype menentukan routing pesan, client_pid digunakan sebagai reply address unik ke tiap client.  
Semaphore helper:
```c
static inline void sem_lock(int semid) {
    struct sembuf sb = {0, -1, SEM_UNDO};
    semop(semid, &sb, 1);
}
static inline void sem_unlock(int semid) {
    struct sembuf sb = {0, 1, SEM_UNDO};
    semop(semid, &sb, 1);
}
```
Digunakan untuk melindungi akses ke Shared Memory dari race condition ketika banyak client mengakses data secara bersamaan.  
Formula utilitas:
```c
static inline int calc_damage(int xp, int weapon_bonus) {
    return BASE_DAMAGE + (xp / 50) + weapon_bonus;
}
static inline int calc_health(int xp) {
    return BASE_HEALTH + (xp / 10);
}
static inline int calc_ultimate(int total_damage) {
    return total_damage * 3;
}
```
Formula damage, health, dan ultimate dipusatkan di sini agar konsisten digunakan oleh server saat menghitung hasil serangan.

## Penjelasan orion.c (Server)
File ini adalah server utama yang mengelola seluruh logika game berbasis IPC.  
Inisialisasi IPC:
```c
shmid = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
shm = (SharedData *)shmat(shmid, NULL, 0);
msgqid = msgget(MSGQ_KEY, IPC_CREAT | 0666);
semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
semctl(semid, 0, SETVAL, 1);
```
Server membuat Shared Memory, Message Queue, dan Semaphore saat pertama kali dijalankan. Client kemudian mengakses resource yang sama menggunakan key yang identik.  
Main loop server:
```c
while (running) {
    if (msgrcv(msgqid, &req, sizeof(req) - sizeof(long), 0, 0) < 0) { ... }
    switch (req.mtype) {
        case MSG_REGISTER:     handle_register(&req);     break;
        case MSG_LOGIN:        handle_login(&req);        break;
        case MSG_LOGOUT:       handle_logout(&req);       break;
        case MSG_MATCHMAKE:    handle_matchmake(&req);    break;
        case MSG_ATTACK:       handle_attack(&req);       break;
        case MSG_ULTIMATE:     handle_ultimate(&req);     break;
        case MSG_BUY_WEAPON:   handle_buy_weapon(&req);   break;
        case MSG_HISTORY:      handle_history(&req);      break;
    }
}
```
Server memblokir di msgrcv menunggu pesan masuk dari client manapun, lalu mendispatch ke handler yang sesuai berdasarkan tipe pesan.  
Register dan Login (Persistent Data – Modul 4):
```c
int idx = shm->player_count++;
Player *p = &shm->players[idx];
strncpy(p->username, req->sender,  MAX_NAME_LEN - 1);
strncpy(p->password, req->payload, MAX_PASS_LEN  - 1);
p->gold  = BASE_GOLD;
p->level = BASE_LEVEL;
p->xp    = BASE_XP;
```
Data pemain disimpan langsung di Shared Memory sehingga persistent selama server berjalan. Username dijamin unik melalui pengecekan find_player sebelum registrasi.  
Proteksi Race Condition (Semaphore – Modul 4):
```c
sem_lock(semid);
// akses dan modifikasi SharedData
sem_unlock(semid);
```
Setiap handler membungkus akses ke Shared Memory dengan sem_lock dan sem_unlock untuk mencegah race condition saat banyak client beroperasi bersamaan.  
Matchmaking:
```c
shm->players[idx].status = STATUS_MATCHMAKING;
int opp = -1;
for (int i = 0; i < shm->player_count; i++) {
    if (i == idx) continue;
    if (shm->players[i].status == STATUS_MATCHMAKING) { opp = i; break; }
}
```
Server mencari pemain lain yang sedang dalam status matchmaking. Jika ditemukan, keduanya langsung dipasangkan dan status diubah ke STATUS_BATTLE.  
Matchmaking Watchdog Thread:
```c
pthread_create(&wd_tid, NULL, matchmaking_watchdog, NULL);
pthread_detach(wd_tid);
```
Thread background yang berjalan setiap 1 detik untuk mengecek pemain yang sudah menunggu lebih dari MATCHMAKING_TIMEOUT (35 detik), kemudian mempertemukannya dengan bot BOT_Golem secara otomatis.  
Logika Battle Real-Time:
```c
static void do_attack_logic(int attacker_idx, int defender_idx, int is_ultimate, long resp_type) {
    time_t now = time(NULL);
    if (now - shm->last_attack[attacker_idx] < ATTACK_COOLDOWN) {
        send_response(resp_type, "COOLDOWN", 0);
        return;
    }
    int dmg = calc_damage(atk->xp, atk->weapon_damage);
    if (is_ultimate) dmg = calc_ultimate(dmg);
    shm->hp[defender_idx] -= dmg;
}
```
Sistem battle tidak menggunakan turn-based, setiap pemain dapat menyerang kapan saja dengan cooldown 1 detik. Ultimate hanya bisa dipakai jika pemain memiliki senjata.  
Battle End dan Update Stats:
```c
atk->xp   += XP_WIN;
atk->gold += GOLD_WIN;
def->xp   += XP_LOSE;
def->gold += GOLD_LOSE;
atk->level = 1 + atk->xp / XP_PER_LEVEL;
def->level = 1 + def->xp / XP_PER_LEVEL;
```
Ketika HP defender mencapai 0, server otomatis mengupdate XP, gold, dan level kedua pemain sesuai formula, lalu mengirim notifikasi BATTLE_END ke masing-masing client.  
History pertempuran:
```c
snprintf(hist, sizeof(hist), "WIN vs %s | +%d XP +%d Gold", def->username, XP_WIN, GOLD_WIN);
add_history(attacker_idx, hist);
snprintf(hist, sizeof(hist), "LOSE vs %s | +%d XP +%d Gold", atk->username, XP_LOSE, GOLD_LOSE);
add_history(defender_idx, hist);
```
Hasil setiap pertempuran dicatat di array history dalam struct Player di Shared Memory, tersimpan selama server aktif.  
Pembelian senjata:
```c
p->gold -= WEAPONS[wid].price;
if (WEAPONS[wid].bonus_damage > p->weapon_damage)
    p->weapon_damage = WEAPONS[wid].bonus_damage;
p->has_weapon = 1;
```
Pemain otomatis menggunakan senjata dengan bonus damage terbesar. Weapon dengan bonus lebih rendah tidak akan menggantikan senjata yang sudah dimiliki.

## Penjelasan eternal.c (Client)
Client digunakan untuk berkomunikasi dengan server melalui IPC dan menyediakan antarmuka interaktif bagi pemain.  
Koneksi ke IPC Server:
```c
msgqid = msgget(MSGQ_KEY, 0666);
shmid = shmget(SHM_KEY, sizeof(SharedData), 0666);
shm = (SharedData *)shmat(shmid, NULL, 0);
semid = semget(SEM_KEY, 1, 0666);
```
Client mengakses resource IPC yang sudah dibuat server menggunakan key yang sama. Jika server belum berjalan, client langsung keluar dengan pesan error.  
Fungsi send dan receive:
```c
void send_to_server(long mtype, const char *sender, const char *target,
                    const char *payload, int ival) {
    Message m;
    memset(&m, 0, sizeof(m));
    m.mtype      = mtype;
    m.client_pid = (int)getpid();
    if (sender)  strncpy(m.sender,  sender,  MAX_NAME_LEN - 1);
    if (payload) strncpy(m.payload, payload, sizeof(m.payload) - 1);
    m.ivalue = ival;
    msgsnd(msgqid, &m, sizeof(m) - sizeof(long), 0);
}

int recv_from_server(Message *out, long type, int nowait) {
    int flags = nowait ? IPC_NOWAIT : 0;
    ssize_t r = msgrcv(msgqid, out, sizeof(*out) - sizeof(long), type, flags);
    return (int)r;
}
```
Semua komunikasi dengan server dikemas dalam dua fungsi ini. Client menggunakan PID-nya sendiri (getpid()) sebagai mtype reply sehingga setiap client hanya membaca response yang ditujukan untuknya.  
Register dan Login:
```c
send_to_server(MSG_REGISTER, uname, NULL, pass, 0);
Message resp;
recv_from_server(&resp, (long)getpid(), 0);
printf("\n  %s\n", resp.payload);
```
Client mengirim username dan password ke server, lalu menunggu response. Jika login berhasil (resp.ivalue == 1), client masuk ke game_menu().  
Thread Battle Asynchronous (Multithreading – Modul 4):
```c
pthread_create(&opp_tid, NULL, battle_opponent_thread, targ);
```
Saat battle dimulai, client membuat thread terpisah untuk terus memantau serangan dari lawan secara asynchronous, sehingga pemain tetap bisa menyerang tanpa harus menunggu giliran.  
Receiver thread battle:
```c
void *battle_opponent_thread(void *arg) {
    while (battle_running) {
        if (recv_from_server(&m, a->my_type, 1) > 0) {
            if (strncmp(m.payload, "BATTLE_END:", 11) == 0) {
                battle_ended = 1; battle_running = 0;
            } else if (strncmp(m.payload, "ATK:", 4) == 0) {
                printf("\r  [OPP ATTACKS for %d!] Your HP: %d\n", dmg, my_hp);
            }
        }
        usleep(100000);
    }
}
```
Thread ini membaca pesan dari server setiap 100ms menggunakan IPC_NOWAIT agar tidak memblokir input pemain.  
Kontrol keyboard non-blocking saat battle:
```c
disable_echo();
fcntl(STDIN_FILENO, F_SETFL, flags_orig | O_NONBLOCK);

char key = 0;
int rd = read(STDIN_FILENO, &key, 1);
if (rd > 0) {
    if (key == 'a') send_to_server(MSG_ATTACK, ...);
    else if (key == 'u') send_to_server(MSG_ULTIMATE, ...);
    else if (key == 'q') battle_running = 0;
}
```
Input keyboard diset non-blocking dan echo dimatikan agar tombol 'a', 'u', 'q' langsung diproses tanpa harus menekan Enter, sesuai sistem battle real-time.  
Tampilan HP real-time dan battle log:
```c
printf("\r  HP [ You: %-3d | %s: %-3d ]  (a=attack, u=ultimate, q=quit)  ",
       shm->hp[my_idx], opp_name, opp_idx >= 0 ? shm->hp[opp_idx] : 0);
print_battle_logs(my_idx);
printf("\033[%dA", BATTLE_LOGS + 2);
```
Client membaca HP langsung dari Shared Memory setiap detik dan menampilkan 5 log battle terakhir secara in-place menggunakan ANSI escape code untuk efek real-time.  
Shop (Armory):
```c
send_to_server(MSG_BUY_WEAPON, current_user, NULL, "", c - 1);
Message resp;
recv_from_server(&resp, (long)getpid(), 0);
printf("\n  %s\n", resp.payload);
```
Client mengirim index senjata yang dipilih ke server, server memvalidasi gold dan memproses pembelian, lalu mengirim konfirmasi kembali ke client.  
Profile:
```c
Player *p = &shm->players[my_idx];
printf("  Damage   : %d\n",  calc_damage(p->xp, p->weapon_damage));
printf("  Max HP   : %d\n",  calc_health(p->xp));
if (p->has_weapon)
    printf("  Ultimate : %d\n",  calc_ultimate(calc_damage(p->xp, p->weapon_damage)));
```
Client membaca data pemain langsung dari Shared Memory dan menghitung stats menggunakan formula dari arena.h.
## revisi
1. username duplikat  
   <img width="346" height="157" alt="Screenshot 2026-05-04 003427" src="https://github.com/user-attachments/assets/e6d363fc-619a-4501-a856-b8890eccef65" />
2. login dengan username yang belum register  
   <img width="277" height="157" alt="Screenshot 2026-05-04 003437" src="https://github.com/user-attachments/assets/f9cf6f30-0d68-42c1-b350-6a830e6f51dd" />
3. login dengan password salah  
   <img width="235" height="153" alt="Screenshot 2026-05-04 003455" src="https://github.com/user-attachments/assets/824473ff-76e8-45a7-b59a-58b9809e7f6e" />


   
