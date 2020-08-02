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
