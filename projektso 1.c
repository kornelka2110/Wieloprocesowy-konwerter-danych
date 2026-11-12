#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h>

#define SYGNAL_KONIEC SIGTERM
#define SYGNAL_STOP   SIGUSR1
#define SYGNAL_START  SIGUSR2
#define SYGNAL_HEX    SIGRTMIN

#define KLUCZ_SHM 12345
#define KLUCZ_SEM 54321
#define PLIK_K2 "komunikacja.txt"

struct PamiecDzielona {
    char bufor[10];
    pid_t pid_p1;
    pid_t pid_p2;
    pid_t pid_p3;
    volatile int tryb_pracy;
};

int shmid;
int semid;
struct PamiecDzielona *shm_ptr;
volatile int czy_pauza = 0;

void sem_p(int sem_id, int sem_num) {
    struct sembuf operacja;
    operacja.sem_num = sem_num;
    operacja.sem_op = -1;
    operacja.sem_flg = 0;
    while (semop(sem_id, &operacja, 1) == -1) {
        if (errno != EINTR) break;
    }
}

void sem_v(int sem_id, int sem_num) {
    struct sembuf operacja;
    operacja.sem_num = sem_num;
    operacja.sem_op = 1;
    operacja.sem_flg = 0;
    semop(sem_id, &operacja, 1);
}

int czy_sygnal_wewnetrzny(pid_t sender_pid) {
    if (sender_pid == shm_ptr->pid_p1) return 1;
    if (sender_pid == shm_ptr->pid_p2) return 1;
    if (sender_pid == shm_ptr->pid_p3) return 1;
    return 0;
}

void powiadom_inne(int sig) {
    pid_t ja = getpid();
    if (shm_ptr->pid_p1 > 0 && shm_ptr->pid_p1 != ja) kill(shm_ptr->pid_p1, sig);
    if (shm_ptr->pid_p2 > 0 && shm_ptr->pid_p2 != ja) kill(shm_ptr->pid_p2, sig);
    if (shm_ptr->pid_p3 > 0 && shm_ptr->pid_p3 != ja) kill(shm_ptr->pid_p3, sig);
}

void handler_akcji(int sig, siginfo_t *info, void *context) {
    (void)context;
    int sender = info->si_pid;
    int od_rodziny = czy_sygnal_wewnetrzny(sender);

    if (sig == SYGNAL_KONIEC) {
        if (!od_rodziny) powiadom_inne(sig);
        exit(0);
    } else if (sig == SYGNAL_STOP) {
        if (czy_pauza == 0) {
            czy_pauza = 1;
            if (!od_rodziny) powiadom_inne(sig);
        }
    } else if (sig == SYGNAL_START) {
        if (czy_pauza == 1) {
            czy_pauza = 0;
            if (!od_rodziny) powiadom_inne(sig);
        }
    } else if (sig == SYGNAL_HEX) {
        if (!od_rodziny) {
            shm_ptr->tryb_pracy = (shm_ptr->tryb_pracy + 1) % 3;
            powiadom_inne(sig);
        }
    }
}

void ustaw_sygnaly() {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sa.sa_sigaction = handler_akcji;
    sigemptyset(&sa.sa_mask);

    sigaction(SYGNAL_KONIEC, &sa, NULL);
    sigaction(SYGNAL_STOP, &sa, NULL);
    sigaction(SYGNAL_START, &sa, NULL);
    sigaction(SYGNAL_HEX, &sa, NULL);
}

void proces_1() {
    ustaw_sygnaly();
    char ch;
    char bufor_wyjsciowy[10];
    int n;

    while (1) {
        usleep(1000);
        while (czy_pauza) { usleep(100000); }

        n = read(STDIN_FILENO, &ch, 1);
        if (ch == '\n') continue;
        if (n == 0) break;
        if (n < 0) continue;

        int tryb = shm_ptr->tryb_pracy;

        if (tryb == 1) {
            sprintf(bufor_wyjsciowy, "0x%02X ", (unsigned char)ch);
        } else if (tryb == 2) {
            sprintf(bufor_wyjsciowy, "%d ", (unsigned char)ch);
        } else {
            char c_safe = (isprint((unsigned char)ch) && ch != '\n') ? ch : '.';
            sprintf(bufor_wyjsciowy, "%c ", c_safe);
        }

        sem_p(semid, 0);
        strcpy(shm_ptr->bufor, bufor_wyjsciowy);
        sem_v(semid, 1);
    }
    kill(getpid(), SYGNAL_KONIEC);
}

