#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

int main() {
    int pid;
    int wybor;

    printf("Podaj PID procesu docelowego: ");
    scanf("%d", &pid);

    while (1) {
        printf("\nWybierz \n");
        printf("1. stop\n");
        printf("2. start\n");
        printf("3. przelacz konwersje\n");
        printf("4. zakoncz\n");
        printf("Wybor: ");
        scanf("%d", &wybor);

        switch (wybor) {
            case 1:
                kill(pid, SIGUSR1);
                printf("Wyslam sygnal %d do procesu %d\n", SIGUSR1, pid);
                break;
            case 2:
                kill(pid, SIGUSR2);
                printf("Wyslam sygnal %d do procesu %d\n", SIGUSR2, pid);
                break;
            case 3:
                kill(pid, SIGRTMIN);
                printf("Wyslam sygnal %d do procesu %d\n", SIGRTMIN, pid);
                break;
            case 4:
                kill(pid, SIGTERM);
                printf("Wyslam sygnal %d do procesu %d\n", SIGTERM, pid);
                return 0;
            default:
                printf("Nieznana opcja\n");
        }
    }
    return 0;
}
