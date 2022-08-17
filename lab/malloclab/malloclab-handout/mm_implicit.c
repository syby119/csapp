#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

// #define FIRST_FIT           /* Placement policy: first fit */
// #define NEXT_FIT            /* Placement policy: next fit */

// #define REFIT_KEEP   /* Realloc policy: keep the data at its orignal position if possible */
// #define REFIT_MAX_UTIL   /* Realloc policy: move the data to coalesce free block if possible */


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


typedef unsigned int WORD;

/* Basic constants and macros */
#define WSIZE       (sizeof(WORD))    /* Word and header/footer size (bytes) */
#define DSIZE       (2 * WSIZE)       /* Double word size (bytes) */
#define CHUNKSIZE   1024              /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))
#define MIN(x, y) ((x) < (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, prev_alloc, alloc)  ((size) | (prev_alloc << 1) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(WORD *)(p))
#define PUT(p, val)  (*(WORD *)(p) = (val))
#define COPY(dst, src) (*(WORD *)(dst) = *(WORD *)(src))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~(WORD)0x7)              /* Size of the block */
#define GET_PREV_ALLOC(p)  ((GET(p) & (WORD)0x2) >> 1)  /* If the previous block is allocated */
#define GET_ALLOC(p) (GET(p) & (WORD)0x1)               /* If the block is allocated */

/* Write the size, previous allocated and allocated fields to address p */
#define SET_SIZE(p, size) {                               \
    *(WORD *)(p) = size | (*(WORD *)(p) & (WORD)3);       \
}

#define SET_PREV_ALLOC(p, prev_alloc) {                   \
    *(WORD *)(p) = (prev_alloc ?                          \
        *(WORD *)(p) | (WORD)2 :                          \
        *(WORD *)(p) & ~(WORD)2);                         \
}

#define SET_ALLOC(p, alloc) {                             \
    *(WORD *)(p) = (alloc ?                               \
        (*(WORD *)(p) | (WORD)1) :                        \
        (*(WORD *)(p) & ~(WORD)1));                       \
}

/* Given a block ptr bp, compute the address of its header */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
/* Given a free block ptr bp, compute the address of its footer */
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given a block ptr bp, compute the address of the next block */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))
/* Given a free block ptr bp, compute the address of the previous block */
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

/* Check the consistency of the heap when debugging */
#ifdef DEBUG
#define CHECK_HEAP() {                                   \
    if (!mm_check()) {                                   \
        printf("check = 0\n\n");                         \
        exit(-1);                                        \
    } else {                                             \
        printf("check = 1\n\n");                         \
    }                                                    \
}
#else
#define CHECK_HEAP() {}
#endif

/* Global variables */
static char *heap_listp; /* Pointer to first block */

#ifdef NEXT_FIT
static char *rover;      /* Next fit rover */
#endif

#ifdef DEBUG
char mm_check_buf[2 << 20]; /* data buffer for mm_realloc checking */
char* mm_check_ptr; /* new block ptr for mm_realloc checking */
size_t mm_check_len; /* data length for mm_realloc checking */
#endif

static void* extend_heap(size_t words);

static void* coalesce(void* bp);
static void* coalesce_aa(void* bp);
static void* coalesce_af(void* bp);
static void* coalesce_fa(void* bp);
static void* coalesce_ff(void* bp);

static void* coalesce_r(void* bp);
static void* coalesce_aa_r(void* bp);
static void* coalesce_af_r(void* bp);
static void* coalesce_fa_r(void* bp);
static void* coalesce_ff_r(void* bp);

static void* find_fit(size_t asize);
static void place(void* bp, size_t asize);
static void place_r(void* bp, size_t asize);

static void* refit(void* bp, size_t asize);

static int checkblock(void* bp, size_t prev_allocated);
static void printblock(void* bp);
static int mm_check();

int mm_init(void) {
#ifdef DEBUG
    printf("mm_init()\n");
    mm_check_len = 0;
    mm_check_ptr = NULL;
#endif

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

#ifdef NEXT_FIT
    rover = heap_listp + 2 * WSIZE;
#endif

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

    size_t asize = (size + WSIZE + (DSIZE - 1)) / DSIZE * DSIZE;

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

    if (ptr != NULL) {
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
    }

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

#ifdef DEBUG
    size_t len = MIN(GET_SIZE(HDRP(ptr)) - WSIZE, size);
    memcpy(mm_check_buf, ptr, len);
#endif

    char* new_ptr;
    size_t asize = (size + WSIZE + (DSIZE - 1)) / DSIZE * DSIZE;
    if (!(new_ptr = refit(ptr, asize))) {
        if (!(new_ptr = find_fit(asize))) {
            size_t extend_size = MAX(CHUNKSIZE, asize);
            if (!(new_ptr = extend_heap(extend_size / WSIZE))) {
                return NULL;
            }
        }

        place(new_ptr, asize);
        memcpy(new_ptr, ptr, GET_SIZE(HDRP(ptr)) - WSIZE);
        mm_free(ptr);
    }

#ifdef DEBUG
    mm_check_ptr = new_ptr;
    mm_check_len = len;
#endif

    CHECK_HEAP();
    return new_ptr;
}

