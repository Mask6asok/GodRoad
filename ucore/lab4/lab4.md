# 练习1

**分配并初始化一个进程控制块**

`alloc_proc`函数（位于`kern/process/proc.c`中）负责分配并返回一个新的`struct proc_struct`结构，用于存储新建立的内核线程的管理信息。ucore需要对这个结构进行最基本的初始化，你需要完成这个初始化过程。

> 【提示】在alloc_proc函数的实现中，需要初始化的proc_struct结构中的成员变量至少包括：state/pid/runs/kstack/need_resched/parent/mm/context/tf/cr3/flags/name。

先查看一下`struct proc_struct`的结构

```c
struct proc_struct {             //进程控制块
    enum proc_state state;       //进程状态
    int pid;                     //进程ID
    int runs;                    //运行时间
    uintptr_t kstack;            //内核栈位置
    volatile bool need_resched;  //是否需要调度
    struct proc_struct *parent;  //父进程
    struct mm_struct *mm;        //进程的虚拟内存
    struct context context;      //进程上下文
    struct trapframe *tf;        //当前中断帧的指针
    uintptr_t cr3;               //当前页表地址
    uint32_t flags;              //进程
    char name[PROC_NAME_LEN + 1];//进程名字
    list_entry_t list_link;      //进程链表       
    list_entry_t hash_link;                  
};
```

在`alloc_proc`中申请一个`proc_struct`并初始化的代码如下

```c
// alloc_proc - alloc a proc_struct and init all fields of proc_struct
static struct proc_struct *
alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct)); // 申请内存空间
    if (proc != NULL)
    {
        //LAB4:EXERCISE1 YOUR CODE
        /*
     * below fields in proc_struct need to be initialized
     *       enum proc_state state;                      // Process state
     *       int pid;                                    // Process ID
     *       int runs;                                   // the running times of Proces
     *       uintptr_t kstack;                           // Process kernel stack
     *       volatile bool need_resched;                 // bool value: need to be rescheduled to release CPU?
     *       struct proc_struct *parent;                 // the parent process
     *       struct mm_struct *mm;                       // Process's memory management field
     *       struct context context;                     // Switch here to run process
     *       struct trapframe *tf;                       // Trap frame for current interrupt
     *       uintptr_t cr3;                              // CR3 register: the base addr of Page Directroy Table(PDT)
     *       uint32_t flags;                             // Process flag
     *       char name[PROC_NAME_LEN + 1];               // Process name
     */
        proc->state = PROC_UNINIT; 	// state 设为未初始化状态
        proc->pid = -1;				// 默认pid为-1
        proc->runs = 0;				// 运行时间为0
        proc->kstack = 0;			// 内核栈位置为0
        proc->need_resched = 0;		// 无需调度
        proc->parent = NULL;		// 无父进程
        proc->mm = NULL;			// 虚拟内存为空
        memset(&(proc->context), 0, sizeof(struct context)); // 上下文（寄存器）为空
        proc->tf = NULL;			// 栈指针为空
        proc->cr3 = boot_cr3;		// 页目录设为内核页目录表的基址
        proc->flags = 0;			// 标志位为0
        memset(proc->name, 0, PROC_NAME_LEN); // 进程名为0
    }
    return proc;
}
```

**1.  struct context context和struct trapframe *tf 成员 变量的含义和作用**

context是进程上下文结构体，其组成结构如下。主要用于进程切换时，保存前一个进程的运行现场状态，方便UCore在各个进程之间切换，不影响进程的运行

```c
struct context {
    uint32_t eip;               
    uint32_t esp;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
};
```

tf是中断帧结构的指针，总是指向内核的某个位置，中断帧组成结构如下。当进程从用户态进入内核态时，会使用中断帧来记录进程在进入内核态之前的状态，当此进程从内核态回到用户态时，需要调整中断帧以恢复让进程继续执行的各寄存器的值

```c
struct trapframe {
    struct pushregs tf_regs;
    uint16_t tf_gs;
    uint16_t tf_padding0;
    uint16_t tf_fs;
    uint16_t tf_padding1;
    uint16_t tf_es;
    uint16_t tf_padding2;
    uint16_t tf_ds;
    uint16_t tf_padding3;
    uint32_t tf_trapno;
    /* below here defined by x86 hardware */
    uint32_t tf_err;
    uintptr_t tf_eip;
    uint16_t tf_cs;
    uint16_t tf_padding4;
    uint32_t tf_eflags;
    /* below here only when crossing rings, such as from user to kernel */
    uintptr_t tf_esp;
    uint16_t tf_ss;
    uint16_t tf_padding5;
} __attribute__((packed));
```

# 练习2

**为新创建的内核线程分配资源**

创建一个内核线程需要分配和设置好很多资源。`kernel_thread`函数通过调用`do_fork`函数完成具体内核线程的创建工作。`do_kernel`函数会调用`alloc_proc`函数来分配并初始化一个进程控制块，但`alloc_proc`只是找到了一小块内存用以记录进程的必要信息，并没有实际分配这些资源。ucore一般通过`do_fork`实际创建新的内核线程。`do_fork`的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。你需要完成在`kern/process/proc.c`中的`do_fork`函数中的处理过程。它的大致执行步骤包括：

- 调用alloc_proc，首先获得一块用户信息块。
- 为进程分配一个内核栈。
- 复制原进程的内存管理信息到新进程（但内核线程不必做此事）
- 复制原进程上下文到新进程
- 将新进程添加到进程列表
- 唤醒新进程
- 返回新进程号

