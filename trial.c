#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>

#define SCHED_WRR 7
#define NUM_PROCESS 10
#define SCHED_SETWEIGHT 398
#define SCHED_GETWEIGHT 399

void prime_factorization(long long  n_origin)
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

int main()
{
    pid_t pids[NUM_PROCESS];
    int weights[NUM_PROCESS];
    long long num;

    clock_t start, end;
    int status;
    int i;
    struct sched_param param;
    param.sched_priority = 1;
    for (i = 0; i < NUM_PROCESS; i++)
    {
        weights[i] = 2 * i + 1;
        num = (long long)2 * 2 * 3 * 3 * 3 * 3 * 5 * 7 * 9 * 11 * 13 * 13 * 17 * 17 * 23 * 59; 
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
            if (syscall(SCHED_SETWEIGHT, pid, weights[i]))
            {
                printf("Failed to set weight for scheduler, pid: %d\n", pid);
                return -1;
            }
            else
            {
                start = clock();
                prime_factorization(num);
                end = clock();

                printf("Elapsed time: %f, pid: %d, weight: %d\n", (float)(end - start) / CLOCKS_PER_SEC, getpid(), weights[i]);
            }
        }
    }
    for (i = 0; i < NUM_PROCESS; i++){
        pid_t wait_pid = waitpid(pids[i], &status, 0);
        if(WIFSIGNALED(status) > 0){
            printf("Error while process (%d).\n", wait_pid);
        } else{
            printf("Process %d terminated.\n", wait_pid);
        }
    }
    return 0;
}