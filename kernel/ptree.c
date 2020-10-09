#include <linux/slab.h>
#include <linux/prinfo.h>
#include <linux/sched/task.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/cred.h>

void process_tree_traversal(struct prinfo *process_infos, struct task_struct *task, int max_cnt, int *curr_cnt, int* true_cnt)
{
  struct task_struct *child, *first_child, *next_sibling;
  (*true_cnt)++;
  if (*curr_cnt < max_cnt)
  {
    first_child = list_first_entry_or_null(&(task->children), struct task_struct, sibling);
    next_sibling = list_first_entry_or_null(&(task->sibling), struct task_struct, sibling);

    process_infos[*curr_cnt].state = task->state;
    process_infos[*curr_cnt].pid = task->pid;
    process_infos[*curr_cnt].parent_pid = task->parent->pid;
    process_infos[*curr_cnt].first_child_pid = (first_child != NULL) ? first_child->pid : 0;
    process_infos[*curr_cnt].next_sibling_pid = (next_sibling != NULL) ? next_sibling->pid : 0;
    process_infos[*curr_cnt].uid = (task->cred->uid).val;
    strncpy(process_infos[(*curr_cnt)++].comm, task->comm, 64);

    list_for_each_entry(child, &task->children, sibling)
    {
      process_tree_traversal(process_infos, child, max_cnt, curr_cnt, true_cnt);
    }
  }
  return;
}

/* ptree implementation */

static int nr_max;
// buf: buffer of process process_info
// nr: # of struct prinfo entries
asmlinkage int sys_ptree(struct prinfo *buf, int *nr)
{
  struct prinfo *process_infos;
  int proc_cnt;
  int true_cnt;
  printk("ptree start");
  // -EINVAL: if buf or nr are null, or if the number of entries is less than 1
  // -EFAULT: if buf or nr are outside the accessible address space.
  if ((buf == NULL) || (nr == NULL))
    return -EINVAL;
  if (access_ok(VERIFY_READ, nr, sizeof(int)) == 0)
    return -EFAULT;

  if (copy_from_user(&nr_max, nr, sizeof(int) != 0))
  { // don't use user memory space in kernel directly
    printk("Could not copy nr from user.\n");
  }

  if (nr_max < 1)
    return -EINVAL;
  if (access_ok(VERIFY_READ, buf, nr_max * sizeof(struct prinfo)) == 0)
    return -EFAULT;

  process_infos = kmalloc(nr_max * sizeof(struct prinfo), GFP_KERNEL);
  proc_cnt = 0;
  true_cnt = 0;

  read_lock(&tasklist_lock);
  process_tree_traversal(process_infos, &init_task, nr_max, &proc_cnt, &true_cnt);
  read_unlock(&tasklist_lock);

  if (copy_to_user(buf, process_infos, proc_cnt * sizeof(struct prinfo)) != 0)
  {
    printk("Could not copy buf to user.\n");
  }
  if (nr_max > proc_cnt)
  {
    if (copy_to_user(nr, &(proc_cnt), sizeof(int)) != 0)
    {
      printk("Could not copy nr to user.\n");
    }
  }
  kfree(process_infos);
  printk("ptree done\n");
  return true_cnt;
}

