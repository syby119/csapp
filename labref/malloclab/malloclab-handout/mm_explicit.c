#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

// #define LIFO                /* Insertion policy: LIFO */
// #define ADDR_ORDER          /* Insertion policy: Address-ordered */


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


typedef unsigned int Word;
typedef Word* Pointer;

/* Basic constants and macros */
#define WSIZE       (sizeof(Word))    /* Word and header/footer size (bytes) */
#define DSIZE       (2 * WSIZE)       /* Double word size (bytes) */
#define CHUNKSIZE   4096              /* Extend heap by this amount (bytes) */

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

#define TRY_SET_PRED(bp, pred_bp) {                       \
    if(bp) {                                              \
        SET_PRED(bp, pred_bp);                            \
    }                                                     \
}

#define GET_SUCC(bp) (*(char**)(bp))

#define SET_SUCC(bp, succ_bp) {                           \
    *(char**)(bp) = (char*)(succ_bp);                     \
}

#define TRY_SET_SUCC(bp, succ_bp) {                       \
    if (bp) {                                             \
        SET_SUCC(bp, succ_bp);                            \
    }                                                     \
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

/* Global variables */
static char *free_listp;  /* Pointer to the first free block */

static void* extend_heap(size_t words);
static void* coalesce(void* bp);
static void* coalesce_aa(void* bp);
static void* coalesce_af(void* bp);
static void* coalesce_fa(void* bp);
static void* coalesce_ff(void* bp);
static void* find_fit(size_t asize);
static void place(void* bp, size_t asize);
static int check_block(void* bp, size_t prev_allocated);
static int check_block_list();
static int check_free_list();
static void print_block(void* bp);
int mm_check();

int mm_init(void) {
#ifdef DEBUG
    printf("mm_init()\n");
#endif
    free_listp = NULL;

    char* heap_listp;
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void*)(-1)) {
        return -1;
    }

    PUT(heap_listp, 0); /* padding for alignment */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 0, 1)); /* prologue header */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1, 1)); /* epilogue header */

    heap_listp += 2 * WSIZE;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
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

    size_t asize = (WSIZE + MAX(size, WSIZE + 2 * sizeof(Pointer)) + (DSIZE - 1)) / DSIZE * DSIZE;

    char* bp = find_fit(asize);
    if (bp == NULL) {
        size_t extend_size = MAX(CHUNKSIZE, asize);
        if ((bp = extend_heap(extend_size / WSIZE)) == NULL) {
            return NULL;
        }
    }

    place(bp, asize);

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

    char* header = HDRP(ptr);
    size_t block_size = GET_SIZE(header);
    size_t asize = (WSIZE + MAX(size, WSIZE + 2 * sizeof(Pointer)) + (DSIZE - 1)) / DSIZE * DSIZE;

    if (block_size >= asize) { /* current block can meet the requirement */
        size_t rsize = block_size - asize;
        if (rsize >= 2 * sizeof(Pointer) + DSIZE) {
            /* current block has additional space for a new free block */
            /* split the block */
            SET_SIZE(header, asize);
            /* set the remaining block free */
            void* next_bp = NEXT_BLKP(ptr);
            PUT(HDRP(next_bp), PACK(rsize, 1, 0));
            PUT(FTRP(next_bp), PACK(rsize, 1, 0));
            /* update the next block of the original block */
            next_bp = NEXT_BLKP(next_bp);
            SET_PREV_ALLOC(HDRP(next_bp), 0);
            if (!GET_ALLOC(HDRP(next_bp))) {
                COPY(FTRP(next_bp), HDRP(next_bp));
                coalesce(next_bp);
            }
        }
    } else { /* current block has no space */
        /* allocate new space for the block */
        void* new_ptr = mm_malloc(size);
        if (new_ptr == NULL) {
            return NULL;
        }
        /* copy data */
        memcpy(new_ptr, ptr, MIN(size, block_size - WSIZE));
        /* free the old block */
        SET_ALLOC(header, 0);
        COPY(FTRP(ptr), header);
        /* set prev_alloc bit of the next block */
        char* next_bp = NEXT_BLKP(ptr);
        header = HDRP(next_bp);
        SET_PREV_ALLOC(header, 0);
        if (!GET_ALLOC(header)) {
            COPY(FTRP(next_bp), header);
        }

        coalesce(ptr);
        /* reset return ptr */
        ptr = new_ptr;
    }

    CHECK_HEAP();

    return ptr;
}




