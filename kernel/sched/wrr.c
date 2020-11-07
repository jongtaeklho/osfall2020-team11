#include "sched.h"

#define WRR_TIMESLICE= HZ/100
#define NO_CPU =1
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
    wrr_se->weight=10;
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







const struct sched_class wrr_sched_class = {
	.next			= &fair_sched_class,
	.enqueue_task		= enqueue_task_wrr,
	.dequeue_task		= dequeue_task_wrr,
	.yield_task		= yield_task_wrr,

	.check_preempt_curr	= check_preempt_curr_wrr,

	.pick_next_task		= pick_next_task_wrr,
	.put_prev_task		= put_prev_task_wrr,


#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_rt,

	.set_cpus_allowed       = set_cpus_allowed_common,
	.rq_online              = rq_online_rt,
	.rq_offline             = rq_offline_rt,
	.task_woken		= task_woken_rt,
	.switched_from		= switched_from_rt,
#endif


	.set_curr_task          = set_curr_task_wrr,
	.task_tick		= task_tick_wrr,

	.update_curr		= update_curr_wrr,
};


