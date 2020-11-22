#include <linux/list.h>
#include <linux/wait.h>
struct range{
    long st, ed;
};
typedef struct range range_t;
struct node{
    int rw;// 0: reader 1: writer
    // int wait_state;
    wait_queue_head* wq_head;
    wait_queue_entry* wq_entry;
    range_t* range;
        // if ed < st:
        //    범위: st~360, 0~ed 
    struct list_head nodes;
    int pc; //counter
};
typedef struct node node_t;
asmlinkage long set_rotation(int degree);

/*
 * Take a read/or write lock using the given rotation range
 * returning 0 on success, -1 on failure.
 * system call numbers 399 and 400
 */
asmlinkage long rotlock_read(int degree, int range);  /* 0 <= degree < 360 , 0 < range < 180 */
asmlinkage long rotlock_write(int degree, int range); /* degree - range <= LOCK RANGE <= degree + range */

/*
 * Release a read/or write lock using the given rotation range
 * returning 0 on success, -1 on failure.
 * system call numbers 401 and 402
 */
asmlinkage long rotunlock_read(int degree, int range);  /* 0 <= degree < 360 , 0 < range < 180 */
asmlinkage long rotunlock_write(int degree, int range); /* degree - range <= LOCK RANGE <= degree + range */