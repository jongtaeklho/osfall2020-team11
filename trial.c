#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>

#define SCHED_WRR 7
#define SCHED_SETWEIGHT 398
#define SCHED_GETWEIGHT 399


void factorize_prime(int n_origin) {
    int n;
    int i, j;
    if (n_origin > 0)
    {
        for (j = 0; j < 10; j++)
        {
            n = n_origin;

            printf("%lld = ", n_origin);
            while (!(n % 2))
            {
                n /= 2;
                printf("2*");
            }
            i = 3;
            while (i * i <= n)
            {
                if (!(n % i))
                {
                    n /= (long long)i;
                    printf("%d*", i);
                }
                else
                {
                    i += 2;
                }
            }
            if (n > 1)
            {
                printf("%lld", n);
            }
            printf("\n");
        }
    }
}


void fact_thread(int n, int weight) {
    struct timespec start, finish;
    double elapsed;
    clock_gettime(CLOCK_MONOTONIC, &start);
    factorize_prime(n);
    clock_gettime(CLOCK_MONOTONIC, &finish);
    elapsed = finish.tv_sec - start.tv_sec + (float)(finish.tv_nsec - start.tv_nsec) / 1000000000;
    printf("Total time for 10 factorization: %f, weight: %d\n", elapsed, weight);
    exit(0);
}


int main(int argc, char *argv[]) {
    int child_status;
    int p;
    struct sched_param param;
    param.sched_priority = 100;

    if (argc != 3) {
        printf("Need 2 inputs num_process and n\n");
        return 0;
    }

    int num_processes = atoi(argv[1]);
    int n = atoi(argv[2]);
    pid_t pid[num_processes];

    for (int i = 0; i < num_processes; i++) {
        int weight = ((3 * i) % 19) + 1;

        if ((pid[i] = fork()) == 0) {
            if (sched_setscheduler(getpid(), SCHED_WRR, &param) != 0) {
                printf("Failed to set scheduler. pid: %d, weight: %d\n", getpid(), weight);
                return -1;
            }
            int w;
            if ((syscall(SCHED_SETWEIGHT, getpid(), weight)) != 0) {
				if(weight>20 || weight <1)
					printf("Invalid weight\n");
				else
                    printf("Failed to set weight. %s\n", strerror(errno));
                return -1;
            }
			else{
			    w=syscall(SCHED_GETWEIGHT, 0);
                printf("weight of pid (%d): %d\n", getpid(), w);
			}
            fact_thread(n, weight);
        }
    }

    for (int i = 0; i < num_processes; i++) {
        pid_t wpid = waitpid(pid[i], &child_status, 0);
    }

    return 0;
}
