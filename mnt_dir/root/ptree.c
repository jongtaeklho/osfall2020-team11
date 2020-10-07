/*
 * ~/emulator/include/linux/syscalls.h
 * ~/emulator/arch/x86/entry/syscalls/syscall_64.tbl
 *  
 */

/* put this file as kernel/ptree.c
*/

#include <linux/sched/task.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <sys/types.h>
#include <unistd.h>

/*  put prinfo in include/linux/prinfo.h as part of the solution  */
struct prinfo {
  int64_t state;            /* current state of process */
  pid_t   pid;              /* process id */
  pid_t   parent_pid;       /* process id of parent */
  pid_t   first_child_pid;  /* pid of oldest child */
  pid_t   next_sibling_pid; /* pid of younger sibling */
  int64_t uid;              /* user id of process owner */
  char    comm[64];         /* name of program executed */
};

/* ptree implementation */
SYSCALL_DEFINE2(ptree, struct prinfo *, buf, int *, nr){
  INIT_LIST_HEAD(getpid(prinfo->pid))
  
  read_lock(&tasklist_lock);
  /* do the job... */
  read_unlock(&tasklist_lock);
}

SYSCALL_DEFINE0(myprint){
  printk("My print\n");
}