void proces_2() {
    ustaw_sygnaly();
    FILE *plik = fopen(PLIK_K2, "w");
    if (!plik) { perror("Błąd pliku K2"); exit(1); }

    while (1) {
        while (czy_pauza) { usleep(100000); }

        sem_p(semid, 1);
        if (fputs(shm_ptr->bufor, plik) == EOF) {}
        fflush(plik);
        sem_v(semid, 0);
    }
    fclose(plik);
}

void proces_3() {
    ustaw_sygnaly();
    FILE *plik = NULL;

    while (!plik) {
        plik = fopen(PLIK_K2, "r");
        usleep(10000);
    }

    int licznik_elementow = 0;
    int c;
    while (1) {
        while (czy_pauza) { usleep(100000); }

        c = fgetc(plik);
        if (c == EOF) {
            clearerr(plik);
            usleep(10000);
            continue;
        }

        if (!isprint(c) && c != ' ' && c != '\n') {
            continue;
        }

        fprintf(stderr, "%c", (char)c);

        if (c == ' ') {
            licznik_elementow++;
            if (licznik_elementow >= 15) {
                fprintf(stderr, "\n");
                licznik_elementow = 0;
            }
        }
    }
}

int main() {
    int wybor_zrodla = 0;
    char nazwa_pliku[256];

    printf("menu\n");
    printf("1. klawiatura\n");
    printf("2. Plik tekstowy\n");
    printf("3. /dev/urandom\n");
    printf("podaj wybor: ");
    if (scanf("%d", &wybor_zrodla) != 1) return 1;

    if (wybor_zrodla == 2) {
        printf("podaj nazwe pliku: ");
        scanf("%255s", nazwa_pliku);
    }

    shmid = shmget(KLUCZ_SHM, sizeof(struct PamiecDzielona), 0666 | IPC_CREAT);
    if (shmid == -1) { perror("shmget"); return 1; }
    shm_ptr = (struct PamiecDzielona*) shmat(shmid, NULL, 0);

    shm_ptr->pid_p1 = 0;
    shm_ptr->pid_p2 = 0;
    shm_ptr->pid_p3 = 0;
    shm_ptr->tryb_pracy = 1;

    semid = semget(KLUCZ_SEM, 2, 0666 | IPC_CREAT);
    semctl(semid, 0, SETVAL, 1);
    semctl(semid, 1, SETVAL, 0);

    ustaw_sygnaly();

    printf("PID Glownego: %d. Oczekiwanie...\n", getpid());

    pid_t pid1 = fork();
    if (pid1 == 0) {
        int fd;
        if (wybor_zrodla == 2) {
            fd = open(nazwa_pliku, O_RDONLY);
            if (fd < 0) { perror("Błąd pliku"); exit(1); }
            dup2(fd, STDIN_FILENO);
            close(fd);
        } else if (wybor_zrodla == 3) {
            fd = open("/dev/urandom", O_RDONLY);
            if (fd < 0) { perror("Błąd urandom"); exit(1); }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        proces_1();
        exit(0);
    }
    shm_ptr->pid_p1 = pid1;
    printf("P1: %d\n", pid1);

    pid_t pid2 = fork();
    if (pid2 == 0) { proces_2(); exit(0); }
    shm_ptr->pid_p2 = pid2;
    printf("P2: %d\n", pid2);

    pid_t pid3 = fork();
    if (pid3 == 0) { proces_3(); exit(0); }
    shm_ptr->pid_p3 = pid3;
    printf("P3: %d\n", pid3);

    int status;
    for (int i = 0; i < 3; i++) wait(&status);

    printf("\nSprzatanie\n");

    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    unlink(PLIK_K2);

    return 0;
}
