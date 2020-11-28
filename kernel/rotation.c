#include <linux/rotation.h>
#include <linux/list.h>
/*
 * sets the current device rotation in the kernel.
 * syscall number 398 (you may want to check this number!)
 */
/* 0 <= degree < 360 */
struct range
{
    long st, ed;
};
struct entry_list
{
    // wait_queue_entry_t* wq_entry;
    int pc; //counter
    struct list_head next_entry;
};

typedef struct range range_t;
typedef struct entry_list entry_list_t;
struct node
{
    int rw; // 0: reader 1: writer
    wait_queue_head_t *wq_head;
    entry_list_t *entry_list;
    range_t *range;
    // if ed < st:
    //    범위: st~360, 0~ed
    struct list_head nodes;
};
static int wait_cnt;
static node_t *head_wait = NULL;
static node_t *head_acquired = NULL;
static node_t dummy1, dummy2;

void init_head_wait() //global waitqueue
{
    // wait_queue_entry_t* wq_entry = DEFINE_WAIT(wq_entry);
    // prepare_to_wait(wq_head, wq_entry, TASK_INTERRUPTABLE);
    head_wait = &dummy1;
    INIT_LIST_HEAD(&head_wait->nodes);
    head_wait->wq_head = DECLARE_WAIT_QUEUE_HEAD(head_wait->wq_head);
}

void init_head_acquired() //global waitqueue
{
    // wait_queue_entry_t* wq_entry = DEFINE_WAIT(wq_entry);
    // prepare_to_wait(wq_head, wq_entry, TASK_INTERRUPTABLE);
    head_acquired = &dummy2;
    INIT_LIST_HEAD(&head_acquired->nodes);
    head_acquired->wq_head = DECLARE_WAIT_QUEUE_HEAD(head_acquired->wq_head);
}

asmlinkage long set_rotation(int degree) //error: -1
{
    node_t *curr, next;
    long cnt;
    node_t *readers;
    node_t node_sched;

    cnt = 0;
    if (degree >= 360 || degree < 0)
    {
        printk(KERN_INFO "Wrong input: degree\n");
        return -1;
    }
    if (head_wait == NULL)
    {
        init_wait();
    }
    // If given range is locked,
    list_for_each_entry_safe(curr, next, &head_acquired.nodes, nodes) long st, ed;
    st = curr->range.st;
    ed = curr->range.ed;
    if (st > ed)
    { // st~360, 0~ed
        if (degree <= ed || degree >= st)
        {
            if (node->rw)
            {
                return 0;
            }
        }
    }
    else
    { // st~ed
        if (degree >= st && degree <= ed)
        {
            if (node->rw)
            {
                return 0;
            }
        }
    }

    readers = kmalloc(sizeof(node_t *), GFP_KERNEL);
    INIT_LIST_HEAD(&readers->nodes);
    list_for_each_entry_safe(curr, next, &head_wait.nodes, nodes)
    {
        long st, ed;
        entry_t *entry_list = curr->entry_list;
        st = curr->range.st;
        ed = curr->range.ed;
        // reader1 ~ writer1 ~ reader2 -> reader1, writer1 execute 
        if (st > ed)
        { // st~360, 0~ed
            if (degree <= ed || degree >= st)
            {
                if (curr->rw)
                { // write: only one entry move
                    node_sched = {
                        .rw = curr.rw,
                        .wq_head = curr->wq_head,
                        .range = curr.range};
                    INIT_LIST_HEAD(&node_sched->nodes);
                    // move to head_acquired
                    list_add(&node_sched->nodes, &head_acquired.nodes);
                    list_move(&entry_list->next_entry, &node_sched.entry_list->next_entry);
                    if (list_empty(curr->entry_list->next_entry))
                    { // remove empty node from head_wait
                        list_del(&curr->nodes);
                    }
                    list_splice_tail(&readers->nodes, &head_wait.nodes); // join readers to head_wait
                    // wake up curr
                    wake_up(curr->wq_head);
                    wait_cnt--;
                    return 1;
                }
                else
                { // read: entire node move
                    list_move_tail(&curr->nodes, &readers->nodes);
                }
            }
        }
        else
        { // st~ed
            if (degree >= st && degree <= ed)
            {
                if (curr->rw)
                {
                    // wake up curr
                    node_sched = {
                        .rw = curr.rw,
                        .wq_head = curr->wq_head,
                        .range = curr.range};
                    INIT_LIST_HEAD(&node_sched->nodes);
                    // move to head_acquired
                    list_add(&node_sched->nodes, &head_acquired.nodes);
                    list_move(&entry_list->next_entry, &node_sched.entry_list->next_entry);
                    if (list_empty(curr->entry_list->next_entry))
                    { // remove empty node from head_wait
                        list_del(&curr->nodes);
                    }
                    list_splice_tail(&readers->nodes, &head_wait.nodes); // join readers to head_wait
                    // wake up curr
                    wake_up(curr->wq_head);
                    wait_cnt--;
                    return 1;
                }
                else
                {
                    list_move_tail(&curr->nodes, &readers->nodes);
                }
            }
        }
    }
    // wake_up_all(&readers->); --> list_for_each_entry use
    list_for_each_entry_safe(curr, next, &readers.nodes, nodes)
    {
        list_move(&curr->nodes, &head_acquired.nodes);
        cnt++;
    }

    wait_cnt -= cnt;
    return cnt;
}
/*
 * Take a read/or write lock using the given rotation range
 * returning 0 on success, -1 on failure.
 * system call numbers 399 and 400
 */
asmlinkage long rotlock_read(int degree, int range)
{
}
/* 0 <= degree < 360 , 0 < range < 180 */
asmlinkage long rotlock_write(int degree, int range)
{

} /* degree - range <= LOCK RANGE <= degree + range */

/*
 * Release a read/or write lock using the given rotation range
 * returning 0 on success, -1 on failure.
 * system call numbers 401 and 402
 */
asmlinkage long rotunlock_read(int degree, int range)
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
    list_for_each_entry_safe(curr, next, &head_acquired.nodes, nodes)
    {
        if ((curr->range.st == st) && (curr->range.ed == ed))
        {
            wake_up(&curr->wq_head);
            wait_cnt--;
            list_del(&curr->nodes);
            break;
        }
    }
    return 0;
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
    list_for_each_entry_safe(curr, next, &head_acquired.nodes, nodes)
    {
        if ((curr->range.st == st) && (curr->range.ed == ed))
        {
            wake_up(&curr->wq_head);
            wait_cnt--;
            list_del(&curr->nodes);
            break;
        }
    }
    return 0;
}
/* degree - range <= LOCK RANGE <= degree + range */

int exit_rotlock()
{
    node_t *curr_hd, next_hd;
    node_t *curr_entry, next_entry;
    list_for_each_entry_safe(curr_hd, next_hd, &head_acquired.nodes, nodes)
    {
        wait_queue_head_t *wq_head = curr->wq_head;
        list_for_each_entry_safe(curr_entry, next_entry, &wq_head->head, entry){
            if()
        }
        remove_wait_queue()
    }
    return 0;
}