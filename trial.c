#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#define _CRT_SECURE_NO_WARNINGS
#define ROTLOCK_READ 399
#define ROTUNLOCK_READ 401

void factorize_prime(int n_origin)
{
    int n;
    int i;
    if (n_origin <= 1)
    {
        printf("%d cannot be factorized.\n", n_origin);
        // fprintf(fp, "%d cannot be factorized.\n", n_origin);
        return;
    }
    n = n_origin;

    printf("%d = ", n_origin);
    // fprintf(fp, "%d = ", n_origin);
    while (!(n % 2))
    {
        n /= 2;
        printf("2 * ");
        // fprintf(fp, "2 * ");
    }
    i = 3;
    while (i * i <= n)
    {
        if (!(n % i))
        {
            n /= (long long)i;
            printf("%d * ", i);
            // fprintf(fp, "%d * ", n_origin);
        }
        else
        {
            i += 2;
        }
    }
    if (n > 1)
    {
        printf("%d", n);
        // fprintf(fp, "%d", n_origin);
    }
    printf("\n");
    // fprintf(fp, "\n");
}

int main(int argc, char *argv[])
{
    char buffer[16];
    FILE *fp;
    if (argc != 2)
    {
        printf("wrong input\n");
        return 0;
    }
    int id = atoi(argv[1]);
    int num;
    while (1)
    {
        if (syscall(ROTLOCK_READ, 90, 90) == -1)
        {
            printf("lock_read is wrong\n");
            return -1;
        }
        fp = fopen("integer", "r");
        fgets(buffer, sizeof(buffer), fp);
        num = atoi(buffer);
        printf("trial-%d: ", id);
        factorize_prime(num);
        fclose(fp);
        if (syscall(ROTUNLOCK_READ, 90, 90) == -1)
        {
            printf("unlock_read is wrong\n");
            return -1;
        }
    }
    return 0;
}