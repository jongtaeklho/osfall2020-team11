#include "sched.h"

#define WRR_TIMESLICE HZ / 100
#define NO_CPU 1000

static DEFINE_PER_CPU(cpumask_var_t, local_cpu_mask);

void __init init_sched_wrr_class(void)
{
    printk(KERN_INFO "init_sched_wrr_class\n");
    unsigned int i;

    for_each_possible_cpu(i)
    {
        zalloc_cpumask_var_node(&per_cpu(local_cpu_mask, i),
                                GFP_KERNEL, cpu_to_node(i));
    }
    current->wrr.weight = 10;
}

void init_wrr_rq(struct wrr_rq *wrr_rq)
{
    printk(KERN_INFO "init_wrr_rq\n");
    wrr_rq->sum = 0;
   
    wrr_rq->head = &wrr_rq->dummy1;
    wrr_rq->tail = &wrr_rq->dummy2;
    wrr_rq->head->nxt = wrr_rq->tail;
    wrr_rq->tail->pre = wrr_rq->head;
}

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
    printk(KERN_INFO "enqueue_task_wrr\n");
    
    struct sched_wrr_entity *wrr_se = &p->wrr;

    struct sched_wrr_entity *head, *tail;
    wrr_se->weight = 10;
    wrr_se->time_slice = wrr_se->weight * WRR_TIMESLICE;

    rcu_read_lock();
    head = rq->wrr.head;
    tail = rq->wrr.tail;

    tail->pre->nxt = wrr_se;
    wrr_se->pre = tail->pre;

    wrr_se->nxt = tail;
    tail->pre = wrr_se;

    rq->wrr.sum += wrr_se->weight;

    rcu_read_unlock();

    wrr_se->parent = &rq->wrr;
    wrr_se->parent_t = p;
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
    printk(KERN_INFO "dequeue_task_wrr\n");
    
    struct sched_wrr_entity *wrr_se = &p->wrr;
    rcu_read_lock();

    wrr_se->pre->nxt = wrr_se->nxt;
    wrr_se->nxt->pre = wrr_se->pre;

    rq->wrr.sum -= wrr_se->weight;

    rcu_read_unlock();
}

static struct task_struct *pick_next_task_wrr(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
    printk(KERN_INFO "pick_next_task_wrr\n");
    
    struct sched_wrr_entity *ptr = rq->wrr.head;
    int i;
    

    for (i = 0; i < 2; i++)
    {
        ptr = ptr->nxt;
        if (ptr == rq->wrr.tail)
            {
                return NULL;
            }
    }
    // rq->curr = ptr->parent_t;
    ptr->parent_t->se.exec_start=rq->clock_task;
    return ptr->parent_t;
}

static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued)
{
    // printk(KERN_INFO "task_tick_wrr\n");
    
    struct sched_wrr_entity *wrr_se = &p->wrr;

    if (p->policy != SCHED_WRR)
        return;
    
    if (--p->wrr.time_slice)
        return;
    rcu_read_lock();
    p->wrr.time_slice = p->wrr.weight * WRR_TIMESLICE;

    rq->wrr.head->nxt = wrr_se->nxt;
    wrr_se->nxt->pre = rq->wrr.head;

    rq->wrr.tail->pre->nxt = wrr_se;
    wrr_se->pre = rq->wrr.tail->pre;

    wrr_se->nxt = rq->wrr.tail;
    rq->wrr.tail->pre = wrr_se;
    rcu_read_unlock();
}

static int can_migrate(struct rq *rq, struct task_struct *p, int dst_cpu)
{
    printk(KERN_INFO "can_migrate\n");
    
    if (task_current(rq, p) && p->nr_cpus_allowed > 1 && cpumask_test_cpu(dst_cpu, &(p)->cpus_allowed))
        return 1;

    return 0;
}

