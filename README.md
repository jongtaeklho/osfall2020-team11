```
Linux kernel
============

This file was moved to Documentation/admin-guide/README.rst

Please notice that there are several guides for kernel developers and users.
These guides can be rendered in a number of formats, like HTML and PDF.

In order to build the documentation, use ``make htmldocs`` or
``make pdfdocs``.

There are various text files in the Documentation/ subdirectory,
several of them using the Restructured Text markup notation.
See Documentation/00-INDEX for a list of what is contained in each file.

Please read the Documentation/process/changes.rst file, as it contains the
requirements for building and running the kernel, and information about
the problems which may result by upgrading your kernel.
```

# Overview
리눅스 커널에 새로운 스케줄링 정책인 Weighted Round Robin(WRR)을 추가하는 과제이다. WRR은 기존의 Round Robin과 비슷하다. 다른 점은 프로세스가 각자의 weight를 가지고 있고, time slice가 weight * 10ms로 결정된다는 것이다. 모든 프로세스의  weight는 10으로 시작한다. 또 WRR에서는 프로세스의 효율성을 위해 정해진 시간마다 load balancing을 해준다. load balancing이란 weight의 총합이 가장 큰 런큐에서 weight의 총합이 가장 작은 런큐로 task를 migrate해주는 것이다.  

# Data structures & Functions

## Process Hierarchy
WRR 스케줄링 정책은 기존의 CFS와 RT사이에 존재한다. 따라서 `struct sched_class rt_sched_class`의 `.next`를 `&fair_sched_class`에서 `wrr_sched_class`로 바꾼다.  

## `struct wrr_rq`와 `struct sched_wrr_entity`
### struct wrr_rq
```
unsigned long long sum;
struct wrr_entity *head;
struct wrr_entity *tail;
```
### struct sched_wrr_entity
```
struct sched_entity se;
struct sched_wrr_entity *nxt;
strcut sched_wrr_entity *pre;
long time_slice;
long weight;
struct wrr_rq *parent;
struct task_struct *parent_t;
```
wrr_rq는 각 cpu의 런큐에 있는 task들의 weight의 합을 sum에다 저장하고 있다. 이 sum은 load balancing할 때 런큐의 weight의 총합을 비교할 때 사용한다. wrr_rq에 linked list의 형태로 sched_wrr_entity들이 붙어있고, wrr_rq에서는 linked list의 head와 tail을 갖고있다.
sched_wrr_entity들은 각각의 weight와 time_slice를 갖고있고, time_slice는 weight * 10ms로 할당받고  0이 되면 cpu를  반납하게된다. 반납할 때는 다시 time_slice를 weight * 10ms로 초기화하여 런큐에 넣는다.

## kernel/sched/wrr.c
```  
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
```   
wrr.c는 위의 함수들로 구성되어 있다. 이 함수들 중 이번 WRR 스케줄링 정책에서 구현한 주요 함수들은 `enqueue_task_wrr`, `dequeue_task_wrr`, `pick_next_task_wrr`, `select_task_rq_wrr`, `task_tick_wrr`이다.
### enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
인자로 rq와 task_struct를 받는다. 주어진 task_struct를 rq->wrr에 linked list형태로 삽입한다. rq->wrr에 삽입할 때 `struct sched_wrr_entity`형태로 삽입하며 rq->wrr.sum에 삽입하는 task의 weight를 추가한다.

### dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags) 
rq->wrr에서 주어진 task_struct를 빼준다.

### pick_next_task_wrr(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
rq->wrr에서 실행중인 task의 다음에 연결되어 있는 task를 return 해준다. 이때 만약 다음 task가 없다면 NULL return한다.

### select_task_rq_wrr(struct task_struct *p, int select_cpu, int sd_flag, int flags) 
현재 실행중인 cpu중 런큐에 있는 task들의 weight의 총합이 가장 작은 cpu를 return한다.

### task_tick_wrr(struct rq *rq, struct task_struct *p, int queued)
kernel/sched/core.c에서 매 time_slice마다 호출되며 만약 p의 주어진 time_slice가 0이하라면 cpu를 반납하고 rq->wrr의 제일 뒤로 옮겨진다.   

## Load Balancing
+ kernel/sched/core.c의 `scheduler_tick`함수에서 2초마다 `wrr_load_balance`를 호출하도록 구현하였다. 
+ `static int can_migrate(struct rq *rq, struct task_struct *p, int dst_cpu)`는 task의 migrate 가능 여부를 체크한다.
+ `static int migrate_task_wrr(int src_cpu, int dst_cpu)`는 src_cpu의 런큐에서 weight가 가장 큰 task를 dst_cpu로 migrate 시킨다.
+ `void wrr_load_balance(void)` 각 cpu의 런큐에서 sum을 체크해서 sum이 가장 큰 cpu와 가장 작은 cpu를 선택해서 migrate_task_wrr의 인자로 집어 넣는다.

## System Calls
+  kernel/sched/core.c에 구현했다.
+ `asmlinkage long sched_setweight(pid_t pid, int weight)` process의 weight를 1~20 사이로 설정한다.
+ `asmlinkage long sched_getweight(pid_t pid)` process의 weight를 return해준다.

# Performance
![plot](https://user-images.githubusercontent.com/48852336/98513267-ef545c00-22aa-11eb-88d7-f3fdb0d1a3b8.png)



# Improvement
이번 과제에서는 스케줄링할 때 task들의 weight를 10으로 고정하고, 테스팅할 때 임의로 task의 weight를 변경하였다. 스케줄러에서 task가 cpu를 점유한 시간을 저장하고 그 시간이 많다면 해당하는 task의 weight를 줄여주준다면 각 task들을 좀 더 공평하게 스케줄링 할 수 있을 것이다. 그렇게 되면 계속해서 cpu를 점유한 task의 weight가 많이 작아질 수 있다. weight를 계속 유지하는 것이 아니라 시간을 정해놓고 일정 시간이 지나면 다시 weight를 리셋해서 이러한 경우를 방지할 수 잇을 것이다.



# Lessons learned  
 과제를 일찍 시작해야겠다고 다짐했다. 마감이 얼마 남지 않고 시작해서 며칠을 밤새서 과제를 마무리했는데 정말 힘들었다. 다음과제는 나오자 마자 시작할 계획이다. 깃에 푸쉬할 때 의도하지 않은 파일들이 변경돼서 몇 번 고생했다. 이번에는 브랜치를 따로 생성하지 않고 마스터에다가 바로 푸쉬했는데 다음부터는 각자 브랜치를 생성하여 작업을 할 예정이다.

