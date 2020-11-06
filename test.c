#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

int main(){
    clock_t start, end;
    start = clock();
    int n_origin;
    int n;
    int primes_size;
    int i;
    int *primes;
    primes_size = 0;
    n_origin = 5100;
    n = n_origin;
    while(!(n % 2)){
        n /= 2;
        primes_size++;
    }
    primes = (int *) calloc(primes_size, sizeof(int));
    for(i = 0; i < primes_size; i++){
        primes[i] = 2;
    }
    i = 3;
    while(i <= floor(sqrt(n))){
        if(!(n % i)){
            n /= i;
            primes_size++;
            primes = (int *) realloc(primes, primes_size * sizeof(int));
            primes[primes_size - 1] = i;
        } else{
            i += 2;
        }

    }
    if(n != 1){
        primes_size++;
        primes = (int *) realloc(primes, primes_size * sizeof(int));
        primes[primes_size - 1] = n;
    }
    printf("Prime factorization of %d: ", n_origin);
    int product = 1;
    for(i = 0; i < primes_size; i++){
        printf("%d ", primes[i]);
        product *= primes[i];
        if(i + 1 != primes_size){
            printf("x ");
        }
    }
    printf("\nProduct from prime factorization: %d\n", product);
    end = clock();

    printf("Elapsed time: %f\n", (float)(end - start)/CLOCKS_PER_SEC);




    return 0;
}
