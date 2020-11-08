#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>

#define SCHED_WRR 7
#define SCHED_SETWEIGHT 398
#define SCHED_GETWEIGHT 399

void prime_factorization(long long n_origin)
{
    long long n;
    int primes_size;
    int i;
    int *primes;
    primes_size = 0;
    n = n_origin;
    while (!(n % 2))
    {
        n /= 2;
        primes_size++;
    }
    primes = (int *)calloc(primes_size, sizeof(int));
    for (i = 0; i < primes_size; i++)
    {
        primes[i] = 2;
    }
    i = 3;
    while (i * i <= n)
    {
        if (!(n % i))
        {
            n /= (long long)i;
            primes_size++;
            primes = (int *)realloc(primes, primes_size * sizeof(int));
            primes[primes_size - 1] = i;
        }
        else
        {
            i += 2;
        }
    }
    if (n != 1)
    {
        primes_size++;
        primes = (int *)realloc(primes, primes_size * sizeof(int));
        primes[primes_size - 1] = n;
    }
    printf("Prime factorization of %lld: ", n_origin);
    long long product = 1;
    for (i = 0; i < primes_size; i++)
    {
        printf("%d ", primes[i]);
        product *= (long long)primes[i];
        if (i + 1 != primes_size)
        {
            printf("x ");
        }
    }
    printf("\nProduct from prime factorization: %lld\n", product);
    return;
}

int main(int argc, char *argv[])
{
    long long num;
    int num2factor;
    struct timespec start, end;
    float elapsed_time;
    if (argc != 3) {
        printf("Need 2 inputs (num_process, num2factor)\n");
        return 0;
    }
    int num_process = atoi(argv[1]);
    num2factor = atoi(argv[2]);

    pid_t pids[num_process];
    int weights[num_process];
    int status;
    int i;
    struct sched_param param;
    param.sched_priority = 99;
    int sched_weight;
    for (i = 0; i < num_process; i++)
    {
        weights[i] = (3 * i + 1) % 20;
        pids[i] = fork();
        if (pids[i] < 0)
        {
            printf("Fork error\n");
            return -1;
        }
        else if (pids[i] == 0)
        {
            pid_t pid = getpid();
            if (sched_setscheduler(pid, SCHED_WRR, &param))
            {
                printf("Failed to set scheduler, pid: %d\n", pid);
                return -1;
            }
            if (syscall(SCHED_SETWEIGHT, pid, weights[i]) < 0)
            {
                printf("Failed to set weight for scheduler, pid: %d\n", pid);
                return -1;
            }
            else
            {
                sched_weight = syscall(SCHED_GETWEIGHT, pid);
                printf("weight of scheduled process: %d\n", sched_weight);
                clock_gettime(CLOCK_MONOTONIC, &start);
                prime_factorization(num2factor);
                clock_gettime(CLOCK_MONOTONIC, &end);
                elapsed_time = end.tv_sec - start.tv_sec + (float)(end.tv_nsec - start.tv_nsec) / 1000000000;
                printf("Elapsed time: %f, pid: %d, weight: %d\n", elapsed_time, pid, weights[i]);
                exit(0);
            }
        }
    }
    for (i = 0; i < num_process; i++){
        pid_t wait_pid = waitpid(pids[i], &status, 0);
        if(!WIFEXITED(status)){
            printf("Error while process (%d).\n", wait_pid);
        } else{
            printf("Process %d terminated. Status: %d\n", wait_pid, WEXITSTATUS(status));
        }
    }
    return 0;
}