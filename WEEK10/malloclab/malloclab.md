实现一个动态内存分配器，类似glibc中的ptmalloc，主要处理好释放块的链表，以及合并空闲块之类的操作

```c
/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)
#define SIZE1 (1 << 4)
#define SIZE2 (1 << 5)
#define SIZE3 (1 << 6)
#define SIZE4 (1 << 7)
#define SIZE5 (1 << 8)
#define SIZE6 (1 << 9)
#define SIZE7 (1 << 10)
#define SIZE8 (1 << 11)
#define SIZE9 (1 << 12)
#define SIZE10 (1 << 13)
#define SIZE11 (1 << 14)
#define SIZE12 (1 << 15)
#define SIZE13 (1 << 16)
#define SIZE14 (1 << 17)
#define SIZE15 (1 << 18)
#define SIZE16 (1 << 19)
#define SIZE17 (1 << 20)
#define LISTS_NUM 18

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into  word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read and write a pointer at address p */
#define GET_PTR(p) ((unsigned int *)(long)(GET(p)))
#define PUT_PTR(p, ptr) (*(unsigned int *)(p) = ((long)ptr))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

static char *heap_listp;

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void insert_list(void *bp);
int getListOffset(size_t size);
void delete_list(void *bp);
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    char *bp;
    int i;
    // use sbrk to get a heap which include 18 free chain-lists and 4 init chunks
    if ((heap_listp = mem_sbrk((LISTS_NUM + 4) * WSIZE)) == (void *)-1)
    {
        return -1;
    }
    // initial 4 chunks
    PUT(heap_listp + LISTS_NUM * WSIZE, 0);
    PUT(heap_listp + (1 + LISTS_NUM) * WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + (2 + LISTS_NUM) * WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + (3 + LISTS_NUM) * WSIZE, PACK(0, 1));
    for (int i = 0; i < LISTS_NUM; i++)
    { // make 18 free chain-list to be NULL
        PUT_PTR(heap_listp + WSIZE * i, NULL);
    }
    if ((bp = extend_heap(CHUNKSIZE / WSIZE)) == NULL)
    { // extend heap with a free block of CHUNKSIZE bytes
        return -1;
    }
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    if (!size) // invalid size
    {
        return NULL;
    }
    // fix un aligned size
    size_t fix_size = size <= DSIZE ? 2 * DSIZE : DSIZE * ((size + DSIZE + DSIZE - 1) / DSIZE);
    void *bp = NULL;
    if ((bp = find_fit(fix_size)) != NULL)
    {                        // get a chunk from free chain-list
        place(bp, fix_size); // see if need to cut
        return bp;
    }
    // no avaliable chunk, get more space by extend chunk
    size_t extendSize = MAX(fix_size, CHUNKSIZE);
    if ((bp = extend_heap(extendSize / WSIZE)) == NULL)
    {
        return NULL; // extend failed
    }
    place(bp, fix_size);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0)); // free flag in header
    PUT(FTRP(ptr), PACK(size, 0)); // flag flag in footer
    coalesce(ptr);                 // see if need to coalesce
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (!size)
    { // realloc size 0 is free
        mm_free(ptr);
        return NULL;
    }
    size_t fix_size = size <= DSIZE ? 2 * DSIZE : DSIZE * ((size + DSIZE + DSIZE - 1) / DSIZE);
    void *nptr = NULL;
    if (fix_size == GET_SIZE(HDRP(ptr))) // same
    {
        return ptr;
    }
    nptr = mm_malloc(size); // a new chunk
    if (nptr)               // valid
    {
        memmove(nptr, ptr, size); // copy data
    }
    mm_free(ptr); // free the old chunk
    return nptr;
}

void *extend_heap(size_t words)
{
    char *bp;
    size_t size = (words % 2) ? ((words + 1) * WSIZE) : (words * WSIZE); // keep even

    if ((bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }
    PUT(HDRP(bp), PACK(size, 0));         // set header
    PUT(FTRP(bp), PACK(size, 0));         // set footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // set next's header
    return coalesce(bp);                  // coalesce if previous' is free
}

void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // previous chunk's use flag
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // next chunk's use flag
    size_t size = GET_SIZE(HDRP(bp));
    if (prev_alloc && next_alloc) // previous and next are using
    {
        bp = bp;
    }
    else if (prev_alloc && !next_alloc) // next is free
    {
        delete_list(NEXT_BLKP(bp));      // remove next chunk from free chain-list
        size += GET_SIZE(NEXT_BLKP(bp)); // coalesce so this chunk plus next chunk
        PUT(HDRP(bp), PACK(size, 0));    // modify header infomation
        PUT(FTRP(bp), PACK(size, 0));    // move footer to new address
    }
    else if (next_alloc && !prev_alloc) // previous is free
    {
        delete_list(PREV_BLKP(bp));              // remove previous chunk from free chain-list
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));   // coalesce so this chunk plus previous chunk
        PUT(FTRP(bp), PACK(size, 0));            // modify footer information
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // modify previous chunk's header infomation
        bp = PREV_BLKP(bp);                      // bp is point to previous chunk
    }
    else // previous and next are free
    {
        delete_list(NEXT_BLKP(bp)); // remove next chunk from free chain-list
        delete_list(PREV_BLKP(bp)); // remove previous chunk from free chain-list
        size += GET_SIZE(NEXT_BLKP(bp)) + GET_SIZE(PREV_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // new header is previous chunk's header
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // new footer is next chunk's footer
        bp = PREV_BLKP(bp);                      // bp is point to previous chunk
    }
    insert_list(bp);
    return bp;
}
// latest free chunk need to join in a suitable chain-list
void insert_list(void *bp)
{
    int size = GET_SIZE(HDRP(bp));   //get this chunk's size
    int index = getListOffset(size); //choice a suitable chain-list
    if (GET_PTR(heap_listp + WSIZE * index) == NULL)
    {                                            // see if this chain-list is empty
        PUT_PTR(heap_listp + WSIZE * index, bp); // join in
        PUT_PTR(bp, NULL);                       // clean this chunk
        PUT_PTR((unsigned int *)bp + 1, NULL);
    }
    else
    {                                                         // this chain-list already has a chunk
        PUT_PTR(bp, GET_PTR(heap_listp + WSIZE * index));     // bp -> top chunk
        PUT_PTR(GET_PTR(heap_listp + WSIZE * index) + 1, bp); // top chunk -> bp
        PUT_PTR((unsigned int *)bp + 1, NULL);                // clean this chunk
        PUT_PTR(heap_listp + WSIZE * index, bp);              // put bp into this chain-list
    }
}
// take off a chunk in a chain-list
void delete_list(void *bp)
{
    int size = GET_SIZE(HDRP(bp));   //get this chunk's size
    int index = getListOffset(size); //choice a suitable chain-list
    if (GET_PTR(bp) == NULL && GET_PTR((unsigned int *)bp + 1) == NULL)
    {                                              //this chunk is the only one in chain-list
        PUT_PTR(heap_listp + WSIZE * index, NULL); // set null
    }
    else if (GET_PTR(bp) == NULL && GET_PTR((unsigned int *)bp + 1) != NULL)
    {
        // this chunk is the bottom one in chain-list
        PUT_PTR((GET_PTR((unsigned int *)GET_PTR((unsigned int *)bp + 1))), NULL); // bp->prev->next=null
        PUT_PTR(GET_PTR((unsigned int *)bp + 1), NULL);                            // bp->prev=null
    }
    else if (GET_PTR((unsigned int *)bp + 1) == NULL && GET_PTR(bp) != NULL)
    {
        //this chunk is the top one in chain-list
        PUT(heap_listp + WSIZE * index, GET_PTR(bp)); // chain->next=bp->next
        PUT_PTR(GET_PTR(bp) + 1, NULL);               // chain->prev=null
    }
    else
    {
        //this chunk is the middle one in chain-list
        PUT_PTR(GET_PTR((unsigned int *)bp + 1), GET_PTR(bp));     // bp->prev->next=bp->next
        PUT_PTR(GET_PTR(bp) + 1, GET_PTR((unsigned int *)bp + 1)); // bp->next->prev=bp->prev
    }
}

int getListOffset(size_t size)
{
    if (size <= SIZE1)
    {
        return 0;
    }
    else if (size <= SIZE2)
    {
        return 1;
    }
    else if (size <= SIZE3)
    {
        return 2;
    }
    else if (size <= SIZE4)
    {
        return 3;
    }
    else if (size <= SIZE5)
    {
        return 4;
    }
    else if (size <= SIZE6)
    {
        return 5;
    }
    else if (size <= SIZE7)
    {
        return 6;
    }
    else if (size <= SIZE8)
    {
        return 7;
    }
    else if (size <= SIZE9)
    {
        return 8;
    }
    else if (size <= SIZE10)
    {
        return 9;
    }
    else if (size <= SIZE11)
    {
        return 10;
    }
    else if (size <= SIZE12)
    {
        return 11;
    }
    else if (size <= SIZE13)
    {
        return 12;
    }
    else if (size <= SIZE14)
    {
        return 13;
    }
    else if (size <= SIZE15)
    {
        return 14;
    }
    else if (size <= SIZE16)
    {
        return 15;
    }
    else if (size <= SIZE17)
    {
        return 16;
    }
    else
    {
        return 17;
    }
}
// choice a suitable position in chain-list
void *find_fit(size_t size)
{
    int index = getListOffset(size);
    unsigned int *ptr;
    while (index < 18)
    {
        ptr = GET_PTR(heap_listp + 4 * index);
        while (ptr != NULL)
        {
            if (GET_SIZE(HDRP(ptr)) >= size) // inorder
            {
                return (void *)ptr;
            }
            ptr = GET_PTR(ptr);
        }
        index++;
    }
    return NULL;
}

void place(void *bp, size_t size)
{
    size_t chunk_size = GET_SIZE(HDRP(bp));
    delete_list(bp);
    if ((chunk_size - size) >= (2 * DSIZE))
    {                                 // bp is too large
        PUT(HDRP(bp), PACK(size, 1)); // set new chunk size in header with alloc flag
        PUT(FTRP(bp), PACK(size, 1)); // set new chunk size in footer with alloc flag
        void *nbp = NEXT_BLKP(bp);
        PUT(HDRP(nbp), PACK(chunk_size - size, 0)); // set new chunk size in remain chunk's header
        PUT(FTRP(nbp), PACK(chunk_size - size, 0)); // set new chunk size in remain chunk's footer
        insert_list(nbp);
    }
    else
    {
        PUT(HDRP(bp), PACK(chunk_size, 1)); // alloc flag
        PUT(FTRP(bp), PACK(chunk_size, 1));
    }
}
```

