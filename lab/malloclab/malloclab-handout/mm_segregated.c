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
    ""
};


// #define FIRST_INSERT      /* Insertion policy: insert the free block at the list header */
// #define ASCENDING_INSERT  /* Insertion policy: the size of the block in the list is in ascending order */
// #define SHRINK_BLOCK      /* Shrink block when realloc if possible */

typedef unsigned int Word;
typedef Word* Pointer;

#define WSIZE           (sizeof(Word))    /* Word and header/footer size (bytes) */
#define DSIZE           (2 * WSIZE)       /* Double word size (bytes) */

#define ALIGNMENT DSIZE                   /* Alignment of the allocated block */
#define ALIGN(size) ((((size) + ALIGNMENT - 1) / ALIGNMENT) * ALIGNMENT)

#define CHUNKSIZE       4096              /* Extend heap by this amount (bytes) */
#define INIT_CHUNKSIZE  64                /* Extend heap by this amount (bytes) when init */

#define LISTCOUNT 16                      /* Number of segregated lists */

#define BIGSIZE   96                      /* Threshold for spliting block */

#define MAX(x, y) ((x) > (y)? (x) : (y)) 
#define MIN(x, y) ((x) < (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, prev_alloc, alloc)  ((size) | (prev_alloc << 1) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(Word *)(p))
#define PUT(p, val)  (*(Word *)(p) = (val))
#define COPY(dst, src) (*(Word *)(dst) = *(Word *)(src))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~(Word)0x7)               /* Size of the block */
#define GET_PREV_ALLOC(p)  ((GET(p) & (Word)0x2) >> 1)   /* If the previous block is allocated */
#define GET_ALLOC(p) (GET(p) & (Word)0x1)                /* If the block is allocated */

/* Write the size, previous allocated and allocated fields to address p */
#define SET_SIZE(p, size) {                               \
    *(Word *)(p) = size | (*(Word *)(p) & (Word)3);       \
}

#define SET_PREV_ALLOC(p, prev_alloc) {                   \
    *(Word *)(p) = (prev_alloc ?                          \
        *(Word *)(p) | (Word)2 :                          \
        *(Word *)(p) & ~(Word)2);                         \
}

#define SET_ALLOC(p, alloc) {                             \
    *(Word *)(p) = (alloc ?                               \
        (*(Word *)(p) | (Word)1) :                        \
        (*(Word *)(p) & ~(Word)1));                       \
}

/* Given a block ptr bp, compute its header address */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
/* Given a free block ptr bp, compute its footer */
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given a block ptr bp, compute address of the next block */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))
/* Given a free block ptr bp, compute address of the previous block */
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))


/* Given a free block ptr bp, get or set its pred / succ fields */
#define GET_PRED(bp) (*((char**)(bp) + 1))

#define SET_PRED(bp, pred_bp) {                           \
    *(char**)((char**)(bp) + 1) = (char*)(pred_bp);       \
}

#define GET_SUCC(bp) (*(char**)(bp))

#define SET_SUCC(bp, succ_bp) {                           \
    *(char**)(bp) = (char*)(succ_bp);                     \
}

#ifdef DEBUG
#define CHECK_HEAP() {                                    \
    if (!mm_check()) {                                    \
        printf("check = 0\n\n");                          \
        exit(-1);                                         \
    } else {                                              \
        printf("check = 1\n\n");                          \
    }                                                     \
}
#else
#define CHECK_HEAP() {}
#endif

void** segregated_lists = NULL;

/* Helper functions */
static void* extend_heap(size_t words);
static void* find_fit(size_t asize);
static void* place(void* bp, size_t asize);
static void shrink(void* bp, size_t asize, size_t rsize);
static void* coalesce(void* bp);
static void insert_block(void* bp);
static void remove_block(void* bp);
static int find_list(size_t payload);

/* For debug usage */
static size_t get_min_capacity(int index);
static size_t get_max_capacity(int index);
static void* get_prologue_block();
static int check_block(void* bp, size_t prev_allocated);
static void print_block(void* bp);
static int check_block_list();
static int check_segregated_list(int index);
static int mm_check();

