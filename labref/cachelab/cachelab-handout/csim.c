#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>

#include "cachelab.h"

typedef uint64_t Address; /* always assume nonnegative address */

struct Line {
    int valid;      /* valid bit */
    uint64_t tag;   /* tag bit */
    int timestamp;  /* timestamp in [0, E - 1] for LRU replacement */
    char* block;    /* data of the block, not used in this experiment */
};

struct Set {
    struct Line* lines; /* array of lines */
};

struct Statistics {
    int hits;        /* statistics: cache hit */
    int misses;      /* statistics: cache miss */
    int evictions;   /* statistics: cache replacement */
};

struct Cache {
    int s;                        /* meta data: number of sets */
    int E;                        /* meta data: number of lines per set */
    int b;                        /* meta data: block size */
    struct Set* sets;             /* array of sets */
    struct Statistics statistics; /* statistics */
} cache;

int initCache(int s, int E, int b) {
    int numSets = 1 << s;
    cache.s = s;
    cache.E = E;
    cache.b = b;

    cache.sets = (struct Set*)malloc(numSets * sizeof(struct Set));
    if (cache.sets == NULL) {
        return 0;
    }

    for (int i = 0; i < numSets; ++i) {
        cache.sets[i].lines = (struct Line*)malloc(cache.E * sizeof(struct Line));
        if (cache.sets[i].lines == NULL) {
            return 0;
        }

        for (int j = 0; j < cache.E; ++j) {
            cache.sets[i].lines[j].valid = 0;
            cache.sets[i].lines[j].timestamp = 0;
            cache.sets[i].lines[j].block = NULL; /* we'll not use this field in the experiment */
        }
    }

    return 1;
}

void cleanupCache() {
    if (cache.sets == NULL) {
        return;
    }

    int numSets = 1 << cache.s;
    for (int i = 0; i < numSets; ++i) {
        if (cache.sets[i].lines == NULL) {
            continue;
        }

        for (int j = 0; j < cache.E; ++j) {
            free(cache.sets[i].lines[j].block);
        }

        free(cache.sets[i].lines);
    }

    free(cache.sets);
    cache.sets = NULL;
}

void printCache() {
    int S = (1 << cache.s);
    printf("\n");
    for (int i = 0; i < S; ++i) {
        for (int j = 0; j < cache.E; ++j) {
            struct Line* p = cache.sets[i].lines + j;
            printf("sets[%d].lines[%d] = {v = %d, tag = %lx, timestamp = %d}\n", 
                   i, j, p->valid, p->tag, p->timestamp);
        }
    }
}

/* For this this lab, you should assume that memory accesses are aligned properly, 
   such that a single memory access never crosses block boundaries. */
void visitCache(Address address, int verbose) {
    const int s = cache.s;
    const int b = cache.b;

    Address mask = (((Address)1) << s) - 1;
    int setIndex = (address >> b) & mask;
    uint64_t tag = address >> (s + b);

    struct Line* lines = cache.sets[setIndex].lines;
    int hitIndex = -1;
    int firstEmptyLineIndex = -1;
    int maxTimeStamp = -1;
    int replaceLineIndex = -1;

    /* parallel search by hardware to find the specified cache line */
    for (int i = 0; i < cache.E; ++i) {
        if (lines[i].valid) {
            if (tag == lines[i].tag) {
                hitIndex = i;
            } else if (lines[i].timestamp == 0) {
                replaceLineIndex = i;
            }

            if (maxTimeStamp < lines[i].timestamp) {
                maxTimeStamp = lines[i].timestamp;
            }
        } else if (firstEmptyLineIndex == -1) {
            firstEmptyLineIndex = i;
        }
    }

    if (hitIndex != -1) { /* cache hit */
        ++cache.statistics.hits;
        if (verbose) {
            printf(" hit");
        }

        int refTimeStamp = lines[hitIndex].timestamp;
        if (refTimeStamp < maxTimeStamp) {
            for (int i = 0; i < cache.E; ++i) {
                if (lines[i].timestamp > refTimeStamp) {
                    --lines[i].timestamp;
                }
            }

            lines[hitIndex].timestamp = maxTimeStamp;
        }
    } else { /* cache miss */
        ++cache.statistics.misses;
        if (verbose) {
            printf(" miss");
        }

        if (firstEmptyLineIndex != -1) { /* use the first empty line */
            lines[firstEmptyLineIndex].valid = 1;
            lines[firstEmptyLineIndex].tag = tag;
            lines[firstEmptyLineIndex].timestamp = maxTimeStamp + 1;
        } else { /* LRU replacement */
            ++cache.statistics.evictions;
            if (verbose) {
                printf(" eviction");
            }
            
            int refTimeStamp = lines[hitIndex].timestamp;
            for (int i = 0; i < cache.E; ++i) {
                if (lines[i].timestamp > refTimeStamp) {
                    --lines[i].timestamp;
                }
            }

            lines[replaceLineIndex].tag = tag;
            lines[replaceLineIndex].timestamp = cache.E - 1;
        }
    }

    // printCache();
}

