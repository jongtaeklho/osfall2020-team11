/*
 * rootfs.img is in tizen-image
 * mount: sudo mount rootfs.img ~/emulator/mnt_dir, sudo umount ~/emulator/mnt_dir
 *  
 */

/* put this file as kernel/ptree.c
*/

#include <linux/prinfo.h>
#include <linux/sched/task.h>
#include <linux/sched.h>
#include <linux/list.h>

#define __USE_GNU

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
  INIT_LIST_HEAD(getpid(prinfo->pid));
  
  read_lock(&tasklist_lock);
  /* do the job... */
  read_unlock(&tasklist_lock);
  return 0;
}


