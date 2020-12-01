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
2. `include/linux/rotation.h` 헤더 파일에 range descriptor 역할을 하는 Node 구조체를 선언한다.
3. `include/linux/syscalls.h` 헤더 파일에 asmlinkage를 붙여 각 함수를 선언한다.
4. `~/kernel/rotation.c`에 sys_ptree 함수를 구현한다.
5. 커널 Makefile(`kernel/Makefile`)에 `obj-y +=rotation.o` 를 추가해 컴파일될 수 있도록 한다.
6. `./build-rpi3-arm64.sh`을 실행하여 변경된 커널을 컴파일한다.
# Design & Implementation
1. `struct Node (node_t)`
본 프로그램에서 lock에서 hold하기 위해 기다리고 있는 프로세스들은 `head_wait`라는 linked list에 기록되고, 현재 lock을 잡고 실행 중인 프로세스들은 `head_acquired`라는 linked list에 기록된다. 이 linked list들의 각 노드는 `struct Node`로 구현하였다. 해당 구조체의 각 멤버는 다음의 정보를 나타낸다.
``` C
struct Node
{
    int rw; // 0이면 reader, 1이면 writer
    long st, ed;    // 각도 범위의 시작점 및 종점
    // ed >= st이면 범위: st~ed
    // ed < st이면 범위: st~360, 0~ed
    pid_t pid; // exit_rotlock 실행 시 wait queue에서 해당 프로세스의 entry를 찾기 위한 process id
    wait_queue_head_t *wq_head; // 해당 노드의 
    struct list_head nodes; // Linked list 구현을 위한 list_head
};
```
`head_wait`에 기록된 모든 lock을 기다리는 프로세스는 `wq_rot`이라는 wait queue에 기록되어 있다.  
2. `long set_rotation(int degree)`
`set_rotation()`은 두 변수를 동적 할당하고, 주어진 `degree`를 static 변수인 `deg_curr`에 저장해 `rotunlock`, `rotlock` 관련 시스템 콜을 실행할 준비를 한다.  `set_rotation()`은 마지막으로 `wake_up_all()` 함수를 통해 `wq_rot`에 들어있는 모든 프로세스를 깨워 각 프로세스의 `rotlock` 및 `rotunlock` 함수가 실행 순서를 조절하도록 한다. 반환값은 따로 사용할 필요가 없어 에러일 때 -1, 정상적으로 실행되었을 때 0을 반환하도록 하였다.  
3. `long rotlock_read(int degree, int range)`
`rotlock_read()`를 실행하면 우선 현재 프로세스를 뜻하는 새로운 노드를 만든다. 이 노드를 `head_wait`에 연결하여 프로세스를 대기 상태로 간주하고, 해당 프로세스가 지금 실행될 수 있는지 확인한다. 확인 과정은 다음과 같이 정해졌으며, writer starvation이 일어나지 않도록 구현되었다. 전체 과정은 `mutex_lock()` 안에 들어있어서 race condition의 영향을 받지 않는다.  
    + 현재 실행 중인 프로세스 중 각도 범위 안에 `degree`를 포함하는 writer가 없는지 확인  
    + 위의 조건을 만족하는 프로세스가 없으면, 자신 이전에 들어온 대기 중인 프로세스 중 각도 범위 안에 `degree`를 포함하는 writer가 없는지 확인  
    + 위의 조건을 만족하는 프로세스가 없으면, 자신의 각도 범위가 `degree`를 포함하는지 확인  
이 과정을 모두 통과한 프로세스의 노드는 `head_acquired`의 tail으로 옮겨간다. 그 뒤, 해당 프로세스가 실행된다. 이 과정을 통과하지 못한 프로세스는 wait queue에 다시 들어가 `wake_up_all()`이 실행될 때까지 기다린다.  
4. `long rotlock_write(int degree, int range)`
`rotlock_write()`는 `rotlock_read()`와 유사하다. 다만, writer는 한 번에 하나만 실행될 수 있고, 프로세스의 실행 여부를 확인하는 과정만 다르다. 확인 과정은 다음과 같다.
    + 현재 실행 중인 프로세스 중 각도 범위 안에 `degree`를 포함하는 프로세스(reader/writer 모두 포함)가 없는지 확인  
    + 위의 조건을 만족하는 프로세스가 없으면, 자신 이전에 들어온 대기 중인 프로세스 중 각도 범위 안에 `degree`를 포함하는 프로세스(reader/writer 모두 포함)가 없는지 확인  
    + 위의 조건을 만족하는 프로세스가 없으면, 자신의 각도 범위가 `degree`를 포함하는지 확인  
5. `long rotunlock_read(int degree, int range)`
`rotunlock_read()`는 `head_acquired`에 있는 실행 중인 프로세스들을 확인하여, 해당 프로세스의 각도 범위 및 reader/writer 종류가 `rotunlock_read()`을 부른 자신의 각도 범위 및 종류와 정확히 일치하는지 확인한다. 만약 정확히 일치하는 노드를 찾는다면 해당 노드를 지우고, 0을 반환한다. 그러한 노드가 없다면 아무 것도 하지 않고 -1을 반환한다.   
6. `long rotunlock_write(int degree, int range)`
`rotunlock_write()`는 `rotunlock_read()`와 매우 유사하며, Unlock 중 확인하는 readwer/writer 종류만 다르다.  
7. `void exit_rotlock()`  
`exit_rotlock()`은 프로세스가 종료될 때 rotation lock을 지우고 나가게 하는 함수로, `do_exit()` 내에서 실행된다. 해당 함수는 두 가지 기능을 갖고 있다. 첫째로, 종료 직전까지 실행 중인 프로세스였을 경우, 프로세스 종류에 맞는 `rotunlock`을 호출하여 다른 프로세스가 실행되도록 한다. 둘째로, 종료 직전에 대기 중이었던 프로세스였을 경우, 해당 프로세스의 `wq_entry`를 wait queue에서 제거한다. 대기 중이었던 프로세스가 writer라면 해당 프로세스 전후의 프로세스가 모두 reader일 경우 해당 프로세스의 제거로 인해 다음 프로세스가 락을 hold하게 될 수 있다. 그러므로 writer가 제거되었다면 `wake_up_all()`을 한번 실행한다.

# Lessons learned
수업 때 다뤘던 Lock은 간단한 형태만 주어져서 금방 끝낼 수 있을 것이라고 생각했는데, lock에 특수한 조건이 있어 어려움이 많았다. 락 구현에 이용한 linked list의 노드를 처음에는 들어온 순서대로 나열하지 않고 range가 같은 것끼리 묶어서 만드는 형태를 시도하였는데, 해당 형태로 fairness를 보장하려면 시간 손해가 크다고 판단하여 중간에 구현 방식이 크게 변한것이 가장 큰 난관이었다. Lock을 구현할 때 fairness를 염두에 둔다면 시간 순서대로 정리하는 것이 매우 유리하다는 것을 체감할 수 있었다. 