int mm_check(void)
{
#ifdef VERBOSE
    printf("Heap (%p):\n", mem_heap_lo() + DSIZE);
    printf("freelistp: %p\n", free_listp);
#endif

    if (!check_block_list()) {
        return 0;
    }

    if (!check_free_list()) {
        return 0;
    }

    return 1;
}

static void* extend_heap(size_t words) {
    size_t size = ((words + 1) / 2) * 2 * WSIZE;
    void *bp;
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

    return coalesce(bp);
}

static void* coalesce(void* bp) {
    size_t prev_alloctated = GET_PREV_ALLOC(HDRP(bp));
    size_t next_allocated = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    if (prev_alloctated && next_allocated) {
        return coalesce_aa(bp);
    } else if (prev_alloctated && !next_allocated) {
        return coalesce_af(bp);
    } else if (!prev_alloctated && next_allocated) {
        return coalesce_fa(bp);
    } else {
        return coalesce_ff(bp);
    }
}

static void* find_fit(size_t asize) {
    for (char* bp = free_listp; bp != NULL; bp = GET_SUCC(bp)) {
        if (GET_SIZE(HDRP(bp)) >= asize) {
            return bp;
        }
    }

    return NULL;
}

static void place(void* bp, size_t asize) {
    void* header = HDRP(bp);
    void* pred_bp = GET_PRED(bp);
    void* succ_bp = GET_SUCC(bp);
    size_t size = GET_SIZE(header);
    size_t rsize = size - asize;

    if (rsize >= DSIZE + 2 * sizeof(Pointer)) {
        PUT(header, PACK(asize, GET_PREV_ALLOC(header), 1));
        /* maintain block list, won't overwrite pred/succ info */
        bp = NEXT_BLKP(bp);
        header = HDRP(bp);
        PUT(header, PACK(rsize, 1, 0));
        COPY(FTRP(bp), header);
        /* maintain free list, bp is now a free block */
#ifdef LIFO
        if (pred_bp == NULL) {
            TRY_SET_PRED(succ_bp, bp);
            SET_PRED(bp, NULL);
            SET_SUCC(bp, succ_bp);
        } else {
            SET_SUCC(pred_bp, succ_bp);
            TRY_SET_PRED(succ_bp, pred_bp);
            SET_PRED(bp, NULL);
            SET_SUCC(bp, free_listp);
            SET_PRED(free_listp, bp);
        }

        free_listp = bp;
#else
        SET_PRED(bp, pred_bp);
        if (pred_bp) {
            SET_SUCC(pred_bp, bp);
        } else {
            free_listp = bp;
        }

        SET_SUCC(bp, succ_bp);
        if (succ_bp) {
            SET_PRED(succ_bp, bp);
        }
#endif
    } else {
        SET_ALLOC(header, 1);
        /* maintain block list */
        bp = NEXT_BLKP(bp);
        header = HDRP(bp);
        SET_PREV_ALLOC(header, 1);
        /* maintain free list */
        if (pred_bp) {
            SET_SUCC(pred_bp, succ_bp);
        } else {
            free_listp = succ_bp;
        }

        TRY_SET_PRED(succ_bp, pred_bp);
    }
}

