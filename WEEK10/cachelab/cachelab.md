有关计算机缓存方面的知识

# Part A

首先是用C语言写一个模拟cache运行，要求使用到LRU策略，文件中给了一个参考程序csim-ref，通过对csim-ref的逆向分析，了解基本运作方式。使用一个计数功能，为每个cache块计数，优先取计数大的块实现LRU策略

```c
#include "cachelab.c"
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

int h, v, s, E, b, S;
int hit_count, miss_count, eviction_count;
char t[1000];
FILE *fp;

void printUsage();
void init_cache();
void update(unsigned int address);
void update_stamp();
void parse_trace();
// void printSummary(hit_count, miss_count, eviction_count);

typedef struct // cache struct
{
    int valid_bits;
    int tag;
    int stamp;
} cache_line, *cache_asso, **cache;

cache _cache_ = NULL;

int main(int argc, char *argv[])
{
    int opt;
    while (-1 != (opt = (getopt(argc, argv, "hvs:E:b:t:")))) // get parameter from comand line
    {
        switch (opt)
        {
        case 'h':
            // h = 1;
            printUsage();
            break;
        case 'v':
            v = 1;
            break;
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            strcpy(t, optarg);
            break;
        default:
            printUsage();
            break;
        }
    }
    puts("");
    if (s <= 0 || E <= 0 || b <= 0 || t == NULL) // invalid parameter
    {
        // printUsage();
        return -1;
    }
    S = 1 << s;         // S = 2 ^ s
    fp = fopen(t, "r"); // check if input file is invalid
    if (fp == NULL)
    {
        printf("trace file open error\n");
        exit(-1);
    }
    init_cache();                                        // initial cache
    parse_trace();                                       // interaction
    printSummary(hit_count, miss_count, eviction_count); // show result
    return 0;
}

void printUsage()
{
    printf("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    puts("Options:");
    puts("  -h         Print this help message.");
    puts("  -v         Optional verbose flag.");
    puts("  -s <num>   Number of set index bits.");
    puts("  -E <num>   Number of lines per set.");
    puts("  -b <num>   Number of block offset bits.");
    puts("  -t <file>  Trace file.");
    puts("\nExamples:");
    printf("  linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("  linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
    exit(0);
}

void init_cache() // initial cache, get a empty space use as cache
{
    _cache_ = (cache)malloc(sizeof(cache_asso) * S);
    for (int i = 0; i < S; i++)
    {
        _cache_[i] = (cache_asso)malloc(sizeof(cache_line) * E); // form cache struct
        for (int j = 0; j < E; j++)
        {
            _cache_[i][j].valid_bits = 0;
            _cache_[i][j].tag = -1;
            _cache_[i][j].stamp = -1;
        }
    }
}

void parse_trace() // cache interaction
{
    char operation;
    unsigned int address;
    int size;
    while (fscanf(fp, " %c %xu,%d\n", &operation, &address, &size) > 0) // read operation from input file
    {
        switch (operation) // I is ignore
        {
        case 'L':
            update(address); // one access
            break;
        case 'M':
            update(address); // two access
        case 'S':
            update(address); // one access
        default:
            break;
        }
        update_stamp(); // counter for LRU
    }
    fclose(fp);
}

void update_stamp()
{
    for (int i = 0; i < S; i++)
    {
        for (int j = 0; j < E; j++)
        {
            if (_cache_[i][j].valid_bits == 1)
            {
                _cache_[i][j].stamp++; // LRU flag
            }
        }
    }
}

void update(unsigned int address) // operation
{

    int setindex = (address >> b) & ((-1U) >> (64 - s)); // select idx from address
    int tag = address >> (b + s);                        // select tag from address

    int max_stamp = INT_MIN; //
    int max_stamp_index = -1;

    for (int i = 0; i < E; i++)
    {
        if (_cache_[setindex][i].tag == tag) // same tag, hit, reset the stamp
        {
            _cache_[setindex][i].stamp = 0;
            hit_count++;
            return;
        }
    }
    for (int i = 0; i < E; i++)
    {
        if (_cache_[setindex][i].valid_bits == 0) // unused, no need to evict
        {
            _cache_[setindex][i].valid_bits = 1; // use a free one
            _cache_[setindex][i].tag = tag;
            _cache_[setindex][i].stamp = 0;
            miss_count++;
            return;
        }
    }
    miss_count++;
    eviction_count++;
    for (int i = 0; i < E; i++) // full, need to evict, use LRU
    {
        if (_cache_[setindex][i].stamp > max_stamp)
        {
            max_stamp = _cache_[setindex][i].stamp;
            max_stamp_index = i; // victim block
        }
    }
    _cache_[setindex][max_stamp_index].tag = tag;
    _cache_[setindex][max_stamp_index].stamp = 0;
}
```

# Part B

要求实现高效的矩阵转置算法，通过源码来看只有三种矩阵，分别是32x32，64x64和61x67，cache的规模为s=5，E=1，b=5即32组，每组一行，每行32字节数据

源文件中给出了逐个转置的算法，但此算法对cache不友好，为了减少cache的miss和evict，使用分块转置方式，即将原矩阵切割，按块转置，尽可能使得每次操作都能充分利用cache中的元素

对于32x32和64x64的矩阵，分块大小为8，61x67的矩阵，分块大小为16

在实际测试中发现，分块后直接转置的方法适用于61x67和32x32，而在64x64中效率仍不高，为解决此问题，参考[https://xjdkc.github.io/CMU%20CacheLab.html](https://xjdkc.github.io/CMU CacheLab.html)中提到的数据暂存方法进行转置

```c
#define MIN(a, b) ((a) < (b) ? (a) : (b))
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int temp[8];
    int i, j, k, l;
    int block_size = N % 8 ? 16 : 8;    
    // 32x32 and 64x64 get the block size is 8, while 61x67 get the block size is 16
    for (i = 0; i < N; i += block_size) // rol
    {
        for (j = 0; j < M; j += block_size) // column
        {
            if (N % 8) // for 61x67 case
            {
                for (k = i; k < MIN(i + block_size, N); k++) // rol
                {
                    // traverse the elem in a block(16x16), transpose it
                    for (l = j; l < MIN(j + block_size, M); l++) 
                        B[l][k] = A[k][l];
                }
            }
            else // for 32x2, 64x64 case, divide the matrix into 8x8 block matrix
            {
                // previous 4 rol in B-block store previous 4 rol in A-block, left half is right
                for (k = i; k < i + 4; k++) 
                {
                    for (l = 0; l < 8; l++)
                        temp[l] = A[k][j + l];
                    for (l = 0; l < 4; l++) // one rol spilit into 2 column
                    {
                        B[j + l][k] = temp[l];
                        B[j + l][k + 4] = temp[l + 4];
                    }
                }
                for (k = j + 4; k < j + 8; k++)
                {
                    // temp[0 -> 4] = B[k - 4][i + 4 -> i + 8], these are the data placed in B-block left-down
                    for (l = 0; l < 4; l++) 
                        temp[l] = B[k - 4][i + l + 4];
                    // temp[4 -> 8] = A[i + 4 -> i + 8][k], these are the data placed in B-block right-down
                    for (l = 4; l < 8; l++) 
                        temp[l] = A[i + l][k];
                    // transpose the data in A-block left-down into B-block right-up
                    for (l = 4; l < 8; l++) 
                        B[k - 4][i + l] = A[i + l][k - 4];
                    // put the temp[0 -> 8] to B-block, form the B-block left-down and right-down
                    for (l = 0; l < 8; l++) 
                        B[k][i + l] = temp[l];
                }
            }
        }
    }
}
```