int mm_init(void) {
#ifdef DEBUG
    printf("\n\n\n##################################################\n\n\n");
    printf("mm_init()\n");
#endif
    char* address = NULL;
    int size = LISTCOUNT * sizeof(Pointer);
    int aligned = !!(size % ALIGNMENT);

    /* init segregated_lists */
    if ((address = mem_sbrk(size)) == (void*)(-1)) {
        return -1;
    }

    segregated_lists = (void**)address;
    for (int i = 0; i < LISTCOUNT; ++i) {
        segregated_lists[i] = NULL;
    }

    /* init prologue / epilogue block */
    size = (aligned ? 3 : 4) * WSIZE;
    if ((address = mem_sbrk(size)) == (void*)(-1)) {
        return -1;
    }

    if (!aligned) {
        PUT(address, 0);                           /* alignment padding */
    }
    PUT(address + (1 * WSIZE), PACK(DSIZE, 0, 1)); /* prologue header */
    PUT(address + (3 * WSIZE), PACK(0, 1, 1));     /* epilogue header */

    /* extend heap */
    if (!extend_heap(INIT_CHUNKSIZE / WSIZE)) {
        return -1;
    }

    CHECK_HEAP();

    return 0;
}

void *mm_malloc(size_t size) {
#ifdef DEBUG
    printf("mm_malloc(%d)\n", size);
#endif

    if (size == 0) {
        return NULL;
    }

    size_t asize = ALIGN(WSIZE + MAX(size, WSIZE + 2 * sizeof(Pointer)));

    void* bp = find_fit(asize);
#ifdef VERBOSE
    printf("find fit bp = %p\n", bp);
#endif

    if (bp == NULL) {
        if ((bp = extend_heap(MAX(CHUNKSIZE, asize) / WSIZE)) == NULL) {
            return NULL;
        }
    }

    bp = place(bp, asize);

    CHECK_HEAP();

    return bp;
}

void mm_free(void *ptr) {
#ifdef DEBUG
    printf("mm_free(%p)\n", ptr);
#endif

    if (ptr == NULL) {
        return;
    }

    /* free current block */
    char* header = HDRP(ptr);
    SET_ALLOC(header, 0);
    COPY(FTRP(ptr), header);

    /* set the prev_alloc bit of the next block */
    char* next_bp = NEXT_BLKP(ptr);
    header = HDRP(next_bp);
    SET_PREV_ALLOC(header, 0);
    if (!GET_ALLOC(header)) {
        SET_PREV_ALLOC(FTRP(next_bp), 0);
    }

    /* insert block to the segregated free list */
    insert_block(ptr);

    /* coalesce the block if possible */
    coalesce(ptr);

    CHECK_HEAP();
}

