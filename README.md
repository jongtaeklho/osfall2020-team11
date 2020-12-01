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

# How to Build this Kernel
본 프로젝트는 set_rotation, rotlock_read, rotlock_write, rotunlock_read, rotunlock_write 등 5개의 시스템 콜을 구현한다. 주어진 tizen 커널에 다음과 같은 과정을 거쳐 새로운 system call를 등록한다. 
1. syscall 번호 등록
    - `arch/arm64/include/asm/unistd32.h`, `include/uapi/asm-generic/unistd.h`에서 398~402번을 할당한다. 
    - `include/uapi/asm-generic/unistd.h`, `arch/arm64/include/asm/unistd.h` 등의 최대 syscall 범위를 402보다 큰 숫자(예:404)로 바꿔 새로 만든 시스템 콜을 사용할 수 있게 한다.
2. `include/linux/prinfo.h` 헤더 파일에 range descriptor 역할을 하는 Node 구조체를 선언한다.
3. `include/linux/syscalls.h` 헤더 파일에 asmlinkage를 붙여 각 함수를 선언한다.
4. `~/kernel/rotation.c`에 sys_ptree 함수를 구현한다.
5. 커널 Makefile(`kernel/Makefile`)에 `obj-y +=rotation.o` 를 추가해 컴파일될 수 있도록 한다.
6. `./build-rpi3-arm64.sh`을 실행하여 변경된 커널을 컴파일한다.
# Design & Implementation
1. `long set_rotation(int degree)`
`set_rotation()`을 실행하면 우선 error 상황을 확인한다. `buf` 또는 `nr` 포인터가 `NULL`을 가리키거나, entry 개수가 1 미만인 등 함수 argument에 문제가 있다면 `-EINVAL`을 반환한다. access_ok를 통해 `nr`, `buf`를 확인하여 access 불가능할 경우 `-EFAULT`를 반환한다.
커널 프로그램에서 user memory space에 계속 접근하는 것은 커널 패닉을 일으킬 수 있으므로, 입력된 `nr` 값을 `nr_max`에 재할당한다. 이제 프로세스 트리를 만들 준비가 끝났으면 `process_tree_traversal()` 함수를 실행하여 preorder로 각 프로세스 정보를 확인하고 `process_info`에 저장한다. 이 과정에서 실행 중인 프로세스가 바뀌는 것을 막기 위해 `read_lock(&tasklist_lock)`을 걸어놓는다. 이 전체 과정이 끝나면 `copy_to_user`를 통해 트리 탐색 결과를 `buf`에, 탐색한 프로세스 개수를 `nr`에 복사한다. 그 뒤, 모든 실행 중인 프로세스 수를 반환한다. `buf`에 포함되지 않은 프로세스도 ptree 실행 당시 실행 중이었다면 반환값에 포함한다.
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