static int check_block(void* bp, size_t prev_allocated) {
    /* check alignment */
    if (((size_t)bp) % DSIZE != 0) {
#ifdef VERBOSE
        printf("Error: %p is not double word aligned\n", bp);
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
        if (bp == mem_heap_lo() + DSIZE) {
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
        } else if (size != DSIZE){
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

    return 1;
}

static int check_block_list() {
    char *bp = mem_heap_lo() + DSIZE;
    char *header = HDRP(bp);
    size_t prev_allocated = GET_PREV_ALLOC(header);
    int free_list_count = 0;

    for (; GET_SIZE(header) > 0; header = HDRP(bp)) {
#ifdef VERBOSE
        print_block(bp);
#endif

        if (!check_block(bp, prev_allocated)) {
            return 0;
        }

        if (!GET_ALLOC(header)) {
            char* pred_bp = GET_PRED(bp);
            if (pred_bp == NULL) {
                if (++free_list_count == 2) {
#ifdef VERBOSE
                    printf("double free list detected\n");
#endif 
                    return 0;
                }
            } else if (GET_ALLOC(HDRP(pred_bp))){
#ifdef VERBOSE
                    printf("free block pred is allocated\n");
                    return 0;
#endif 
            }
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

static int check_free_list() {
    char* pred = NULL;
    for (char* p = free_listp; p; p = GET_SUCC(p)) {
        if (GET_ALLOC(p)) {
#ifdef VERBOSE
            printf("Block in free list is allocated\n");
#endif
            return 0;
        }

        if (GET_PRED(p) != pred) {
#ifdef VERBOSE
            printf("Pred pointer of %p inconsistent: pred = %p, GET_PRED(p) = %p\n", p, pred, GET_PRED(p));
            printf("alloc: %d\n", GET_ALLOC(HDRP(p)));
#endif
            return 0;
        }

        pred = p;
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

static void* coalesce_aa(void* bp) {
    // maintain free list
#ifdef LIFO
    if (free_listp) {
        SET_PRED(free_listp, bp);
        SET_PRED(bp, NULL);
        SET_SUCC(bp, free_listp);
    } else {
        SET_PRED(bp, NULL);
        SET_SUCC(bp, NULL);
    }

    return free_listp = bp;
#else
#error "Not Implemented"
#endif
}

static void* coalesce_af(void* bp) {
    char* header = HDRP(bp);
    char* next_bp = NEXT_BLKP(bp);
    size_t size = GET_SIZE(header) + GET_SIZE(HDRP(next_bp));

    // maintain block list
    SET_SIZE(header, size);
    COPY(FTRP(bp), header);

    // maintain free list
#ifdef LIFO
    char* pred_next_bp = GET_PRED(next_bp);
    char* succ_next_bp = GET_SUCC(next_bp);

    if (free_listp == next_bp) {
        SET_PRED(bp, NULL);
        SET_SUCC(bp, succ_next_bp);
        TRY_SET_PRED(succ_next_bp, bp);
    } else {
        SET_SUCC(pred_next_bp, succ_next_bp);
        TRY_SET_PRED(succ_next_bp, pred_next_bp);
        SET_PRED(free_listp, bp);
        SET_PRED(bp, NULL);
        SET_SUCC(bp, free_listp);
    }

    return free_listp = bp;
#else
#error "Not Implemented"
#endif
}

static void* coalesce_fa(void* bp) {
    char* prev_bp = PREV_BLKP(bp);
    char* prev_header = HDRP(prev_bp);
    size_t size = GET_SIZE(HDRP(bp)) + GET_SIZE(prev_header);

    // maintain block list
    SET_SIZE(prev_header, size);
    COPY(FTRP(prev_bp), prev_header);

    // maintain free list
#ifdef LIFO
    if (free_listp == prev_bp) {
        /* do nothing */
    } else {
        char* pred_prev_bp = GET_PRED(prev_bp);
        char* succ_prev_bp = GET_SUCC(prev_bp);
        
        SET_SUCC(pred_prev_bp, succ_prev_bp);
        TRY_SET_PRED(succ_prev_bp, pred_prev_bp);

        SET_PRED(free_listp, prev_bp);
        SET_PRED(prev_bp, NULL);
        SET_SUCC(prev_bp, free_listp);

        free_listp = prev_bp;
    }

    return prev_bp;
#else
#error "Not Implemented"
#endif
}

static void* coalesce_ff(void* bp) {
    char* prev_bp = PREV_BLKP(bp);
    char* next_bp = NEXT_BLKP(bp);
    char* prev_header = HDRP(prev_bp);
    size_t size = GET_SIZE(HDRP(bp)) + GET_SIZE(prev_header) + GET_SIZE(HDRP(next_bp));

    // 1. maintain block list
    SET_SIZE(prev_header, size);
    COPY(FTRP(prev_bp), prev_header);

    // 2. maintain free list
#ifdef LIFO
    char* pred_bp, *succ_bp;
    // 2.1 remove prev_bp from list
    pred_bp = GET_PRED(prev_bp);
    succ_bp = GET_SUCC(prev_bp);
    if (pred_bp == NULL) {
        free_listp = succ_bp;
        TRY_SET_PRED(succ_bp, NULL);
    } else {
        SET_SUCC(pred_bp, succ_bp);
        TRY_SET_PRED(succ_bp, pred_bp);
    }
    // 2.2 remove next_bp from list
    pred_bp = GET_PRED(next_bp);
    succ_bp = GET_SUCC(next_bp);
    if (pred_bp == NULL) {
        free_listp = succ_bp;
        TRY_SET_PRED(succ_bp, NULL);
    } else {
        SET_SUCC(pred_bp, succ_bp);
        TRY_SET_PRED(succ_bp, pred_bp);
    }
    // 2.3 insert prev_bp into the head of the free list
    SET_PRED(prev_bp, NULL);
    SET_SUCC(prev_bp, free_listp);
    TRY_SET_PRED(free_listp, prev_bp);

    return free_listp = prev_bp;
#else
#error "Not Implemented"
#endif
}