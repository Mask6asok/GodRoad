[参考此文]([https://veritas501.space/2017/10/16/%E7%BF%BB%E8%AF%91_%E7%BC%96%E5%86%99%E4%B8%80%E4%B8%AALinux%E8%B0%83%E8%AF%95%E5%99%A8/](https://veritas501.space/2017/10/16/翻译_编写一个Linux调试器/))

# 启动程序

一个简单的调试器，必须支持运行程序，即由此调试器运行起被调试程序，从而使得调试器能够调试被调试程序

使用fork函数，可以划分出两个线程，于是我们可以在这两个线程中分别运行被调试程序和调试器

```c++
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Program name not specified\n";
        return -1;
    }
    auto prog = argv[1];
    auto pid = fork(); // use fork to separete two process
    if (pid == 0)      // child process
    {
        // debuggee
        execute_debugee(prog); // lunch to-debugger program
    }
    else if (pid >= 1)
    {
        // debugger
        debugger dbg{prog, pid}; //
        dbg.run();
    }
}
```

在`execute_debugee`函数中，使用ptrace和execl函数来建立被调试程序运行环境

```c++
void execute_debugee(const std::string &prog_name)
{
    if (ptrace(PTRACE_TRACEME, 0, 0, 0) < 0)
    {
        std::cerr << "Error in ptrace\n";
        return;
    }
    execl(prog_name.c_str(), prog_name.c_str(), nullptr);
}
```

ptrace使可以实时监控并修改另一个进程的运行，此处用在子进程中，PTRACE_TRACEME运行子进程被父进程跟踪，即希望父进程（调试器）跟踪子进程（被调试程序，execl执行起被调试程序

到这里为止，运行调试器，调试器只是会简单地将被调试程序运行起来，不会做其余操操作

# 简易调试

在子进程可以运行起来后，需要调试器去控制该进程，上文中的父进程函数体内，使用了一个debugger类，该类是自己实现的调试器

```c++
class debugger
{
public:
    debugger(std::string prog_name, pid_t pid)
        : m_prog_name{std::move(prog_name)}, m_pid{pid} {}
    void run();
private:
    void handle_command(const std::string &line);
    void continue_execution();
    std::string m_prog_name;
    pid_t m_pid;
};
```

调试器希望能够以交互的方式来调试子进程。当子进程启动完成后，它将会发送SIGTRAP信号，表示这是一个跟踪或者遇到断点（这是PTRACE_TRACEME的结果？）。在run方法中，调试器会中断进程运行，直到接收到SIGTRAP信号，意味着子进程已启动，此时可以读取调试命令，进行下一步操作

```c++
void debugger::run()
{
    int wait_status;
    auto options = 0;
    waitpid(m_pid, &wait_status, options); 

    char *line = nullptr;
    while ((line = linenoise("dbg> ")) != nullptr) 
    {
        handle_command(line);
        linenoiseHistoryAdd(line);
        linenoiseFree(line);
    }
}
```

>linenoise是易于调试命令行信息获取的一个库

获取到调试命令后，需要根据命令进行调试功能，此处只实现继续执行程序的命令，即gdb中的

“c”。在handle_command函数中，对调试命令进行切割，指令匹配，分配到不同的调试功能函数

```c++
void debugger::handle_command(const std::string &line)
{
    auto args = split(line, ' ');
    auto command = args[0];

    if (is_prefix(command, "c"))
    {
        continue_execution();
    }
    else
    {
        std::cerr << "Unknown command\n";
    }
}
```

继续执行程序的调试功能的实现依赖于ptrace，使用PTRACE_CONT告知子进程继续执行，接着中断调试进程，直到子进程的下一个信号到来

```c++
void debugger::continue_execution()
{
    ptrace(PTRACE_CONT, m_pid, nullptr, nullptr); // use PTRACE_CONT to tell the process continue
    int wait_status;
    auto options = 0;
    waitpid(m_pid, &wait_status, options);
}
```

---

测试一下我们的调试器，写一个小程序，有一些简单的输出

```c
// gcc test.c -o test
#include <stdlib.h>
int main()
{
    puts("I AM REPEATER");
    char buf[0x10];
    memset(buf, 0, 0x10);
    puts(buf);
}
```

使用我们写的调试器对其进行调试，在程序开始运行时，被调试程序是没有输出的，通过输入`c`指令后，程序运行，输出了“I AM REPEATER”，由于调试器功能简单，没有进行后续的交互，无法往被调试程序中输入信息，程序直接运行结束

```
➜  minidbg git:(tut_setup) ✗ ./minidbg test
minidbg> c
I AM REPEATER

minidbg>
```

# 设置断点

调试器最基本的一个功能就是在程序中设立断点，使得程序运行到断点处可以暂停运行

断点可分为硬件断点与内存断点，硬件断点是通过设置架构指定的寄存器来设置中断，有数量限制；而软件断点是通过修改运行时程序的内存来设置中断，没有数量上的限制

我们主要实现内存断点的机制，即通过修改运行时程序的内存来实现中断。由于上头我们利用了PTRACE_TRACEME，在父进程中可以读写子进程的内存

x86上的`int 3`指令（0xcc）就是用来中断的指令，当处理器执行到`int 3`时，就会跳转到断点中断处理程序，在Linux上即发送SIGTRAP信号，类似上文的子进程启动后发送的信号，我们可以捕获（waitpid）并进行跟踪

建立一个断点类，提供两个功能，实现断点的开启和关闭

```c++
class breakpoint
{
public:
    breakpoint(pid_t pid, std::intptr_t addr)
        : m_pid{pid}, m_addr{addr}, m_enabled{false}, m_saved_data {}
    void enable();
    void disable();
    auto is_enabled() const -> bool { return m_enabled; }
    auto get_address() const -> std::intptr_t { return m_addr; }

private:
    pid_t m_pid;
    std::intptr_t m_addr; // breakpoint address
    bool m_enabled;       // switch
    uint8_t m_saved_data; // source data
};
```

下断时，要把断点地址处的指令替换成`int 3`，并保存原有的指令

PTRACE_PEEKDATA可以用来从子进程的指定地址读出一个字长的数据，读出原有指令

由于`int 3`的机器码是0xcc，只占一个字节的长度，只需要保存原有指令的最低位即可

PTRACE_POKEDATA可以用来往子进程的指定地址写入一个字长的数据，替换原有指令

```c++
void breakpoint::enable()
{
    auto data = ptrace(PTRACE_PEEKDATA, m_pid, m_addr, nullptr); // read source data
    m_saved_data = static_cast<uint8_t>(data & 0xff);            // keep 1 byte
    uint64_t int3 = 0xcc;                                        // int 3
    uint64_t data_with_int3 = ((data & ~0xff) | int3);           // change data
    ptrace(PTRACE_POKEDATA, m_pid, m_addr, data_with_int3);      // write new data
    m_enabled = true;                                            // means this breakpoint is open
}
```

取消断点时，要把断点地址处的指令还原成原指令，就是上面的逆过程

```c++
void breakpoint::disable()
{
    auto data = ptrace(PTRACE_PEEKDATA, m_pid, m_addr, nullptr);
    auto restored_data = ((data & ~0xff) | m_saved_data); // one byte
    ptrace(PTRACE_POKEDATA, m_pid, m_addr, restored_data);
    m_enabled = false; // means this breakpoint is close
}
```

修改debugger类，使其支持断点功能，添加一个设置断点的函数，再新建一个map变量来存储地址对应的断点对象，同时在handle_command函数中添加断点调试指令

记录用户下断的地址与之对应的断点类

```c++
std::unordered_map<std::intptr_t,breakpoint> m_breakpoints;
```

设置断点函数，使用断点类的enable函数

```c++
void debugger::set_breakpoint_at_address(std::intptr_t addr)
{
    std::cout << "Set breakpoint at address 0x" << std::hex << addr << std::endl;
    breakpoint bp{m_pid, addr};
    bp.enable();
    m_breakpoints[addr] = bp;
}
```

修改handle_command函数使调试器支持下断指令

```c++
// c command
else if (is_prefix(command, "b"))
{
    std::string addr{args[1], 2}; // b 0xdeadbeef
    set_breakpoint_at_address(std::stol(addr, 0, 16));
}
// others
```

---

测试断点功能，还是利用上面的test程序，由于目前只实现了下断操作，到达断点后，继续执行程序的控制还没实现，即无法步过该断点，以下是test函数的机器码地址

```
Dump of assembler code for function main:
   0x00000000004005a7 <+0>:	push   rbp
   0x00000000004005a8 <+1>:	mov    rbp,rsp
   0x00000000004005ab <+4>:	sub    rsp,0x20
   0x00000000004005af <+8>:	mov    rax,QWORD PTR fs:0x28
   0x00000000004005b8 <+17>:	mov    QWORD PTR [rbp-0x8],rax
   0x00000000004005bc <+21>:	xor    eax,eax
   0x00000000004005be <+23>:	lea    rdi,[rip+0xcf]        # 0x400694
   0x00000000004005c5 <+30>:	call   0x400490 <puts@plt>
   0x00000000004005ca <+35>:	lea    rax,[rbp-0x20]
   0x00000000004005ce <+39>:	mov    edx,0x10
   0x00000000004005d3 <+44>:	mov    esi,0x0
   0x00000000004005d8 <+49>:	mov    rdi,rax
   0x00000000004005db <+52>:	call   0x4004b0 <memset@plt>
   0x00000000004005e0 <+57>:	lea    rax,[rbp-0x20]
   0x00000000004005e4 <+61>:	mov    rdi,rax
   0x00000000004005e7 <+64>:	call   0x400490 <puts@plt>
   0x00000000004005ec <+69>:	mov    eax,0x0
   0x00000000004005f1 <+74>:	mov    rcx,QWORD PTR [rbp-0x8]
   0x00000000004005f5 <+78>:	xor    rcx,QWORD PTR fs:0x28
   0x00000000004005fe <+87>:	je     0x400605 <main+94>
   0x0000000000400600 <+89>:	call   0x4004a0 <__stack_chk_fail@plt>
   0x0000000000400605 <+94>:	leave  
   0x0000000000400606 <+95>:	ret    
End of assembler dump.
```

为了演示断点效果，我们分别把断点打在`puts`函数上与函数后，观察两次的输出，在`puts`函数上下断的程序是不会有输出的，证明断点设置成功

```
➜  minidbg git:(tut_setup) ✗ ./minidbg test
dbg> b 0x4005c5
Set breakpoint at address 0x4005c5
dbg> c
dbg> c
dbg> 
➜  minidbg git:(tut_setup) ✗ ./minidbg test
dbg> b 0x4005db
Set breakpoint at address 0x4005db
dbg> c
I AM REPEATER   <- 有输出
dbg> c
dbg> 
```

# 寄存器

调试器需要支持对寄存器与内存的读取与修改，内存的修改在上面设置断点一节已经通过PTRACE_PEEKDATA与PTRACE_POKEDATA实现，读取寄存器的功能可由PTRACE_GETREGS来实现，因此可以简单封装一下两个函数用来读写内存，数值范围是64位数

```c++
uint64_t debugger::read_memory(uint64_t address) {
    return ptrace(PTRACE_PEEKDATA, m_pid, address, nullptr);
}
void debugger::write_memory(uint64_t address, uint64_t value) {
    ptrace(PTRACE_POKEDATA, m_pid, address, value);
}
```

同时修改handle_command函数使其支持内存读写指令

```c++
else if (is_prefix(command, "m")) // memory
{
    std::string addr{args[2], 2}; // address
    if (is_prefix(args[1], "r"))  // read. m r address
    {
        std::cout << std::hex << read_memory(std::stol(addr, 0, 16)) << std::endl;
    }
    else if (is_prefix(args[1], "w")) // write. m w address value
    {
        std::string val{args[3], 2};
        write_memory(std::stol(addr, 0, 16), std::stol(val, 0, 16));
    }
}
```

寄存器的个数与名称会依据硬件平台架构的不同而改变，我们只关注于x86_64平台上的常用寄存器，即我们再gdb中看到的那些，对于浮点寄存器和向量寄存器不予理会

我们只关注于以下的寄存器，由于读出的寄存器符合DWARF，我们可以建立一个指定寄存器与其结构体索引之间的关系

```c++
enum class reg {
    rax, rbx, rcx, rdx,
    rdi, rsi, rbp, rsp,
    r8,  r9,  r10, r11,
    r12, r13, r14, r15,
    rip, rflags,    cs,
    orig_rax, fs_base,
    gs_base,
    fs, gs, ss, ds, es
}; // 支持的寄存器
constexpr std::size_t n_registers = 27;
struct reg_descriptor {
    reg r;				// 寄存器
    int dwarf_r;		// 索引
    std::string name;	// 名字
}; // 寄存器信息结构体
const std::array<reg_descriptor, n_registers> g_register_descriptors {{
    { reg::r15, 15, "r15" },
    { reg::r14, 14, "r14" },
    { reg::r13, 13, "r13" },
    { reg::r12, 12, "r12" },
    { reg::rbp, 6, "rbp" },
    { reg::rbx, 3, "rbx" },
    { reg::r11, 11, "r11" },
    { reg::r10, 10, "r10" },
    { reg::r9, 9, "r9" },
    { reg::r8, 8, "r8" },
    { reg::rax, 0, "rax" },
    { reg::rcx, 2, "rcx" },
    { reg::rdx, 1, "rdx" },
    { reg::rsi, 4, "rsi" },
    { reg::rdi, 5, "rdi" },
    { reg::orig_rax, -1, "orig_rax" },
    { reg::rip, -1, "rip" },
    { reg::cs, 51, "cs" },
    { reg::rflags, 49, "eflags" },
    { reg::rsp, 7, "rsp" },
    { reg::ss, 52, "ss" },
    { reg::fs_base, 58, "fs_base" },
    { reg::gs_base, 59, "gs_base" },
    { reg::ds, 53, "ds" },
    { reg::es, 50, "es" },
    { reg::fs, 54, "fs" },
    { reg::gs, 55, "gs" },
}}; // 寄存器信息列表，与user_regs_struct结构体对应
struct user_regs_struct
{
  __extension__ unsigned long long int r15;
  __extension__ unsigned long long int r14;
  __extension__ unsigned long long int r13;
  __extension__ unsigned long long int r12;
  __extension__ unsigned long long int rbp;
  __extension__ unsigned long long int rbx;
  __extension__ unsigned long long int r11;
  __extension__ unsigned long long int r10;
  __extension__ unsigned long long int r9;
  __extension__ unsigned long long int r8;
  __extension__ unsigned long long int rax;
  __extension__ unsigned long long int rcx;
  __extension__ unsigned long long int rdx;
  __extension__ unsigned long long int rsi;
  __extension__ unsigned long long int rdi;
  __extension__ unsigned long long int orig_rax;
  __extension__ unsigned long long int rip;
  __extension__ unsigned long long int cs;
  __extension__ unsigned long long int eflags;
  __extension__ unsigned long long int rsp;
  __extension__ unsigned long long int ss;
  __extension__ unsigned long long int fs_base;
  __extension__ unsigned long long int gs_base;
  __extension__ unsigned long long int ds;
  __extension__ unsigned long long int es;
  __extension__ unsigned long long int fs;
  __extension__ unsigned long long int gs;
}; // 返回寄存器结构体
```

PTRACE_GETREGS的使用方法需要传给ptrace一个user_regs_struct的结构，即读寄存器的函数如下

```c++
uint64_t get_register_value(pid_t pid, reg r) {
    user_regs_struct regs;
    ptrace(PTRACE_GETREGS, pid, nullptr, & regs);
    auto it = std::find_if(begin(g_register_descriptors), end(g_register_descriptors),
                       [r](auto&& rd) { return rd.r == r; });
    return *(reinterpret_cast<uint64_t*>(& regs) + (it - begin(g_register_descriptors)));
}
```

为了读出指定寄存器的值，就需要对regs进行下一步操作，由于g_register_descriptors表与user_regs_struct结构相同（有意而为），我们就可以通过在g_register_descriptors表中找到参数r对应寄存器的位置，将user_regs_struct作为以一个unsigned long long int型的数组进行取值，即可取得指定寄存器的值

写入寄存器的函数也类似，先读出寄存器列表，再修改寄存器结构体上指定位置处的值，最后重新写入

```c++
void set_register_value(pid_t pid, reg r, uint64_t value) {
    user_regs_struct regs;
    ptrace(PTRACE_GETREGS, pid, nullptr, & regs);
    auto it = std::find_if(begin(g_register_descriptors), end(g_register_descriptors),
                           [r](auto&& rd) { return rd.r == r; });

    *(reinterpret_cast<uint64_t*>(&regs) + (it - begin(g_register_descriptors))) = value;
    ptrace(PTRACE_SETREGS, pid, nullptr, & regs);
}
```

添加一些辅助函数，如寄存器名与寄存器引用之间的对应关系

```c++
std::string get_register_name(reg r) {
    auto it = std::find_if(begin(g_register_descriptors), end(g_register_descriptors),
                           [r](auto&& rd) { return rd.r == r; });
    return it->name;
}

reg get_register_from_name(const std::string& name) {
    auto it = std::find_if(begin(g_register_descriptors), end(g_register_descriptors),
                           [name](auto&& rd) { return rd.name == name; });
    return it->r;
}
```

最后给dubugger添加一个打印寄存器的功能

```c++
void debugger::dump_registers() {
    for (const auto& rd : g_register_descriptors) {
        std::cout << rd.name << " 0x"
                  << std::setfill('0') << std::setw(16) << std::hex << get_register_value(m_pid, rd.r) << std::endl;
    }
}
```

修改handle_command函数使其支持寄存器操作

```c++
else if (is_prefix(command, "r")) // register
{
    if (is_prefix(args[1], "p")) // print. r p
    {
        dump_registers();
    }
    else if (is_prefix(args[1], "r")) // read. r r rax
    {                                 // first turn register name into index
        std::cout << get_register_value(m_pid, get_register_from_name(args[2])) << std::endl;
    }
    else if (is_prefix(args[1], "w")) // write. r w rax 0x1
    {
        std::string val{args[3], 2}; // input format: 0xdeadbeef
        set_register_value(m_pid, get_register_from_name(args[2]), std::stol(val, 0, 16));
    }
}
```

---

测试功能，首先在puts函数下断点，执行到此处的0xcc后，控制权教给调试器，通过`m r 0x4005c5`调试指令读出内存中0x4005c5处的值，可以发现最低位为0xcc，即中断指令，通过`r r rip`调试指令读出rip寄存器的值，此时为0x4005c6，为中断指令下一指令的地址，通过`r w rip 0x1234`调试指令将rip寄存器的值修改为0x1234，通过`r p`调试指令打印所有寄存器的值

```
➜  minidbg git:(tut_setup) ✗ ./minidbg test
dbg> b 0x4005c5
Set breakpoint at address 0x4005c5
dbg> c
dbg> m r 0x4005c5
458d48fffffec6cc
dbg> r r rip
4005c6
dbg> r w rip 0x1234
dbg> r p
r15 0x0000000000000000
r14 0x0000000000000000
r13 0x00007fffddc017f0
r12 0x00000000004004c0
rbp 0x00007fffddc01710
rbx 0x0000000000000000
r11 0x0000000000000003
r10 0x0000000000000000
r9 0x00007f3c47a43d80
r8 0x00007f3c47a43d80
rax 0x0000000000000000
rcx 0x0000000000400610
rdx 0x00007fffddc01808
rsi 0x00007fffddc017f8
rdi 0x0000000000400694
orig_rax 0xffffffffffffffff
rip 0x0000000000001234
cs 0x0000000000000033
eflags 0x0000000000000246
rsp 0x00007fffddc016f0
ss 0x000000000000002b
fs_base 0x00007f3c47c574c0
gs_base 0x0000000000000000
ds 0x0000000000000000
es 0x0000000000000000
fs 0x0000000000000000
gs 0x0000000000000000
```

# 步过断点

当我们给程序下断后，程序执行到断点会将控制权交给调试器，此时希望能在调试器上手动步过该断点，让程序可以正常执行，即要完善上文中的continue_execution函数

流程是我们在让程序继续执行的时候，得判断当前是否处于一个断点中，如果是，我们就得停用该断点并继续执行

判断当前是否处于一个断点上的方法是获取当前的pc指针（rip），与断点记录表m_breakpoints进行对比，即可知道

编写辅助函数，封装对rip寄存器的读写

```c++
uint64_t debugger::get_pc()
{
    return get_register_value(m_pid, reg::rip);
}
void debugger::set_pc(uint64_t pc)
{
    set_register_value(m_pid, reg::rip, pc);
}
```

步过断点的函数，需要通过关闭断点的方式修改内存指令，并重置rip寄存器，使其执行原有指令

```c++
void debugger::step_over_breakpoint() {
    auto possible_breakpoint_location = get_pc() - 1;
    if (m_breakpoints.count(possible_breakpoint_location)) {
        auto& bp = m_breakpoints[possible_breakpoint_location];
        if (bp.is_enabled()) {
            auto previous_instruction_address = possible_breakpoint_location;
            set_pc(previous_instruction_address);
            bp.disable();
            ptrace(PTRACE_SINGLESTEP, m_pid, nullptr, nullptr);
            wait_for_signal();
            bp.enable();
        }
    }
}
void debugger::wait_for_signal() {
    int wait_status;
    auto options = 0;
    waitpid(m_pid, &wait_status, options);
}
```

修改continue_execution函数，使其支持在断点上继续运行

---

测试调试器在遇到断点后能否继续执行，测试发现，子程序能正常输出，可以说明调试器的功能是正确的

```
➜  minidbg git:(tut_setup) ✗ ./minidbg test
dbg> b 0x4005c5
Set breakpoint at address 0x4005c5
dbg> c
dbg> c
I AM REPEATER

```

# 单步执行

todo