#include <linux/list.h>
struct node
{
    int rw; // 0: reader 1: writer
    long st, ed;
    pid_t pid;
    wait_queue_head_t *wq_head;
    // if ed < st:
    //    범위: st~360, 0~ed
    struct list_head nodes;
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
void exit_rotlock();