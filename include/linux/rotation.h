#include <linux/list.h>
#include <linux/wait.h>
struct Node
{
    int rw; // 0: reader 1: writer
    long st, ed;
    pid_t pid;
    // if ed < st:
    //    범위: st~360, 0~ed
    struct list_head nodes;
};
typedef struct Node node_t;
void exit_rotlock(void);