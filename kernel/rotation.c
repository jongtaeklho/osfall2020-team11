#include <linux/kernel.h>
#include <linux/rotation.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/sched.h>

#define DEG_CHECK(st, ed, deg) ((((st) > (ed)) && (((st) <= (deg)) || ((ed) >= (deg)))) || (((st) < (ed)) && (((st) <= (deg)) && ((ed) >= (deg)))))
/*
 * sets the current device rotation in the kernel.
 * syscall number 398 (you may want to check this number!)
 */
/* 0 <= degree < 360 */

static DECLARE_WAIT_QUEUE_HEAD(wq_rot);
static node_t *head_wait = NULL;
static node_t *head_acquired = NULL;

DEFINE_MUTEX(list_mutex);
DEFINE_MUTEX(wq_mutex);

static int deg_curr;

asmlinkage long set_rotation(int degree) //error: -1
{
    if (head_wait == NULL)
    {
        // printk(KERN_ALERT "Initiate head_wait.\n");
        head_wait = kmalloc(sizeof(node_t), GFP_KERNEL);
        if (head_wait == NULL)
        {
            printk(KERN_ALERT "failed to allocate head_wait.\n");
            return -1;
        }
        INIT_LIST_HEAD(&head_wait->nodes);
    }
    if (head_acquired == NULL)
    {
        // printk(KERN_ALERT "Initiate head_acquired.\n");
        head_acquired = kmalloc(sizeof(node_t), GFP_KERNEL);
        if (head_acquired == NULL)
        {
            printk(KERN_ALERT "failed to allocate head_acquired.\n");
            return -1;
        }
        INIT_LIST_HEAD(&head_acquired->nodes);
    }
    printk(KERN_INFO "Set rotation, degree: %d\n", degree);
    deg_curr = degree;
    wake_up_all(&wq_rot);
    return 0;
}
/*
 * Take a read/or write lock using the given rotation range
 * returning 0 on success, -1 on failure.
 * system call numbers 399 and 400
 */

void gosleep(wait_queue_head_t *wq_head)
{
    mutex_lock(&wq_mutex);
    DEFINE_WAIT(wait_queue_entry);
    prepare_to_wait(wq_head, &wait_queue_entry, TASK_INTERRUPTIBLE);
    mutex_unlock(&wq_mutex);
    schedule();
    mutex_lock(&wq_mutex);
    finish_wait(wq_head, &wait_queue_entry);
    mutex_unlock(&wq_mutex);
}

/*
 * Take a read/or write lock using the given rotation range
 * returning 0 on success, -1 on failure.
 * system call numbers 399 and 400
 */
asmlinkage long rotlock_read(int degree, int range)
{
    if (!(degree >= 0 && degree < 360) || !(range > 0 && range < 180))
        return -1;
    node_t *curr, *nxt;
    int can_read;
    long st = (degree - range + 360) % 360;
    long ed = (degree + range) % 360;
    node_t *new_nodes = kmalloc(sizeof(node_t), GFP_KERNEL);
    if (new_nodes == NULL)
    {
        printk(KERN_INFO "kmalloc error\n");
        return -1;
    }
    new_nodes->st = st;
    new_nodes->ed = ed;
    new_nodes->rw = 0;
    new_nodes->pid = current->pid;
    INIT_LIST_HEAD(&new_nodes->nodes);
    list_add_tail(&new_nodes->nodes, &head_wait->nodes);
    mutex_lock(&list_mutex);
    while (1)
    {
        can_read = 1;
        list_for_each_entry_safe(curr, nxt, &head_acquired->nodes, nodes)
        {
            if (DEG_CHECK(curr->st, curr->ed, deg_curr) && curr->rw)
            {
                can_read = 0;
                break;
            }
        }
        if (can_read)
        {
            list_for_each_entry_safe(curr, nxt, &head_wait->nodes, nodes)
            {

                if (DEG_CHECK(curr->st, curr->ed, deg_curr) && curr->rw)
                {
                    can_read = 0;
                    break;
                }
                else if (curr == new_nodes)
                {
                    break;
                }
            }
        }
        if (!DEG_CHECK(st, ed, deg_curr))
        {
            can_read = 0;
        }
        if (can_read)
            break;
        mutex_unlock(&list_mutex);
        gosleep(&wq_rot);
        mutex_lock(&list_mutex);
    }
    list_move_tail(&new_nodes->nodes, &head_acquired->nodes);
    // printk(KERN_INFO "Hold Read Lock\n");
    mutex_unlock(&list_mutex);
    return 0;
}; /* 0 <= degree < 360 , 0 < range < 180 */