static int mm_check(void) {
#ifdef DEBUG
    char *bp = heap_listp;
    char *header = HDRP(bp);
    size_t prev_allocated = GET_PREV_ALLOC(header);

#ifdef VERBOSE
    printf("Heap (%p):\n", heap_listp);
#endif

    for (; GET_SIZE(header) > 0; header = HDRP(bp)) {
#ifdef VERBOSE
        printblock(bp);
#endif

        if (!checkblock(bp, prev_allocated)) {
            return 0;
        }

        prev_allocated = GET_ALLOC(header);
        bp = NEXT_BLKP(bp);
    }

#ifdef VERBOSE
    printblock(bp);
#endif

    if (!checkblock(bp, prev_allocated)) {
        return 0;
    }

    for (size_t i = 0; i < mm_check_len; ++i) {
        if (mm_check_buf[i] != mm_check_ptr[i]) {
            mm_check_len = 0;
            mm_check_ptr = NULL;
#ifdef VERBOSE
            printf("realloc perserve data failure at %d\n", i);
#endif
            return 0;
        }
    }

    mm_check_len = 0;
    mm_check_ptr = NULL;
#endif

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
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 0, 1)); /* epilogue header */

    return coalesce(bp);
}

static void* coalesce(void* bp) {
    size_t prev_alloctated = GET_PREV_ALLOC(HDRP(bp));
    size_t next_allocated = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    if (prev_alloctated && next_allocated) {
        bp = coalesce_aa(bp);
    } else if (prev_alloctated && !next_allocated) {
        bp = coalesce_af(bp);
    } else if (!prev_alloctated && next_allocated) {
        bp = coalesce_fa(bp);
    } else {
        bp = coalesce_ff(bp);
    }

#ifdef NEXT_FIT
    if (rover > (char*)bp && rover < NEXT_BLKP(bp)) {
        rover = bp;
    }
#endif

    return bp;
}

static void* find_fit(size_t asize) {
    size_t size;
    char* header;

#ifdef NEXT_FIT
    for (char* bp = rover; ; bp = NEXT_BLKP(bp)) {
        header = HDRP(bp);
        size = GET_SIZE(header);
        if (!GET_ALLOC(header)) {
            if (size >= asize) {
                return rover = bp;
            }
        } else if (size == 0) {
            break;
        }
    }

    for (char* bp = heap_listp; bp != rover; bp = NEXT_BLKP(bp)) {
        header = HDRP(bp);
        size = GET_SIZE(header);
        if (!GET_ALLOC(header) && size >= asize) {
            return rover = bp;
        }
    }

    return NULL;
#else
    for (char* bp = heap_listp; ; bp = NEXT_BLKP(bp)) {
        header = HDRP(bp);
        size = GET_SIZE(header);
        if (!GET_ALLOC(header)) {
            if (size >= asize) {
                return bp;
            }
        } else if (size == 0) {
            break;
        }
    }

    return NULL;
#endif
}

static void* refit(void* bp, size_t asize) {
    char* new_bp;
    size_t block_size = GET_SIZE(HDRP(bp));
    size_t potential_size = block_size;

#ifdef REFIT_MAX_UTIL
    char* prev_bp = PREV_BLKP(bp);
    size_t prev_allocated = GET_PREV_ALLOC(HDRP(bp));
    if (!prev_allocated) {
        potential_size += GET_SIZE(HDRP(prev_bp));
    }

    char* next_bp = NEXT_BLKP(bp);
    size_t next_allocated = GET_ALLOC(HDRP(next_bp));
    if (!next_allocated) {
        potential_size += GET_SIZE(HDRP(next_bp));
    }

    if (potential_size < asize) {
        return NULL;
    }

    if (prev_allocated && next_allocated) {
        new_bp = bp;
    } else if (prev_allocated && !next_allocated) {
        SET_SIZE(HDRP(bp), potential_size);
        new_bp = bp;
    } else if (!prev_allocated && next_allocated) {
        SET_SIZE(HDRP(prev_bp), potential_size);
        SET_ALLOC(HDRP(prev_bp), 1);
        new_bp = prev_bp;
    } else {
        SET_SIZE(HDRP(prev_bp), potential_size);
        SET_ALLOC(HDRP(prev_bp), 1);
        new_bp = prev_bp;
    }

    if (new_bp != bp) {
        memmove(new_bp, bp, MIN(block_size, asize) - WSIZE);
    }

#elif defined(REFIT_KEEP)
    char* next_bp = NEXT_BLKP(bp);
    size_t next_allocated = GET_ALLOC(HDRP(next_bp));
    if (!next_allocated) {
        potential_size += GET_SIZE(HDRP(next_bp));
    }

    if (potential_size < asize) {
        return NULL;
    }

    if (next_allocated) {
        new_bp = bp;
    } else {
        SET_SIZE(HDRP(bp), potential_size);
        new_bp = bp;
    }
#else
#error "Not Implemented"
#endif

    place_r(new_bp, asize);

// the following code will decrease memory util, so they're not used
// #ifdef NEXT_FIT
//     next_bp = NEXT_BLKP(new_bp);
//     if (!GET_ALLOC(HDRP(next_bp))) {
//         rover = next_bp;
//     }
// #endif

    return new_bp;
}

