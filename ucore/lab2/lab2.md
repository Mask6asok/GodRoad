# lab2

物理内存相关

## 练习1

**实现first-fit连续物理内存分配算法**

要考虑地址连续的空闲块之间的合并操作

在建立空闲页块链表时，需要按照空闲页块起始地址来排序，形成一个有序的链表。可能会修改default_pmm.c中的default_init，default_init_memmap，default_alloc_pages， default_free_pages等相关函数

```c
// 空双向链表
static void
default_init(void)
{
    list_init(&free_list);
    nr_free = 0;
}

// 初始化每个物理页面记录，将其加入free_list
static void
default_init_memmap(struct Page *base, size_t n)
{
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p++)
    {
        assert(PageReserved(p));
        p->flags = 0;
        SetPageProperty(p);
        p->property = 0;
        set_page_ref(p, 0);
        list_add_before(&free_list, &(p->page_link));
    }
    nr_free += n;
    //first block
    base->property = n;
}
// 申请page
static struct Page *
default_alloc_pages(size_t n)
{
    assert(n > 0);
    if (n > nr_free)
    {
        return NULL;
    }
    list_entry_t *le, *len;
    le = &free_list;

    while ((le = list_next(le)) != &free_list)
    {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n)
        {
            int i;
            for (i = 0; i < n; i++)
            {
                len = list_next(le);
                struct Page *pp = le2page(le, page_link);
                SetPageReserved(pp);
                ClearPageProperty(pp);
                list_del(le);
                le = len;
            }
            if (p->property > n)
            {
                (le2page(le, page_link))->property = p->property - n;
            }
            ClearPageProperty(p);
            SetPageReserved(p);
            nr_free -= n;
            return p;
        }
    }
    return NULL;
}
// 释放page，注意看前后是否合并
static void
default_free_pages(struct Page *base, size_t n)
{
    assert(n > 0);
    assert(PageReserved(base));

    list_entry_t *le = &free_list;
    struct Page *p;
    while ((le = list_next(le)) != &free_list)
    {
        p = le2page(le, page_link);
        if (p > base)
        {
            break;
        }
    }
    //list_add_before(le, base->page_link);
    for (p = base; p < base + n; p++)
    {
        list_add_before(le, &(p->page_link));
    }
    base->flags = 0;
    set_page_ref(base, 0);
    ClearPageProperty(base);
    SetPageProperty(base);
    base->property = n;

    p = le2page(le, page_link);
    if (base + n == p)
    {
        base->property += p->property;
        p->property = 0;
    }
    le = list_prev(&(base->page_link));
    p = le2page(le, page_link);
    if (le != &free_list && p == base - 1)
    {
        while (le != &free_list)
        {
            if (p->property)
            {
                p->property += base->property;
                base->property = 0;
                break;
            }
            le = list_prev(le);
            p = le2page(le, page_link);
        }
    }

    nr_free += n;
    return;
}
```

## 练习2

**实现寻找虚拟地址对应的页表项**

要获取表项时，如果查询所在页目录项不存在，要申请一个内存物理页，设置其引用数目为1，再获取对应的物理地址，将其内容初始化为0，再将其加入到页目录项；已存在则直接返回

```c
pte_t *
get_pte(pde_t *pgdir, uintptr_t la, bool create) 
{
    pde_t *pdep = &pgdir[PDX(la)];
    if (!(*pdep & PTE_P))
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep = pa | PTE_U | PTE_W | PTE_P;
    }
    return &((pte_t *)KADDR(PDE_ADDR(*pdep)))[PTX(la)];
}
```

**请描述页目录项（Pag Director Entry）和页表（Page Table Entry）中每个组成部分的含义和以及对ucore而言的潜在用处**

页是4k对齐，因此低12位可以当作标志位使用

**页目录项**：

- bit 0(P): resent 位，若该位为 1 ,则 PDE 存在，否则不存在。
- bit 1(R/W): read/write 位，若该位为 0 ,则只读，否则可写。
- bit 2(U/S): user/supervisor位。
- bit 3(PWT): page-level write-through，若该位为1则开启页层次的写回机制。
- bit 4(PCD): page-level cache disable，若该位为1,则禁止页层次的缓存。
- bit 5(A): accessed 位，若该位为1,表示这项曾在地址翻译的过程中被访问。
- bit 6: 该位忽略。
- bit 7(PS): 这个位用来确定 32 位分页的页大小，当该位为 1 且 CR4 的 PSE 位为 1 时，页大小为4M，否则为4K。
- bit 11:8: 这几位忽略。
- bit 32:12: 页表的PPN（页对齐的物理地址）。

**页表项**：

页表项除了第 7 ， 8 位与 PDE 不同，其余位作用均相同。

- bit 7(PAT): 如果支持 PAT 分页，间接决定这项访问的 4 K 页的内存类型;如果不支持，这位保留（必须为 0 ）。
- bit 8(G): global 位。当 CR4 的 PGE 位为 1 时，若该位为 1 ，翻译是全局的;否则，忽略该位。

其中被忽略的位可以被操作系统用于实现各种功能;和权限相关的位可以用来增强ucore的内存保护机制;access 位可以用来实现内存页交换算法。

**如果ucore执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？**

x

## 练习3

**释放某虚地址所在的页并取消对应二级页表项的映射**

将该页引用数目减一，如果变零则释放该页，页目录项清零，刷新tlb

```c
static inline void
page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep)
{
    if (*ptep & PTE_P)
    {
        struct Page *page = pte2page(*ptep);
        if (page_ref_dec(page) == 0)
        {
            free_page(page);
        }
        *ptep = 0;
        tlb_invalidate(pgdir, la);
    }
}
```

**数据结构Page的全局变量（其实是一个数组）的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是啥？**

当页目录项或页表项有效时，Page与页目录项和页表项有关系。

Page中存着物理页面信息，而页目录项记录着页表项，页表项记录一个物理页面信息，

**如果希望虚拟地址与物理地址相等，则需要如何修改lab2，完成此事？**

todo