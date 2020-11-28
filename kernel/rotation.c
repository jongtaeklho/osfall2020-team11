#include <linux/rotation.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/sched.h>

#define DEG_CHECK(st, ed, deg) (((st) > (ed)) && (((st) <= (deg)) || ((ed) >= (deg)))) || (((st) < (ed)) && (((st) <= (deg)) && ((ed) >= (deg))))
/*
 * sets the current device rotation in the kernel.
 * syscall number 398 (you may want to check this number!)
 */
/* 0 <= degree < 360 */
struct node
{
    int rw; // 0: reader 1: writer
    long st, ed;
    pid_t pid;
    // if ed < st:
    //    범위: st~360, 0~ed
    struct list_head nodes;
};
static int read_cnt, write_cnt;
static node_t *head_wait = NULL;
static node_t *head_acquired = NULL;
DECLARE_WAIT_QUEUE_HEAD(wq_head);
DEFINE_MUTEX(node_mutex);

static int deg_curr;

asmlinkage long set_rotation(int degree) //error: -1
{
    printk(KERN_INFO "Set rotation, degree: %d\n", degree);
    deg_curr = degree;
    wake_up_all(wq_head);
    return 0;
}
/*
 * Take a read/or write lock using the given rotation range
 * returning 0 on success, -1 on failure.
 * system call numbers 399 and 400
 */

int isrange(int degree, long st, long ed)
{
    if (st < ed)
    {
        if (degree >= st && degree <= ed)
            return 1;
        else
            return 0;
    }
    else
    {
        if (degree >= st || degree <= ed)
            return 1;
        else
            return 0;
    }
}
void gosleep(node_t *cur)
{
    mutex_lock(&queue_mutex);
    DEFINE_WAIT(wait_queue_entry);
    prepare_to_wait(cur->wq_head, &wait_queue_entry, TASK_INTERRUPTIBE);
    mutex_unlock(&queue_mutex);
    schedule();
    mutex_lock(&queue_mutex);
    finish_wait(cur->wq_head, &wait_queue_entry);
    mutex_unlock(&queue_mutex);
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
    long st = (degree - range + 360) % 360;
    long ed = (degree + range) % 360;
    //들어갈수있으면 보내고
    //재우고
    node_t *new_nodes = kmalloc(sizeof(node_t), GFP_KERNEL);
    new_nodes->st = st;
    new_nodes->ed = ed;
    new_nodes->rw = 0;
    int writer_cnt;
    mutex_lock(&wait_mutex);
    mutex_lock(&acquired_mutex);
    list_add_tail(&new_nodes->nodes, head_wait);
    node_t *curr, nxt;
    int iswriter;
    iswriter = 0;
    while (1)
    {
        iswriter = 0;
        list_for_each_entry_safe(curr, nxt, &head_acquired, nodes)
        {
            if (isrange(deg_curr, curr->st, curr->ed) && curr->rw == 1)
            {
                iswriter = 1;
                break;
            }
        }
        if (iswriter == 0)
        {
            list_for_each_entry_safe(curr, nxt, &head_wait, nodes)
            {
                if (curr == new_nodes)
                {
                    break;
                }
                if (isrange(deg_curr, curr->st, curr->ed) && curr->rw == 1)
                {
                    iswriter = 1;
                    break;
                }
            }
        }
        writer_cnt = atomic_read(&__writer_cnt);
        if (!isrange(deg_curr, st, ed) || writer_cnt)
        {
            iswriter = 1;
        }
        if (iswriter == 0)
            break;
        mutex_unlock(&wait_mutex);
        mutex_unlock(&acquired_mutex);
        gosleep(new_nodes);
        mutex_lock(&wait_mutex);
        mutex_lock(&acquired_mutex);
    }
    //일어났으면 acquried_head 에다가 추가하고
    //wait_head 빼고
    list_move_tail(new_nodes, head_acquired);
    atomic_inc(&__reader_cnt);

    mutex_unlock(&wait_mutex);
    mutex_unlock(&acquired_mutex);
    return 0;
}; /* 0 <= degree < 360 , 0 < range < 180 */
asmlinkage long rotlock_write(int degree, int range)
{

    if (!(degree >= 0 && degree < 360) || !(range > 0 && range < 180))
        return -1;
    long st = (degree - range + 360) % 360;
    long ed = (degree + range) % 360;
    //들어갈수있으면 보내고
    //재우고
    node_t *new_nodes = kmalloc(sizeof(node_t), GFP_KERNEL);
    new_nodes->st = st;
    new_nodes->ed = ed;
    new_nodes->rw = 1;
    mutex_lock(&wait_mutex);
    mutex_lock(&acquired_mutex);
    node_t *curr, nxt;
    int isok = 0;
    int reader_cnt, writer_cnt;
    while (1)
    {
        isok = 0;
        list_for_each_entry_safe(curr, nxt, &head_acquired, nodes)
        {
            if (isrange(deg_curr, curr->st, curr->ed))
            {
                isok = 1;
                break;
            }
        }
        if (isok == 0)
        {
            list_for_each_entry_safe(curr, nxt, &head_wait, nodes)
            {
                if (curr == new_nodes)
                {
                    break;
                }
                isok = 1;
                break;
            }
        }
        reader_cnt = atomic_read(&__reader_cnt);
        writer_cnt = atomic_read(&__writer_cnt);
        if (reader_cnt || writer_cnt || !isrange(deg_curr, st, ed))
        {
            isok = 1;
        }
        if (isok == 0)
            break;
        mutex_unlock(&wait_mutex);
        mutex_unlock(&acquired_mutex);
        gosleep(new_nodes);
        mutex_lock(&wait_mutex);
        mutex_lock(&acquired_mutex);
    }
    list_move_tail(new_nodes, head_acquired);
    atomic_inc(&__writer_cnt);

    mutex_unlock(&wait_mutex);
    mutex_unlock(&acquired_mutex);
    return 0;

}; /* degree - range <= LOCK RANGE <= degree + range */