void *mm_realloc(void *ptr, size_t size) {
#ifdef DEBUG
    printf("mm_realloc(%p, %d)\n", ptr, size);
#endif

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    if (ptr == NULL) {
        return mm_malloc(size);
    }

    size_t asize = ALIGN(WSIZE + MAX(size, WSIZE + 2 * sizeof(Pointer)));
    size_t bsize = GET_SIZE(HDRP(ptr));

    if (bsize >= asize) { /* current block is big enough */
#ifdef SHRINK_BLOCK
        size_t rsize = bsize - asize;
        if (rsize >= 2 * sizeof(Pointer) + DSIZE) {
            shrink(ptr, asize, rsize);
        }
#endif
    } else { /* current block is not big enough */
        int finish = 0;

        /* try to utilize the next block if possible */
        void* next_ptr = NEXT_BLKP(ptr);
        if (!GET_ALLOC(HDRP(next_ptr))) {
            size_t nsize = GET_SIZE(HDRP(next_ptr));
            /* next block is a free block and merging two block is big enough */
            if (bsize + nsize >= asize) {
                /* remove next_ptr from segregated list */
                remove_block(next_ptr);

                bsize += nsize;
#ifdef SHRINK_BLOCK
                size_t rsize = bsize - asize;
                if (rsize < 2 * sizeof(Pointer) + DSIZE) {
                    /* set size of the block */
                    SET_SIZE(HDRP(ptr), bsize);
                    /* set the prev_allocated field of the next block (must be allocated) */
                    SET_PREV_ALLOC(HDRP(NEXT_BLKP(ptr)), 1);
                } else {
                    shrink(ptr, asize, rsize);
                }
#else
                SET_SIZE(HDRP(ptr), bsize);
                /* set the prev_allocated field of the next block (must be allocated) */
                SET_PREV_ALLOC(HDRP(NEXT_BLKP(ptr)), 1);
#endif
                
                finish = 1;
            }
        }
        
        /* find fit in the segregated list to avoid extending heap */
        if (!finish) {
            void* new_ptr = find_fit(asize);
            if (new_ptr != NULL) {
                new_ptr = place(new_ptr, asize);
                memcpy(new_ptr, ptr, bsize - WSIZE);
                
                mm_free(ptr);
                ptr = new_ptr;

                finish = 1;
            }
        }

        /* extend_heap can directly solve the problem */
        if (!finish) {
            int direct_extendable = 0;
            size_t nsize = GET_SIZE(HDRP(next_ptr));
            if (!nsize) { /* epilogue header */
                direct_extendable = 1;
            } else if (!GET_ALLOC(HDRP(next_ptr)) && !GET_SIZE(HDRP(NEXT_BLKP(next_ptr)))) {
                direct_extendable = 1;
            }

            if (direct_extendable) {
                size_t words = MAX(CHUNKSIZE, asize - nsize - bsize) / WSIZE;
                if (mem_sbrk(((words + 1) / 2) * 2 * WSIZE) == (void*)-1) {
                    return NULL;
                }

                if (nsize) {
                    remove_block(next_ptr);
                }

                /* set epilogue header */
                void* epilogue_bp = mem_heap_hi() + 1;
                bsize = (char*)epilogue_bp - (char*)ptr;
#ifdef SHRINK_BLOCK
                if (bsize < asize + DSIZE + 2 * sizeof(Pointer)) {
                    PUT(HDRP(epilogue_bp), PACK(0, 1, 1));
                    SET_SIZE(HDRP(ptr), bsize);
                } else {
                    PUT(HDRP(epilogue_bp), PACK(0, 0, 1));
                    shrink(ptr, asize, bsize - asize);
                }
#else
                PUT(HDRP(epilogue_bp), PACK(0, 1, 1));
                SET_SIZE(HDRP(ptr), bsize);
#endif

                finish = 1;
            }
        }

        /* allocate new space for the block */
        if (!finish) {
            void* new_ptr = extend_heap(MAX(CHUNKSIZE, asize) / WSIZE);
            if (new_ptr == NULL) {
                return NULL;
            }

            new_ptr = place(new_ptr, asize);
            memcpy(new_ptr, ptr, bsize - WSIZE);
            mm_free(ptr);

            ptr = new_ptr;
        }
    }

    CHECK_HEAP();

    return ptr;
}

static void* extend_heap(size_t words) {
    size_t size = ((words + 1) / 2) * 2 * WSIZE;
    void* bp;
    if ((bp = mem_sbrk(size)) == (void*)-1) {
        return NULL;
    }

    /* now bp is at the previous position of epilogue block */
    char* header = HDRP(bp);
    size_t prev_allocated = GET_PREV_ALLOC(header);
    PUT(header, PACK(size, prev_allocated, 0));
    COPY(FTRP(bp), header);
    /* new epilogue header */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 0, 1));

    /* insert the new free block to the list */
    insert_block(bp);
    
    return coalesce(bp);
}

static void* find_fit(size_t asize) {
    /* segregated fit, search the segregated lists from small to large size class */
    for (int i = find_list(asize - WSIZE); i < LISTCOUNT; ++i) {
        /* find the first free block in the list that can fit the given size */
        for (void* bp = segregated_lists[i]; bp; bp = GET_SUCC(bp)) {
            if (GET_SIZE(HDRP(bp)) >= asize) {
                return bp;
            }
        }
    }

    return NULL;
}

static void* place(void* bp, size_t asize) {
    size_t rsize = GET_SIZE(HDRP(bp)) - asize;
    char* next_bp = NEXT_BLKP(bp);

    remove_block(bp);

    if (rsize < DSIZE + 2 * sizeof(Pointer)) {
        /* the remaining size is not enough for a new block */
        /* 1. set alloc field for bp */
        SET_ALLOC(HDRP(bp), 1);
        /* 2. set the prev_allocated field for the next block (must be allocated) */
        SET_PREV_ALLOC(HDRP(next_bp), 1);

        return bp;
    } else {
        /* split the block */
        if (asize >= BIGSIZE) {
            /* place the allocated block at high address */
            /* 1. set the prev_allocated field for the next block (must be allocated) */
            SET_PREV_ALLOC(HDRP(next_bp), 1);
            /* 2. set the size field for the free block */
            SET_SIZE(HDRP(bp), rsize);
            COPY(FTRP(bp), HDRP(bp));
            /* 3. set meta info of the allocated block */
            next_bp = NEXT_BLKP(bp);
            PUT(HDRP(next_bp), PACK(asize, 0, 1));
            /* 4. insert the free block to the list */
            insert_block(bp);

            return next_bp;
        } else {
            /* place the allocated block at low address */
            /* 1. set the prev_allocated field for the next block (must be allocated) */
            SET_PREV_ALLOC(HDRP(next_bp), 0);
            /* 2. set the size field for the allocated block */
            SET_SIZE(HDRP(bp), asize);
            SET_ALLOC(HDRP(bp), 1);
            /* 3. set meta info of the free block */
            next_bp = NEXT_BLKP(bp);
            PUT(HDRP(next_bp), PACK(rsize, 1, 0));
            COPY(FTRP(next_bp), HDRP(next_bp));
            /* 4. insert the free block to the list */
            insert_block(next_bp);

            return bp;
        }
    }
}