static void place(void* bp, size_t asize) {
    void* header = HDRP(bp);
    size_t size = GET_SIZE(header);
    size_t remaining_size = size - asize;

    if (remaining_size >= DSIZE) {
        PUT(header, PACK(asize, GET_PREV_ALLOC(header), 1));
        bp = NEXT_BLKP(bp);
        header = HDRP(bp);
        PUT(header, PACK(remaining_size, 1, 0));
        COPY(FTRP(bp), header);
    } else {
        SET_ALLOC(header, 1);
        bp = NEXT_BLKP(bp);
        header = HDRP(bp);
        SET_PREV_ALLOC(header, 1);
        if (!GET_ALLOC(header)) {
            SET_PREV_ALLOC(FTRP(bp), 1);
        }
    }
}

static int checkblock(void* bp, size_t prev_allocated) {
    if (((size_t)bp) % DSIZE != 0) {
#ifdef VERBOSE
        printf("Error: %p is not double word aligned\n", bp);
#endif
        return 0;
    }

    void* header = HDRP(bp);
    if (!GET_ALLOC(header)) {
        if (GET(header) != GET(FTRP(bp))) {
#ifdef VERBOSE
            printf("Error: header does not match footer\n");
#endif
            return 0;
        }
    }

    size_t size = GET_SIZE(header);
    if (size < DSIZE) {
        if (bp == heap_listp) {
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
        } else {
#ifdef VERBOSE
            printf("Error: Invalid block size\n");
#endif
            return 0;
        }
    }

    if (prev_allocated != GET_PREV_ALLOC(header)) {
#ifdef VERBOSE
        printf("Error: Prev block allocated inconsistent\n");
#endif
        return 0;
    }

    return 1;
}

static void printblock(void* bp) {
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
        printf("%p: header: [%d:%c:%c] footer: [%d:%c]\n", 
            bp, header_size, prev_allocated, header_allocated, footer_size, footer_allocated);
    } else {
        printf("%p: header: [%d:%c:%c]\n", bp, header_size, prev_allocated, header_allocated);
    }
}

static void* coalesce_aa(void* bp) {
    return bp;
}

static void* coalesce_af(void* bp) {
    char* header = HDRP(bp);
    char* next_bp = NEXT_BLKP(bp);

    size_t size = GET_SIZE(header) + GET_SIZE(HDRP(next_bp));
    SET_SIZE(header, size);
    COPY(FTRP(bp), header);

    return bp;
}

static void* coalesce_fa(void* bp) {
    char* prev_bp = PREV_BLKP(bp);
    char* prev_hdr = HDRP(prev_bp);
    size_t size = GET_SIZE(prev_hdr) + GET_SIZE(HDRP(bp));

    SET_SIZE(prev_hdr, size);
    COPY(FTRP(prev_bp), prev_hdr);

    return prev_bp;
}

static void* coalesce_ff(void* bp) {
    char* prev_bp = PREV_BLKP(bp);
    char* next_bp = NEXT_BLKP(bp);
    char* prev_hdr = HDRP(prev_bp);
    size_t size = GET_SIZE(HDRP(bp)) + GET_SIZE(prev_hdr) + GET_SIZE(HDRP(next_bp));

    SET_SIZE(prev_hdr, size);
    COPY(FTRP(prev_bp), prev_hdr);

    return prev_bp;
}

static void place_r(void* bp, size_t asize) {
    size_t potential_size = GET_SIZE(HDRP(bp));
    size_t remaining_size = potential_size - asize;
    if (remaining_size >= DSIZE) {
        char* next_bp = bp + asize;

        SET_SIZE(HDRP(bp), asize);
        PUT(HDRP(next_bp), PACK(remaining_size, 1, 0));
        COPY(FTRP(next_bp), HDRP(next_bp));

        next_bp = NEXT_BLKP(next_bp);
        SET_PREV_ALLOC(HDRP(next_bp), 0);
        if (!GET_ALLOC(HDRP(next_bp))) {
            COPY(FTRP(next_bp), HDRP(next_bp));
        }
    } else {
        char* next_bp = NEXT_BLKP(bp);
        SET_PREV_ALLOC(HDRP(next_bp), 1);
        if (!GET_ALLOC(HDRP(next_bp))) {
            COPY(FTRP(next_bp), HDRP(next_bp));
        }
    }
}