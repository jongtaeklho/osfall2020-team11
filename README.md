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
'''
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
'''
wrr.c는 위의 함수들로 구성되어 있다. 이 함수들 중 이번 WRR 스케줄링 정책에서 구현한 함수는 `enqueue_task_wrr`, `dequeue_task_wrr`, `pick_next_task_wrr`, `select_task_rq_wrr`, `task_tick_wrr`이다.
##`enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)`
인자로 rq와 task_struct를 받는다. 주어진 task_struct를 rq->wrr에 linked list형태로 삽입한다. rq->wrr에 삽입할 때 `struct sched_wrr_entity`형태로 삽입하며 rq->wrr.sum에 삽입하는 task의 weight를 추가한다.

##`dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)`
rq->wrr에서 주어진 task_struct를 빼준다.

##`pick_next_task_wrr(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)`
rq->wrr에서 실행중인 task의 다음에 연결되어 있는 task를 return 해준다. 이때 만약 다음 task가 없다면 NULL return한다.



2. `void process_tree_traversal(struct prinfo *process_infos, struct task_struct *task, int max_cnt, int *curr_cnt, int* true_cnt)`
`process_infos`는 프로세스 정보를 담아둘 struct 배열, `task`는 현재 확인 중인 프로세스의 task_struct, `max_cnt`는 `process_infos`에 담을 프로세스 개수의 상한, `curr_cnt`는 프로세스 iteration을 위한 커서, `true_cnt`는 모든 실행 중인 프로세스 수를 뜻한다.
`process_tree_traversal()`을 실행하면 현재 새로운 프로세스를 확인 중이므로 `true_cnt`이 가리키는 값을 1만큼 증가시키고, 아직 주어진 프로세스 탐색 개수만큼 프로세스를 탐색하지 않았을 경우 `task`가 가리키는 프로세스를 탐색한다. 프로세스의 state, pid, parent_pid, first_child_pid, next_sibling_pid, uid, 프로세스명 등의 정보가 모두 task_struct의 멤버로 포함되어 있으므로, task의 각 멤버에 접근한 뒤 이 정보를 `process_infos`의 `curr_cnt`번째 원소에 저장하는 방식으로 정보를 순서대로 저장한다. 
Linux에서 모든 프로세스는 `init_task`의 child process이다. 그러므로 `init_task`를 root로 하고 프로세스 트리를 순회하면 모든 프로세스를 탐색할 수 있다. 현재 확인중인 struct의 정보가 모두 입력되면, 남은 프로세스들을 preorder로 순회한다. task_struct는 다른 프로세스의 task_struct와 linked list로 연결되어 있기 때문에, linux에서 제공하는 list_for_each_entry 매크로 함수를 통해 이 linked list를 순회하는 방식으로 preorder traversal을 구현하였다.
# Investigation of the Process Tree
테스트 코드를 실행해보면 pid 0, 1, 2는 항상 swapper, systemd, kthreadd가 차지하는 것을 알 수 있다. 각 스레드의 역할은 다음과 같다.
- swapper 프로세스는 CPU에 스케줄링할 다른 프로세스가 없을 때 실행되는 프로세스로, 부팅 시 pid 1에 해당하는 systemd를 실행하는 것 외에는 다른 역할이 없다. swapper는 가장 낮은 우선순위를 가지는 프로세스이다. 
- systemd (system daemon) 프로세스는 시스템을 부팅하고 다른 프로세스들을 관리하는 역할을 하며, 기존에 사용되었던 init을 대체하는 역할을 한다. 기존 init 프로세스보다 빠르고 효율적으로 부팅하기 위해 부팅 시 실행하는 프로세스를 최소화하고, 프로세스를 최대한 병렬적으로 실행한다. systemctl 명령어로 커널 상에서 사용할 수 있다. 
- kthreadd 프로세스는 모든 커널 스레드의 부모 프로세스이며, 커널 스레드를 생성하는 프로세스이다. `kthread_create()`, `kthread_run()`, `kthread_bind()`, `kthread_stop()`, `kthread_should_stop()`, `kthread_data()` 등의 api 호출시 커널 스레드를 fork한다.

# Lessons learned
수업 내용과 프로젝트 내용이 큰 관련이 없어 어려움을 많이 겪었다. 하지만 과제 수행을 위해 따로 커널에 대해 공부하면서 커널 프로그래밍에 대한 지식을 쌓을 수 있었고, 커널 프로그래밍에서 커널 관련 지식의 중요성을 느낄 수 있었다. 