static void* coalesce(void* bp) {
    char* next_bp = NEXT_BLKP(bp);
    size_t prev_allocated = GET_PREV_ALLOC(HDRP(bp));
    size_t next_allocated = GET_ALLOC(HDRP(next_bp));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_allocated && next_allocated) {
        return bp;
    } else if (prev_allocated && !next_allocated) {
        remove_block(bp);
        remove_block(next_bp);
        size += GET_SIZE(HDRP(next_bp));

        SET_SIZE(HDRP(bp), size);
        COPY(FTRP(bp), HDRP(bp));
    } else if (!prev_allocated && next_allocated) {
        char* prev_bp = PREV_BLKP(bp);
        remove_block(prev_bp);
        remove_block(bp);
        size += GET_SIZE(HDRP(prev_bp));

        SET_SIZE(HDRP(prev_bp), size);
        COPY(FTRP(prev_bp), HDRP(prev_bp));
        bp = prev_bp;
    } else {
        char* prev_bp = PREV_BLKP(bp);
        remove_block(prev_bp);
        remove_block(bp);
        remove_block(next_bp);
        size += GET_SIZE(HDRP(prev_bp)) + GET_SIZE(HDRP(next_bp));
        
        SET_SIZE(HDRP(prev_bp), size);
        COPY(FTRP(prev_bp), HDRP(prev_bp));
        bp = prev_bp;
    }

    insert_block(bp);

    return bp;
}

static void shrink(void* bp, size_t asize, size_t rsize) {
    /* split the block */
    SET_SIZE(HDRP(bp), asize);

    /* set the remaining block free */
    char* next_bp = NEXT_BLKP(bp);
    PUT(HDRP(next_bp), PACK(rsize, 1, 0));
    COPY(FTRP(next_bp), HDRP(next_bp));

    insert_block(next_bp);

    /* update the next block of the original block */
    next_bp = NEXT_BLKP(next_bp);
    SET_PREV_ALLOC(HDRP(next_bp), 0);

    if (!GET_ALLOC(HDRP(next_bp))) {
        COPY(FTRP(next_bp), HDRP(next_bp));
        coalesce(next_bp);
    }
}

static void insert_block(void* bp) {
    /* find the coresponding list */
    size_t size = GET_SIZE(HDRP(bp));
    int index = find_list(size - WSIZE);

#ifdef ASCENDING_INSERT
    /* find the position of the block to be inserted behind */
    /* to keep the size of the free block in the list in ascending order */
    char* insert_bp = NULL;
    char* search_bp = segregated_lists[index];
    for (; search_bp; search_bp = GET_SUCC(search_bp)) {
        if (GET_SIZE(HDRP(search_bp)) >= size) {
            break;
        }
        insert_bp = search_bp;
    }
#endif

    /* insert the block into the list */
#ifdef ASCENDING_INSERT
    if (search_bp != NULL) {
        if (insert_bp != NULL) {
            /* segregated_lists[index] <-> xxx <-> insert_bp <-> bp <-> search_bp */
            SET_PRED(bp, insert_bp);
            SET_SUCC(bp, search_bp);
            SET_SUCC(insert_bp, bp);
            SET_PRED(search_bp, bp);
        } else {
            /* segregated_lists[index] <-> search_bp */
            SET_PRED(bp, NULL);
            SET_SUCC(bp, search_bp);
            SET_PRED(search_bp, bp);
            segregated_lists[index] = bp;
        }
    } else {
        if (insert_bp != NULL) {
            /* segregated_lists[index] <-> xxx <-> insert_bp <-> NULL */
            SET_PRED(bp, insert_bp);
            SET_SUCC(bp, NULL);
            SET_SUCC(insert_bp, bp);
        } else {
            /* segregated_lists[index] -> NULL */
            SET_PRED(bp, NULL);
            SET_SUCC(bp, NULL);
            segregated_lists[index] = bp;
        }
    }
#else
    char* insert_bp = segregated_lists[index];
    if (insert_bp != NULL) {
        SET_PRED(bp, NULL);
        SET_SUCC(bp, insert_bp);
        SET_PRED(insert_bp, bp);
    } else {
        SET_PRED(bp, NULL);
        SET_SUCC(bp, NULL);
    }

    segregated_lists[index] = bp;
#endif
}