```c
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
{
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS) // 进程数限制
    {
        goto fork_out;
    }
    ret = -E_NO_MEM;
    //LAB4:EXERCISE2 YOUR CODE
    /*
     * Some Useful MACROs, Functions and DEFINEs, you can use them in below implementation.
     * MACROs or Functions:
     *   alloc_proc:   create a proc struct and init fields (lab4:exercise1)
     *   setup_kstack: alloc pages with size KSTACKPAGE as process kernel stack
     *   copy_mm:      process "proc" duplicate OR share process "current"'s mm according clone_flags
     *                 if clone_flags & CLONE_VM, then "share" ; else "duplicate"
     *   copy_thread:  setup the trapframe on the  process's kernel stack top and
     *                 setup the kernel entry point and stack of process
     *   hash_proc:    add proc into proc hash_list
     *   get_pid:      alloc a unique pid for process
     *   wakeup_proc:  set proc->state = PROC_RUNNABLE
     * VARIABLES:
     *   proc_list:    the process set's list
     *   nr_process:   the number of process set
     */

    //    1. call alloc_proc to allocate a proc_struct
    //    2. call setup_kstack to allocate a kernel stack for child process
    //    3. call copy_mm to dup OR share mm according clone_flag
    //    4. call copy_thread to setup tf & context in proc_struct
    //    5. insert proc_struct into hash_list && proc_list
    //    6. call wakeup_proc to make the new child process RUNNABLE
    //    7. set ret vaule using child proc's pid
    if ((proc = alloc_proc()) == NULL) // 申请 alloc_proc 并初始化
    {
        goto fork_out;
    }
    proc->parent = current; // 设父进程为当前进程

    if (setup_kstack(proc) != 0) // 分配内核栈
    {
        goto bad_fork_cleanup_proc;
    }
    if (copy_mm(clone_flags, proc) != 0) // 复制父进程内存信息
    {
        goto bad_fork_cleanup_kstack;
    }
    copy_thread(proc, stack, tf); // 复制中断帧和上下文信息

    bool intr_flag;
    local_intr_save(intr_flag); // 屏蔽中断，inir_flag设为1
    {
        proc->pid = get_pid(); // 获取当前进程pid
        hash_proc(proc); // hash映射
        list_add(&proc_list, &(proc->list_link)); // 加入进程链表
        nr_process++; // 进程数加一
    }
    local_intr_restore(intr_flag); // 恢复中断
    wakeup_proc(proc); // 唤醒新进程
    ret = proc->pid; // 返回新进程的pid
fork_out:
    return ret;

bad_fork_cleanup_kstack: // 内核栈失败
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}
```

**1. ucore是否做到给每个新fork的线程一个唯一的id**

分配进程id由`get_pid`函数生成，查看`get_pid`函数源码

```c
// get_pid - alloc a unique pid for process
static int
get_pid(void)
{
    static_assert(MAX_PID > MAX_PROCESS); // pid限制
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++last_pid >= MAX_PID)
    {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe)
    {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list)
        {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid)
            {
                if (++last_pid >= next_safe)
                {
                    if (last_pid >= MAX_PID)
                    {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid)
            {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}
```

在函数内部，会根据`proc_list`来获取已有的进程，并拿`last_pid`和这些进程的pid进行对比，若相同，会对`last_pid`进行自增，从而确保id唯一

# 练习3

**阅读代码，理解 proc_run 函数和它调用的函数如何完成进程切换的**

`proc_run`函数源码如下，主要流程是先屏蔽中断，再修改ESP、页表项和上下文切换，最后恢复中断

```c
void proc_run(struct proc_struct *proc)
{
    if (proc != current)
    {
        bool intr_flag; // 中断变量
        struct proc_struct *prev = current, *next = proc; // 区分当前进程与准备运行的进程
        local_intr_save(intr_flag); // 屏蔽中断
        {
            current = proc; // 切换到proc进程
            load_esp0(next->kstack + KSTACKSIZE); // 修改esp
            lcr3(next->cr3); // 修改页表项
            switch_to(&(prev->context), &(next->context)); // 上下文切换
        }
        local_intr_restore(intr_flag); // 允许中断
    }
}
```

其中的`switch_to`函数主要完成的是进程的上下文缺环，先保存当前的寄存器值，然后将下一进程的上下文信息读取到寄存器中

```c
switch_to:                      # switch_to(from, to)
    # save from's registers
    movl 4(%esp), %eax          #保存from的首地址
    popl 0(%eax)                #将返回值保存到context的eip
    movl %esp, 4(%eax)          #保存esp的值到context的esp
    movl %ebx, 8(%eax)          #保存ebx的值到context的ebx
    movl %ecx, 12(%eax)         #保存ecx的值到context的ecx
    movl %edx, 16(%eax)         #保存edx的值到context的edx
    movl %esi, 20(%eax)         #保存esi的值到context的esi
    movl %edi, 24(%eax)         #保存edi的值到context的edi
    movl %ebp, 28(%eax)         #保存ebp的值到context的ebp

    # restore to's registers
    movl 4(%esp), %eax          #保存to的首地址到eax
    movl 28(%eax), %ebp         #保存context的ebp到ebp寄存器
    movl 24(%eax), %edi         #保存context的ebp到ebp寄存器
    movl 20(%eax), %esi         #保存context的esi到esi寄存器
    movl 16(%eax), %edx         #保存context的edx到edx寄存器
    movl 12(%eax), %ecx         #保存context的ecx到ecx寄存器
    movl 8(%eax), %ebx          #保存context的ebx到ebx寄存器
    movl 4(%eax), %esp          #保存context的esp到esp寄存器
    pushl 0(%eax)               #将context的eip压入栈中
    ret
```