static int migrate_task_wrr(int src_cpu, int dst_cpu)
{
    printk(KERN_INFO "migrate_task_wrr\n");
    
    struct sched_wrr_entity *wrr_se;
    struct rq *rq_dst;
    struct rq *rq_src;
    struct task_struct *curr;
    struct task_struct *migrate_task = NULL;
    int src_weight, dst_weight;
    int max_;
    rcu_read_lock();
    rq_src = cpu_rq(src_cpu);
    rq_dst = cpu_rq(dst_cpu);
    max_ = 0;
    src_weight = rq_src->wrr.sum;
    dst_weight = rq_dst->wrr.sum;
    struct sched_wrr_entity *tmp = rq_src->wrr.head->nxt;
        while (tmp != rq_src->wrr.tail)
        {
            curr = tmp->parent_t;
            if (can_migrate(rq_src, curr, dst_cpu) && tmp->weight > max_ && dst_weight + tmp->weight <= src_weight - tmp->weight)
            {
                max_ = tmp->weight;
                migrate_task = curr;
            }

            tmp = tmp->nxt;
        }
        rcu_read_unlock();

        if (migrate_task != NULL)
        {
            raw_spin_lock(&migrate_task->pi_lock);
            double_rq_lock(rq_src, rq_dst);
            if (task_cpu(migrate_task) != src_cpu)
            {
                double_rq_unlock(rq_src, rq_dst);
                raw_spin_unlock(&migrate_task->pi_lock);
                return 1;
            }
            if (!cpumask_test_cpu(dst_cpu, &(migrate_task->cpus_allowed)))
            {
                printk(KERN_ALERT "failed");
                double_rq_unlock(rq_src, rq_dst);
                raw_spin_unlock(&migrate_task->pi_lock);
                return 0;
            }
            if (migrate_task->on_rq)
            {
                deactivate_task(rq_src, migrate_task, 1);
                set_task_cpu(migrate_task, rq_dst->cpu);
                activate_task(rq_dst, migrate_task, 0);
                check_preempt_curr(rq_dst, migrate_task, 0);
            }
            double_rq_unlock(rq_src, rq_dst);
            raw_spin_unlock(&migrate_task->pi_lock);
            return 1;
        }
        printk(KERN_ALERT "no migrateble task");
        return 0;
    
}
void wrr_load_balance(void)
{
    printk(KERN_INFO "wrr_load_balance\n");
    
    int src_cpu, dst_cpu;
    struct rq *rq;
    int max_ = 0;
    int min_ = 100000000;
    int init_cpu = smp_processor_id();
    rq = cpu_rq(init_cpu);
    src_cpu = rq->cpu;
    dst_cpu = rq->cpu;

    if (unlikely(!cpu_active_mask) && num_active_cpus() < 1)
    {
        printk(KERN_ALERT "NO ACTIVE CPUS");
        return;
    }
    rcu_read_lock();
    int cpu;
    for_each_online_cpu(cpu)
    {
        rq = cpu_rq(cpu);
        if (cpu == NO_CPU)
            continue;
        if (min_ > rq->wrr.sum)
        {
            min_ = rq->wrr.sum;
            dst_cpu = rq->cpu;
        }
        if (max_ < rq->wrr.sum)
        {
            max_ = rq->wrr.sum;
            src_cpu = rq->cpu;
        }
    }
    if (min_ == 100000000 || max_ == 0)
    {
        printk(KERN_ALERT "NO");
        return;
    }
    rcu_read_unlock();
    if (!migrate_task_wrr(src_cpu, dst_cpu))
    {
        printk(KERN_ALERT "NONO...\n");
    }
    return;
}
static void yield_task_wrr(struct rq *rq)
{
    // requeue_task_wrr(rq, rq->curr, 0);
    printk(KERN_INFO "yield_task\n");
    return;
}

static int
select_task_rq_wrr(struct task_struct *p, int select_cpu, int sd_flag, int flags)
{
    printk(KERN_INFO "select_task_rq_wrr\n");
    
    int selected_cpu = task_cpu(p);    
	if (p->nr_cpus_allowed == 1)
        return selected_cpu;
    if(sd_flag != SD_BALANCE_FORK)
        return selected_cpu;
    int cpu;
    int ans=selected_cpu;
    rcu_read_lock();
    struct rq* rq = cpu_rq(selected_cpu);
    for_each_online_cpu(cpu)
    {
        if (cpu == NO_CPU)
            continue;
        if (rq->wrr.sum > cpu_rq(cpu)->wrr.sum)
            {
                rq = cpu_rq(cpu);
                ans=cpu;
            }
    }
    rcu_read_unlock();
    return ans;
}
static void rq_online_wrr(struct rq *rq)
{
    printk(KERN_INFO "rq_online\n");
}
static void rq_offline_wrr(struct rq *rq)
{
    printk(KERN_INFO "rq_offline\n");
}
static void task_woken_wrr(struct rq *rq, struct task_struct *p)
{
    printk(KERN_INFO "task_woken\n");
}
static void switched_from_wrr(struct rq *rq, struct task_struct *p)
{
    printk(KERN_INFO "switched_from\n");
}
static void set_curr_task_wrr(struct rq *rq)
{
    printk(KERN_INFO "set_curr_task\n");
}
static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	printk(KERN_INFO "check_preempt_curr\n");
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p)
{
    printk(KERN_INFO "put_prev_task\n");
}

static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task)
{
    printk(KERN_INFO "get_rr_interval_wrr\n");
    
    /*
	 * Time slice is 0 for SCHED_FIFO tasks
	 */
    if (task->policy == SCHED_RR)
        return sched_rr_timeslice;
    else
        return 0;
}

static void
prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio)
{
    printk(KERN_INFO "prio_changed\n");
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p)
{
    printk(KERN_INFO "switched_to\n");
    struct sched_wrr_entity *wrr_e = &p->wrr;
    wrr_e->time_slice = wrr_e->weight * WRR_TIMESLICE;
}

static void update_curr_wrr(struct rq *rq)
{
    printk(KERN_INFO "update_curr_rt\n");
}
const struct sched_class wrr_sched_class = {
    .next = &fair_sched_class,
    .enqueue_task = enqueue_task_wrr,
    .dequeue_task = dequeue_task_wrr,
    .yield_task = yield_task_wrr,
    .check_preempt_curr = check_preempt_curr_wrr,

    .pick_next_task = pick_next_task_wrr,
    .put_prev_task = put_prev_task_wrr,

#ifdef CONFIG_SMP
    .select_task_rq = select_task_rq_wrr,

    .set_cpus_allowed = set_cpus_allowed_common,
    .rq_online = rq_online_wrr,
    .rq_offline = rq_offline_wrr,
    .task_woken = task_woken_wrr,
    .switched_from = switched_from_wrr,
#endif

    .set_curr_task = set_curr_task_wrr,
    .task_tick = task_tick_wrr,

    .get_rr_interval = get_rr_interval_wrr,

    .prio_changed = prio_changed_wrr,
    .switched_to = switched_to_wrr,

    .update_curr = update_curr_wrr};
