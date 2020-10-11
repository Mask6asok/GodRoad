# 练习1

**给未被映射的地址映射上物理页**

完成`do_pgfault（mm/vmm.c）`函数，给未被映射的地址映射上物理页。设置访问权限 的时候需要参考页面所在 VMA 的权限，同时需要注意映射物理页时需要操作内存控制 结构所指定的页表，而不是内核的页表。注意：在LAB2 EXERCISE 1处填写代码。执行`make qemu`后，如果通过check_pgfault函数的测试后，会有“check_pgfault() succeeded!”的输出，表示练习1基本正确。

`do_pgfault`是用于发生页错误时的处理函数，由对应的handler处理

```c
static int
pgfault_handler(struct trapframe *tf) {
    extern struct mm_struct *check_mm_struct;
    print_pgfault(tf);
    if (check_mm_struct != NULL) {
        return do_pgfault(check_mm_struct, tf->tf_err, rcr2());
    }
    panic("unhandled page fault.\n");
}
```

从调用方式中看，传进去的参数包含`check_mm_struct`，`tf->tf_err`，`rcr2`，分别代表了一个虚拟内存空间变量，页访问异常错误码，与发生页访问异常的线性地址。

在`do_pgfault`函数中，根据传进来的参数，先寻找一个包含`addr`的`vma`

```c
	struct vma_struct *vma = find_vma(mm, addr);
```

接着判断该`vma`是否合法（非空且`vm_start`＜`addr`），若合法则认为这是一次合法访问，只是没有建立虚拟内存与物理内存之间的映射关系导致的缺页，会分配一个空闲页并修改页表完成虚拟地址到物理地址之间的映射，此时可以返回发生页访问错误的地址继续执行；若不合法，则认为这是一次非法访问

```c
    if (vma == NULL || vma->vm_start > addr)
    {
        cprintf("not valid addr %x, and  can not find it in vma\n", addr);
        goto failed;
    }
	// go on
	......
failed:
	return ret;
```

在合法访问的情况下，`do_pgfault`函数会针对不同的页访问异常错误码执行不同的功能

```c
    switch (error_code & 3)
    {
    default:
        /* error code flag : default is 3 ( W/R=1, P=1): write, present */
    case 2: /* error code flag : (W/R=1, P=0): write, not present */
        if (!(vma->vm_flags & VM_WRITE))
        {
            cprintf("do_pgfault failed: error code flag = write AND not present, but the addr's vma cannot write\n");
            goto failed;
        }
        break;
    case 1: /* error code flag : (W/R=0, P=1): read, present */
        cprintf("do_pgfault failed: error code flag = read AND present\n");
        goto failed;
    case 0: /* error code flag : (W/R=0, P=0): read, not present */
        if (!(vma->vm_flags & (VM_READ | VM_EXEC)))
        {
            cprintf("do_pgfault failed: error code flag = read AND not present, but the addr's vma cannot read or exec\n");
            goto failed;
        }
    }
```

此处只检查异常错误码的低两位，涉及写映射、写未映射、读映射、读未映射，在这个过程中遇到无权限读写的时候都直接执行`failed`

下面就涉及到建立虚拟内存地址与物理内存地址之间的映射关系，注释中提到了`get_pte`来获取出错的线性地址对应的虚拟页起始地址对应到的页表项，而在UCore中会使用页表项来保存物理地址，从而找到地址

先使用`get_pte`寻找`adde`对应的页表项，并判断非空，否则执行`failed`

```c
	if ((ptep = get_pte(mm->pgdir, addr, 1)) == NULL)
    {
        cprintf("get_pte in do_pgfault failed\n");
        goto failed;
    }
```

判断页表项对应的页地址是否已经分配存在，若不存在则申请一个，否则就是被放到了外存中，需要实现练习2的页面替换算法

```c
    if (*ptep == 0)
    {
        if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL)
        {
            cprintf("pgdir_alloc_page in do_pgfault failed\n");
            goto failed;
        }
    }
	else
    {
        ...
    }
```

# 练习2

**补充完成基于FIFO的页面替换算法**

完成`vmm.c`中的`do_pgfault`函数，并且在实现FIFO算法的`swap_fifo.c`中完成`map_swappable`和`swap_out_vistim`函数。通过对swap的测试。

页面替换发生在页表项对应的页地址已经分配，也就是`(*ptep == 0) == false`的情况，此时页地址被放到外存中，需要替换进内存

`map_swappable`函数根据FIFO原则，将一个物理页添加到可换出物理页链表的末尾，更新物理页对应的虚拟地址

```c
static int
_fifo_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *head = (list_entry_t *)mm->sm_priv;
    list_entry_t *entry = &(page->pra_page_link);

    assert(entry != NULL && head != NULL);
    //record the page access situlation
    /*LAB3 EXERCISE 2: YOUR CODE*/
    //(1)link the most recent arrival page at the back of the pra_list_head qeueue.
    list_add(head, entry);
    return 0;
}
```

`swap_out_victim`函数根据FIFO原则，选择可换出物理页链表的首元素，作为被换出的物理页

```c
static int
_fifo_swap_out_victim(struct mm_struct *mm, struct Page **ptr_page, int in_tick)
{
    list_entry_t *head = (list_entry_t *)mm->sm_priv;
    assert(head != NULL);
    assert(in_tick == 0);
    /* Select the victim */
    /*LAB3 EXERCISE 2: YOUR CODE*/
    //(1)  unlink the  earliest arrival page in front of pra_list_head qeueue
    //(2)  set the addr of addr of this page to ptr_page
    /* Select the tail */
    list_entry_t *le = head->prev; // 被换出页
    assert(head != le);
    struct Page *p = le2page(le, pra_page_link); // 获取page
    list_del(le); // 删除最早的页
    assert(p != NULL);
    *ptr_page = p;
    return 0;
}
```

在`do_pgfault`中，先判断swap机制是否已初始化，若已初始化，则进行替换操作

```c
            struct Page *page = NULL;
            if ((ret = swap_in(mm, addr, &page)) != 0) // 外存换入内存
            {
                cprintf("swap_in in do_pgfault failed\n");
                goto failed;
            }
            page_insert(mm->pgdir, page, addr, perm); // 建立虚拟地址和物理地址之间的关系
            swap_map_swappable(mm, addr, page, 1); // 设为可交换
            page->pra_vaddr = addr; // 更新其虚拟页的信息
```