/*
 * Release a read/or write lock using the given rotation range
 * returning 0 on success, -1 on failure.
 * system call numbers 401 and 402
 */
asmlinkage long rotunlock_read(int degree, int range)
{
    node_t *curr, next;
    long st, ed;
    printk(KERN_INFO "Rotation unlock(read), degree:%d, range:%d\n", degree, range);
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
    st = (degree > range) ? (degree - range) : (degree - range + 360);
    ed = (degree + range < 360) ? (degree + range) : (degree + range - 360);
    mutex_lock(&node_mutex);
    while (!DEG_CHECK(st, ed, deg_curr))
    {
        mutex_unlock(&node_mutex);
        DEFINE_WAIT(wq_entry);
        prepare_to_wait(&wq_head, &wq_entry, TASK_INTERRUPTIBLE);
        schedule();
        finish_wait(&wq_head, &wq_entry);
        mutex_lock(&node_mutex);
    }

    // holding lock from here

    list_for_each_entry_safe(curr, next, &head_acquired.nodes, nodes)
    {
        if ((curr->st == st) && (curr->ed == ed))
        {
            list_del(&curr->nodes);
            read_cnt--;
            kfree(curr);
            mutex_unlock(&node_mutex);
            return 0;
        }
    }
    mutex_unlock(&node_mutex);
    return -1;
}

/* 0 <= degree < 360 , 0 < range < 180 */
asmlinkage long rotunlock_write(int degree, int range)
{
    node_t *curr, next;
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

    st = (degree > range) ? (degree - range) : (degree - range + 360);
    ed = (degree + range < 360) ? (degree + range) : (degree + range - 360);
    mutex_lock(&node_mutex);
    while (!DEG_CHECK(st, ed, deg_curr))
    {
        mutex_unlock(&node_mutex);
        DEFINE_WAIT(wq_entry);
        prepare_to_wait(&wq_head, &wq_entry, TASK_INTERRUPTIBLE);
        schedule();
        finish_wait(&wq_head, &wq_entry);
        mutex_lock(&node_mutex);
    }

    // holding lock from here

    list_for_each_entry_safe(curr, next, &head_acquired.nodes, nodes)
    {
        if ((curr->st == st) && (curr->ed == ed))
        {
            list_del(&curr->nodes);
            write_cnt--;
            kfree(curr);
            mutex_unlock(&node_mutex);
            return 0;
        }
    }
    mutex_unlock(&node_mutex);
    return -1;
}
/* degree - range <= LOCK RANGE <= degree + range */

int exit_rotlock()
{
    node_t *curr, next;
    int is_write, degree, range;
    long st, ed;
    list_for_each_entry_safe(curr, next, &head_acquired.nodes, nodes)
    {
        if (curr->pid == current->pid)
        {
            is_write = curr->rw;
            st = curr->st;
            ed = curr->ed;
            if (st > ed)
            {
                degree = (ed + 360 + st) / 2;
                range = (ed + 360 - st) / 2;
            }
            else
            {
                degree = (ed + st) / 2;
                range = (ed - st) / 2;
            }
            if (is_write)
            {
                rotunlock_write(degree, range);
            }
            else
            {
                rotunlock_read(degree, range);
            }
            break;
        }
    }
    return 0;
}