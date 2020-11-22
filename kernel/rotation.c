#include <linux/rotation.h>
#include <linux/list.h>
/*
 * sets the current device rotation in the kernel.
 * syscall number 398 (you may want to check this number!)
 */
/* 0 <= degree < 360 */

static node_t* head_wait = NULL;
static node_t* head_acquired = NULL;

int init_wait()
{
    wait_queue_head_t *wq_head = DECLARE_WAIT_QUEUE_HEAD(wq_head);
    // wait_queue_entry_t* wq_entry = DEFINE_WAIT(wq_entry);
    // prepare_to_wait(wq_head, wq_entry, TASK_INTERRUPTABLE);
}

asmlinkage long set_rotation(int degree)    //error: -1
{
    node_t *curr, next;
    int cnt;
    node_t* readers;

    cnt = 0;
    if (head_wait == NULL)
    {
        init_wait();
    }
    
    list_for_each_entry_safe(curr, next, &head_acquired.nodes, nodes)
        long st, ed;
        st = curr->range.st;
        ed = curr->range.ed;
        if(st > ed){  // st~360, 0~ed
            if(degree <= ed || degree >= st){
                if(curr->rw){
                    return 0;
                }
            } 
        } else{ // st~ed
            if(degree >= st && degree <= ed){
                if(curr->rw){
                    return 0;
                }
            } 
        }
    }
    readers = kmalloc(sizeof(node_t), GFP_KERNEL);
    INIT_LIST_HEAD(&readers->nodes);
    list_for_each_entry_safe(curr, next, &head_wait.nodes, nodes)
    {
        long st, ed;
        st = curr->range.st;
        ed = curr->range.ed;
        if(st > ed){  // st~360, 0~ed
            if(degree <= ed || degree >= st){
                if(curr->rw){
                    // wake up curr
                    list_del(&curr->nodes);
                    list_add(&curr->nodes, &head_acquired.nodes);
                    curr->pc--;
                    return 1;
                } else{
                    list_add_tail(&curr->nodes, &readers->nodes);
                }
            } 
        } else{ // st~ed
            if(degree >= st && degree <= ed){
                if(curr->rw){
                    // wake up curr
                    list_del(&curr->nodes);
                    list_add(&curr->nodes, &head_acquired.nodes);
                    curr->pc--;
                    return 1;
                } else{
                    list_add_tail(&curr->nodes, &readers->nodes);
                }
            }
        }
    }

    list_for_each_entry_safe(curr, next, &head_acquired.nodes, nodes)
        long st, ed;
        st = curr->range.st;
        ed = curr->range.ed;
        if(st > ed){  // st~360, 0~ed
            if(degree <= ed || degree >= st){
                list_del(&curr->nodes);
                list_add(&curr->nodes, &head_acquired.nodes);
                cnt++;
            } 
        } else{ // st~ed
            if(degree >= st && degree <= ed){
                list_del(&curr->nodes);
                list_add(&curr->nodes, &head_acquired.nodes);
                cnt++;
            } 
        }
    }
    return cnt;
}

/*
 * Take a read/or write lock using the given rotation range
 * returning 0 on success, -1 on failure.
 * system call numbers 399 and 400
 */
asmlinkage long rotlock_read(int degree, int range){

};  /* 0 <= degree < 360 , 0 < range < 180 */
asmlinkage long rotlock_write(int degree, int range); /* degree - range <= LOCK RANGE <= degree + range */

/*
 * Release a read/or write lock using the given rotation range
 * returning 0 on success, -1 on failure.
 * system call numbers 401 and 402
 */
asmlinkage long rotunlock_read(int degree, int range);  /* 0 <= degree < 360 , 0 < range < 180 */
asmlinkage long rotunlock_write(int degree, int range); /* degree - range <= LOCK RANGE <= degree + range */