asmlinkage long rotlock_write(int degree, int range)
{
    if (!(degree >= 0 && degree < 360) || !(range > 0 && range < 180))
        return -1;
    node_t *curr, *nxt;
    int can_write;
    long st, ed;
    st = (degree - range + 360) % 360;
    ed = (degree + range) % 360;
    node_t *new_nodes = kmalloc(sizeof(node_t), GFP_KERNEL);
    if (new_nodes == NULL)
    {
        printk(KERN_INFO "kmalloc error\n");
        return -1;
    }
    new_nodes->st = st;
    new_nodes->ed = ed;
    new_nodes->rw = 1;
    new_nodes->pid = current->pid;
    INIT_LIST_HEAD(&new_nodes->nodes);
    list_add_tail(&new_nodes->nodes, &head_wait->nodes);
    mutex_lock(&list_mutex);
    while (1)
    {
        can_write = 1;
        list_for_each_entry_safe(curr, nxt, &head_acquired->nodes, nodes)
        {
            if (DEG_CHECK(curr->st, curr->ed, deg_curr))
            {
                can_write = 0;
                break;
            }
        }
        if (can_write)
        {
            list_for_each_entry_safe(curr, nxt, &head_wait->nodes, nodes)
            {
                if (curr == new_nodes)
                {
                    break;
                }
                if (DEG_CHECK(curr->st, curr->ed, deg_curr))
                {
                    can_write = 0;
                    break;
                }
            }
        }
        if (!DEG_CHECK(st, ed, deg_curr))
        {
            can_write = 0;
        }
        if (can_write)
            break;
        mutex_unlock(&list_mutex);
        gosleep(&wq_rot);
        mutex_lock(&list_mutex);
    }
    list_move_tail(&new_nodes->nodes, &head_acquired->nodes);

    // printk(KERN_INFO "Hold Write Lock\n");
    mutex_unlock(&list_mutex);
    return 0;

}; /* degree - range <= LOCK RANGE <= degree + range */

/*
 * Release a read/or write lock using the given rotation range
 * returning 0 on success, -1 on failure.
 * system call numbers 401 and 402
 */
asmlinkage long rotunlock_read(int degree, int range)
{
    node_t *curr, *next;
    long st, ed;
    if (degree < 0 || degree >= 360)
    {
        printk(KERN_INFO "Wrong argument: degree\n");
        return -1;
    }
    if (range <= 0 || range >= 180)
    {
        printk(KERN_INFO "Wrong argument: range\n");
        return -1;
    }
    st = (degree - range + 360) % 360;
    ed = (degree + range) % 360;
    mutex_lock(&list_mutex);

    // unlock from here

    list_for_each_entry_safe(curr, next, &head_acquired->nodes, nodes)
    {
        if ((curr->st == st) && (curr->ed == ed) && !curr->rw)
        {
            list_del(&curr->nodes);
            kfree(curr);
            mutex_unlock(&list_mutex);
            // printk(KERN_INFO "Release Read Lock\n");
            return 0;
        }
    }
    // printk(KERN_INFO "Release Read Lock\n");
    mutex_unlock(&list_mutex);
    return -1;
}

/* 0 <= degree < 360 , 0 < range < 180 */
asmlinkage long rotunlock_write(int degree, int range)
{
    node_t *curr, *next;
    long st, ed;
    if (degree < 0 || degree >= 360)
    {
        printk(KERN_INFO "Wrong argument: degree\n");
        return -1;
    }
    if (range <= 0 || range >= 180)
    {
        printk(KERN_INFO "Wrong argument: range\n");
        return -1;
    }

    st = (degree - range + 360) % 360;
    ed = (degree + range) % 360;
    mutex_lock(&list_mutex);

    // unlock from here

    list_for_each_entry_safe(curr, next, &head_acquired->nodes, nodes)
    {
        if ((curr->st == st) && (curr->ed == ed) && curr->rw)
        {
            list_del(&curr->nodes);
            kfree(curr);
            // printk(KERN_INFO "Release Write Lock\n");
            mutex_unlock(&list_mutex);
            return 0;
        }
    }
    // printk(KERN_INFO "Release Write Lock\n");
    mutex_unlock(&list_mutex);
    return -1;
}
/* degree - range <= LOCK RANGE <= degree + range */

void exit_rotlock()
{
    node_t *curr, *next;
    int is_write, degree, range;
    long st, ed;
    // Unlock if acquired
    if (head_acquired != NULL)
    {
        list_for_each_entry_safe(curr, next, &head_acquired->nodes, nodes)
        {
            if (curr->pid == current->pid)
            {
                is_write = curr->rw;
                st = curr->st;
                ed = curr->ed;
                degree = (ed + st + 360 * (st > ed)) / 2;
                range = (ed - st + 360 * (st > ed)) / 2;
                // printk(KERN_INFO "exit rotlock\n");
                if (is_write)
                {
                    rotunlock_write(degree, range);
                }
                else
                {
                    rotunlock_read(degree, range);
                }
                return;
            }
        }
    }
    // Remove if waiting
    if (head_wait != NULL)
    {
        list_for_each_entry_safe(curr, next, &head_wait->nodes, nodes)
        {
            if (curr->pid == current->pid)
            {
                wait_queue_entry_t *curr_entry, *next_entry;
                is_write = curr->rw;
                st = curr->st;
                ed = curr->ed;
                degree = (ed + st + 360 * (st > ed)) / 2;
                range = (ed - st + 360 * (st > ed)) / 2;
                // printk(KERN_INFO "exit rotlock\n");
                list_for_each_entry_safe(curr_entry, next_entry, &wq_rot.head, entry)
                {
                    struct task_struct *task = curr_entry->private;
                    if (task->pid == current->pid)
                    {
                        remove_wait_queue(&wq_rot, curr_entry);
                        list_del(&curr->nodes);
                        if (curr->rw)
                        {
                            wake_up_all(&wq_rot); // wake if precedecing write is gone
                        }
                        kfree(curr);
                    }
                }
                return;
            }
        }
    }
}