static void remove_block(void* bp) {
    size_t size = GET_SIZE(HDRP(bp));
    int index = find_list(size - WSIZE);
    char* pred_bp = GET_PRED(bp);
    char* succ_bp = GET_SUCC(bp);

    if (pred_bp != NULL) {
        if (succ_bp != NULL) {
            SET_PRED(succ_bp, pred_bp);
            SET_SUCC(pred_bp, succ_bp);
        } else {
            SET_SUCC(pred_bp, NULL);
        }
    } else {
        if (succ_bp != NULL) {
            SET_PRED(succ_bp, NULL);
            segregated_lists[index] = succ_bp;
        } else {
            segregated_lists[index] = NULL;
        }
    }
}

static int find_list(size_t payload) {
    int index = 1;
    size_t max_capacity = 2 * sizeof(Pointer);
    while (payload > max_capacity && index < LISTCOUNT) {
        max_capacity <<= 1;
        index += 1;
    }

    return index - 1;
}

static size_t get_min_capacity(int index) {
    size_t max_capacity = 2 * sizeof(Pointer);
    for (int i = 0; i < index; ++i) {
        max_capacity <<= 1;
    }

    if (index == 0) {
        return 1;
    } else {
        return max_capacity / 2 + 1;
    }
}

static size_t get_max_capacity(int index) {
    size_t max_capacity = 2 * sizeof(Pointer);
    for (int i = 0; i < index; ++i) {
        max_capacity <<= 1;
    }

    if (index == LISTCOUNT - 1) {
        return 0;
    } else {
        return max_capacity;
    }
}

static void* get_prologue_block() {
    return mem_heap_lo() + LISTCOUNT * sizeof(Pointer)
           + (LISTCOUNT % 2 ? 1 : 2) * WSIZE;
}

static int check_block(void* bp, size_t prev_allocated) {
    /* check alignment */
    if (((size_t)bp) % ALIGNMENT != 0) {
#ifdef VERBOSE
        printf("Error: %p is not %d bytes aligned\n", bp, ALIGNMENT);
#endif
        return 0;
    }

    /* check header and footer consistency */
    void* header = HDRP(bp);
    if (!GET_ALLOC(header)) {
        if (GET(header) != GET(FTRP(bp))) {
#ifdef VERBOSE
            printf("Error: header does not match footer\n");
#endif
            return 0;
        }
    }

    /* check size */
    size_t size = GET_SIZE(header);
    if (size % DSIZE != 0) {
#ifdef VERBOSE
        printf("Error: invalid size %d\n", size);
#endif 
        return 0;
    }

    if (size < 2 * DSIZE) {
        if (bp == get_prologue_block()) {
            if (size != DSIZE || GET_PREV_ALLOC(header) || !GET_ALLOC(header)) {
#ifdef VERBOSE
                printf("Error: Bad prelogue block\n");
#endif
                return 0;
            }
        } else if (size == 0) {
            if (!GET_ALLOC(header)) {
#ifdef VERBOSE
                printf("Error: Bad epilogue block\n");
#endif
                return 0;
            }
        } else if (size != DSIZE) {
#ifdef VERBOSE
            printf("Error: Invalid block size\n");
#endif
            return 0;
        }
    }

    /* check prev_allocated field consistency */
    if (prev_allocated != GET_PREV_ALLOC(header)) {
#ifdef VERBOSE
        printf("Error: Prev block allocated inconsistent\n");
#endif
        return 0;
    }

    /* check in the segregated list for free block */
    if (!GET_ALLOC(header)) {
        int index = find_list(GET_SIZE(header) - WSIZE);
        void* ptr = segregated_lists[index];
        while ((ptr != NULL) && (ptr != bp)) {
            ptr = GET_SUCC(ptr);
        }

        if (ptr == NULL) {
#ifdef VERBOSE
            printf("Error: free block %p is not in the segregated_list[%d]: %p\n",
                    bp, index, segregated_lists[index]);
#endif
            return 0;
        }
    }

    return 1;
}

