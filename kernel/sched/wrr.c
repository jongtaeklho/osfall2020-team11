#include "sched.h"

#define WRR_TIMESLICE= HZ/100
#define NO_CPU =1




static DEFINE_PER_CPU(cpumask_var_t, local_cpu_mask);


void __init init_sched_wrr_class(void)
{
	unsigned int i;

	for_each_possible_cpu(i) {
		zalloc_cpumask_var_node(&per_cpu(local_cpu_mask, i),
					GFP_KERNEL, cpu_to_node(i));
	}
        curr->wrr.weight=10;
}





static void init_rt_wrr(struct wrr_rq *wrr_rq)
{
        wrr_rq->sum=0;
        struct sched_wrr_entity dummy1,dummy2;
        wrr_rq->head=&dummy1;
        wrr_rq->tail=&dummy2;
        wrr_rq->head->nxt=tail;
        wrr_rq->tail->pre=head;

}



static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
    struct sched_wrr_entity *wrr_se= &p->wrr;

    struct sched_wrr_entity *head,*tail;
   
    wrr_se->timeslice=wrr_se->weight * WRR_TIMESLICE;
    
    int cpu;
    rq = cpu_rq(0);
    rcu_read_lock();
    for_each_online_cpu(cpu){
        if(cpu==NO_CPU) continue;
        if(rq->wrr->sum>cpu_rq(cpu)->wrr->sum)
             rq=cpu_rq(cpu);
    }
    

    head= rq->wrr->head;
    tail= rq->wrr->tail;
    
    tail->pre->nxt=wrr_se;
    wrr_se->pre=tail->pre;

    wrr_se->nxt=tail;
    tail->pre=wrr_se;
 
    rq->wrr->sum += wrr_se->weight;
 
    rcu_read_unlock();
 
    wrr_se->parent = rq->wrr; 
    wrr_se->parent_t=p;

}



static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags){
    struct sched_wrr_entity *wrr_se= &p->wrr;
    rcu_read_lock();
 
    wrr_se->pre->nxt=wrr_se->nxt;
    wrr_se->nxt->pre=wrr_se->pre;
 
    rq->wrr->sum-=wrr_se->weight;
 
    rcu_read_unlock();
}




static struct task_struct *pick_next_task_wrr(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
    struct sched_wrr_entity *ptr=rq->wrr->head;
   
    rcu_read_lock();


    for(int i=0;i<2;i++){
        ptr=ptr->nxt;
        if(ptr==rq->wrr->tail) return NULL;
    }
    rq->curr=ptr->parent_t;
    rcu_read_unlock();
    return ptr->parent_t;
}




static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;
	

	
	if (p->policy != SCHED_WRR)
		return;
        rcu_read_lock();
	if (--p->wrr.time_slice)
		return;

	p->wrr.time_slice = p->wrr.weight * WRR_TIMESLICE;
        
        rq->wrr->head->nxt=wrr_se->nxt;
        wrr_se->nxt->pre=rq->wrr->head;

        rq->wrr->tail->pre->nxt=wrr_se;
        wrr_se->pre=rq->wrr->tail->pre;

        wrr_se->nxt=rq->wrr->tail;
        rq->wrr->tail->pre=wrr_se;
        

}


static int can_migrate(struct rq *rq, struct task_struct *p, int dst_cpu) {
    if(task_curr(rq,p)&&p->nr_cpus_allowed>1&&cpumask_test_cpu(dst_cpu,&(p)->cpus_allowed)) 
    return 1;

return 0;



}

static int migrate_task_wrr(int src_cpu,int dst_cpu{
    struct sched_wrr_entity *wrr_se;
    struct rq *rq_dst;
    struct rq *rq_src;
    struct task_struct *curr;
    struct task_struct *migrate_task=NULL;
    int src_weight,dst_weight;
    int max_;
    rcu_read_lock();
    rq_src=cpu_rq(src_cpu);
    rq_dst=cup_rq(dst_cpu);
    max_=0;
    src_weight=rq_src->wrr.sum;
    dst_weight=rq_dst->wrr.sum;
    struct sched_wrr_entity *tmp=rq_src->wrr->head->nxt;
    while(!tmp!=rq_src->wrr->tail){
    curr=tmp->parent_t;
    if(can_migarte(rq_src,curr,dst_cpu)&&tmp.weight>max_&&dst_weight+tmp.weight<=src_weight-tmp.weight)
    {
        max_=tmp.weight;
        migrate_task=curr;
    }

    tmp=tmp->nxt;
    }
    rcu_read_unlock();

    if(migrate_task!=NULL){
    raw_spin_lock(&migrate_task->pi_lock);
    double_rq_lock(rq_src,rq_dst);
    if(task_cpu(migrate_task)!=src_cpu){
        double_rq_unlock(rq_src,rq_dst);
        raw_spin_unlock(&migrate_task->pi_lock);
    return 1;
    }
    if(!cpumask_test_cpu(dst_cpu,&(migrate_task->cpus_allowed)))    
    {
        printk(KERN_ALERT "failed");
        double_rq_unlock(rq_src,rq_dst);
        raw_spin_unlock(&migrate_task->pi_lock);
        return 0;
    }
    if(migarte_task->on_rq) {
        deactivate_task(rq_src,migrate_task,1);
        set_task_cpu(migrate_task,rq_dst->cpu);
        activate_task(rq_dst,migrate_task,0);
        check_preempt_curr(rq_dst,migrate_task,0);
    }
    double_rq_unlock(rq_src,rq_dst);
    raw_spin_unlock(&migarte_task->pi_lock);
    return 1;

    }
    printk(KERN_LERT "no migrateble task");
    return 0;

}

void wrr_load_balance(void){
    int src_cpu,dst_cpu;
    struct rq *rq;
    int max_=0;
    int min_=100000000;
    int init_cpu=smp_processor_id();
    rq=cpu_rq(init_cup);
    src_cpu=rq->cpu;
    dst_cpu=rq->cpu;

    if(unlikely(!cpu_active_mask)&&num_active_cpus()<1) {
        printk(KERN_ALERT "NO ACTIVE CPUS");
        return;
    }
    rcu_read_lock();
    for_each_online_cpu(cpu){
        rq=cpu_rq(cpu);
        if(cpu==NO_CPU) continue;
        if(min_>rq->wrr.sum) {
            min_=rq->wrr.sum;
            dst_cpu=rq->cpu;
        }
        if(max_<rq->wrr.sum) {
            max_=rq->wrr.sum;
            src_cpu=rq->cpu;
        }
    }
    if(min_==100000000||max_==0) {
        printk(KERN_ALERT "NO");
        return;
    }
    rcu_read_unlock();
    if(!migarte_task_wrr(src_cpu,dst_cpu) {
        printk(KERN_ALERT "NONO...\n");
    }
    return;
}



const struct sched_class wrr_sched_class = {
	.next			= &fair_sched_class,
	.enqueue_task		= enqueue_task_wrr,
	.dequeue_task		= dequeue_task_wrr,
	.yield_task		= yield_task_wrr,

	.pick_next_task		= pick_next_task_wrr,
	.put_prev_task		= put_prev_task_wrr,


#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,

	.set_cpus_allowed       = set_cpus_allowed_common,
	.rq_online              = rq_online_wrr,
	.rq_offline             = rq_offline_wrr,
	.task_woken		= task_woken_wrr,
	.switched_from		= switched_from_wrr,
#endif


	.set_curr_task          = set_curr_task_wrr,
	.task_tick		= task_tick_wrr,

	.update_curr		= update_curr_wrr,
};