void load(Address address, int size, int verbose) {
    if (verbose) {
        printf("L %lx,%d", address, size);
    }

    visitCache(address, verbose);

    if (verbose) {
        printf("\n");
    }
}

void store(Address address, int size, int verbose) {
    if (verbose) {
        printf("S %lx,%d", address, size);
    }

    visitCache(address, verbose);

    if (verbose) {
        printf("\n");
    }
}

void modify(Address address, int size, int verbose) {
    if (verbose) {
        printf("M %lx,%d", address, size);
    }

    visitCache(address, verbose); /* first load */
    visitCache(address, verbose); /* then store */

    if (verbose) {
        printf("\n");
    }
}

struct Options {
    int h;                  /* boolean value to show usage */
    int v;                  /* boolean value to show verbose info */
    int s;                  /* Number of set index bits */
    int E;                  /* number of lines per set */
    int b;                  /* Number of block bits */
    char filename[80];      /* file to be traced */
} options;

int parseOptions(int argc, char* argv[]) {
    int opt;
    int success = 1;
    const char* optionString = "hvs:E:b:t:";

    /* init options */
    options.h = 0;
    options.v = 0;

    /* parse arguments */
    while ((opt = getopt(argc, argv, optionString)) != -1) {
        switch (opt) {
            case 'h': 
                options.h = 1; break;
            case 'v': 
                options.v = 1; break;
            case 's': 
                options.s = atoi(optarg); break;
            case 'E': 
                options.E = atoi(optarg); break;
            case 'b': 
                options.b = atoi(optarg); break;
            case 't': 
                strcpy(options.filename, optarg); break;
            default: 
                success = 0; break;
        }
    }

    /* check validness of arguments */
    if (options.s < 0 || options.E <= 0 || options.b < 0) {
        success = 0;
    }

    return success;
}

void printUsage() {
    printf("Usage: ./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n");
    printf("Options:\n");
    printf("\t-h: Optional help flag that prints usage info\n");
    printf("\t-v: Optional verbose flag that displays trace info\n");
    printf("\t-s <s>: Number of set index bits (S = 2^s is the number of sets\n");
    printf("\t-E <E>: Associativity (number of lines per set)\n");
    printf("\t-b <b>: Number of block bits (B = 2^b is the block size)\n");
    printf("\t-t <tracefile>: Name of the valgrind trace to replay\n");
    printf("\n");
    printf("Examples:\n");
    printf("\tlinux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("\tlinux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

int main(int argc, char* argv[]) {
    if (!parseOptions(argc, argv) || options.h) {
        printUsage();
        exit(0);
    }

    FILE* fp;
    if (!(fp = fopen(options.filename, "r"))) {
        printf("Cannot open %s\n", options.filename);
        exit(-1);
    }

    if (!initCache(options.s, options.E, options.b)) {
        printf("Init cache with (%d, %d, %d) failure\n", options.s, options.E, options.b);
        cleanupCache();
        fclose(fp);
        exit(-1);
    }

    char op;
    Address address;
    int size;
    while (fscanf(fp, " %c %lx,%d", &op, &address, &size) > 0) {
        // printf("op = %c, address = %lx, size = %d\n", op, address, size);
        switch (op) {
            case 'L': /* data load */
                load(address, size, options.v);
                break;
            case 'S': /* data store */
                store(address, size, options.v);
                break;
            case 'M': /* data modify */
                modify(address, size, options.v);
                break;
            default: /* ignore instruction */
                break;
        }
    }

    cleanupCache();
    fclose(fp);

    printSummary(cache.statistics.hits, cache.statistics.misses, cache.statistics.evictions);

    return 0;
}