static void print_block(void* bp) {
    char* header = HDRP(bp);
    size_t header_size = GET_SIZE(header);
    char header_allocated = GET_ALLOC(header) ? 'a' : 'f';
    char prev_allocated = GET_PREV_ALLOC(header) ? 'a' : 'f';

    if (header_size == 0) {
        printf("%p: EPILOGUE(0, %c, %c)\n", bp, prev_allocated, header_allocated);
        return ;
    }

    if (header_allocated == 'f') {
        char* footer = FTRP(bp);
        size_t footer_size = GET_SIZE(footer);
        char footer_allocated = GET_ALLOC(footer) ? 'a' : 'f';
        char* pred_bp = GET_PRED(bp);
        char* succ_bp = GET_SUCC(bp);
        printf("%p: header: [%d:%c:%c], footer: [%d:%c], prev: %p, succ: %p\n", 
            bp, header_size, 
            prev_allocated, header_allocated,
            footer_size, footer_allocated,
            pred_bp, succ_bp);
    } else {
        printf("%p: header: [%d:%c:%c]\n", bp, 
               header_size, prev_allocated, header_allocated);
    }
}

static int check_block_list() {
    char* bp = get_prologue_block();
    char* header = HDRP(bp);
    size_t prev_allocated = GET_PREV_ALLOC(header);

    for (; GET_SIZE(header) > 0; header = HDRP(bp)) {
#ifdef VERBOSE
        print_block(bp);
#endif

        if (!check_block(bp, prev_allocated)) {
            return 0;
        }

        prev_allocated = GET_ALLOC(header);
        bp = NEXT_BLKP(bp);
    }

#ifdef VERBOSE
    print_block(bp);
#endif

    if (!check_block(bp, prev_allocated)) {
        return 0;
    }

    return 1;
}

static int check_segregated_list(int index) {
#ifdef VERBOSE
    printf("+ lists[%2d] ", index);
    printf("(%6d, ", get_min_capacity(index));
    if (index == LISTCOUNT - 1) {
        printf("   inf): %p\n", segregated_lists[index]);
    } else {
        printf("%6d): %p\n", get_max_capacity(index), segregated_lists[index]);
    }
#endif

    const size_t min_size = get_min_capacity(index) + WSIZE;
    const size_t max_size = (index < LISTCOUNT - 1) ? 
                            (get_max_capacity(index) + WSIZE) : ((size_t)-1);

    if (segregated_lists[index] == NULL) {
        return 1;
    }

    char* pred_bp = NULL;
    for (char* bp = segregated_lists[index]; bp; bp = GET_SUCC(bp)) {
#ifdef VERBOSE
        print_block(bp);
#endif

        if (GET_ALLOC(bp)) {
#ifdef VERBOSE
            printf("Block in free list is allocated\n");
#endif
            return 0;
        }

        if (GET_PRED(bp) != pred_bp) {
#ifdef VERBOSE
            printf("Pred pointer of %p inconsistent: pred = %p, GET_PRED(p) = %p\n",
                    bp, pred_bp, GET_PRED(bp));
            printf("alloc: %d\n", GET_ALLOC(HDRP(bp)));
#endif
            return 0;
        }

        size_t size = GET_SIZE(HDRP(bp));
        if (size < min_size || size > max_size) {
#ifdef VERBOSE
            printf("Illegal block size %d, expected [%d, %d]\n", size, min_size, max_size);
#endif
            return 0;
        }

        pred_bp = bp;
    }

    return 1;
}

static int mm_check() {
#ifdef VERBOSE
    printf("Heap Memory Low (%p): \n", mem_heap_lo());
#endif

#ifdef VERBOSE
    printf("Segregated Lists:\n");
#endif

    for (int i = 0; i < LISTCOUNT; ++i) {
        if (!check_segregated_list(i)) {
            return 0;
        }
    }

#ifdef VERBOSE
    printf("Block List:\n");
#endif

    if (!check_block_list()) {
        return 0;
    }

    return 1;
}