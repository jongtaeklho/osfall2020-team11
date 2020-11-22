#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

struct prinfo
{
    int64_t state;          /* current state of process */
    pid_t pid;              /* process id */
    pid_t parent_pid;       /* process id of parent */
    pid_t first_child_pid;  /* pid of oldest child */
    pid_t next_sibling_pid; /* pid of younger sibling */
    int64_t uid;            /* user id of process owner */
    char comm[64];          /* name of program executed */
};

int main()
{

    struct prinfo *buf;
    int entire_proc;
    int nr = 100;
    int i = 0;
    int indent = 0;
    int curr_branch = 0;
    int j;
    int *level = (int *)calloc(nr, sizeof(int));
    for (i = 0; i < nr; i++)
    { // initialize tree level
        level[i] = 0;
    }
    buf = calloc(nr, sizeof(struct prinfo));
    printf("Value of nr before ptree: %d\n", nr);

    entire_proc = syscall(398, buf, &nr);
    printf("Return value of ptree: %d\n", entire_proc);
    printf("Value of nr after ptree: %d\n", nr);

    //error handling
    if (entire_proc < 0)
    {
        if (buf == NULL || &nr == NULL || nr < 1)
        {
            errno = EINVAL;
            perror("Ptree error : buf/nr is NULL or nr is less than 1");
            printf("Address of buf: %p\n", buf);
            printf("Address of nr: %p\n", &nr);
        }
        else
        {
            errno = EFAULT;
            perror("Ptree error : buf/nr is outside of the accessible address space");
            printf("Address of buf: %p\n", buf);
            printf("Address of nr: %p\n", &nr);
        }
        return 1;
    }
    
    for (i = 0; i < nr; i++)
    {
        // print process tree
        for (j = 0; j < indent; j++)
            printf("\t");
        printf("%s,%d,%lld,%d,%d,%d,%lld\n", buf[i].comm, buf[i].pid, buf[i].state, buf[i].parent_pid, buf[i].first_child_pid, buf[i].next_sibling_pid, buf[i].uid);

        if (buf[i].next_sibling_pid != 0)
        { // another branch exists
            level[curr_branch++] = indent;
        }
        // change indentation based on depth
        // if child exists, add indentation
        // if child doesn't exist, use indentation of former branch
        indent = (buf[i].first_child_pid != 0) ? indent + 1 : level[--curr_branch];
    }
    free(buf);

    return 0;
}