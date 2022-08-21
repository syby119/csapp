// #define DEBUG             /* Print additional info when debugging */
// #define VERBOSE           /* Print info when checking correctness */

// #define IMPLICIT_LIST     /* Heap structure: Implicit list */
// #define EXPLICIT_LIST     /* Heap structure: Explicit list */
#define SEGREGATED_LIST   /* Heap structure: Segregated list */


#ifdef IMPLICIT_LIST
// #define FIRST_FIT         /* Placement policy: first fit */
#define NEXT_FIT          /* Placement policy: next fit */

// #define REFIT_KEEP        /* Realloc policy: keep the data at its orignal position if possible */
#define REFIT_MAX_UTIL    /* Realloc policy: move the data to coalesce free block if possible */

#include "mm_implicit.c"
#elif defined(EXPLICIT_LIST)
#define LIFO              /* Insertion policy: LIFO */
// #define ADDR_ORDER        /* Insertion policy: Address-ordered, not implemented yet */

#include "mm_explicit.c"
#elif defined(SEGREGATED_LIST)

// #define FIRST_INSERT      /* Insertion policy: insert the free block at the list header */
#define ASCENDING_INSERT  /* Insertion policy: the size of the block in the list is in ascending order */

// #define SHRINK_BLOCK      /* Shrink block when realloc if possible */

#include "mm_segregated.c"
#endif