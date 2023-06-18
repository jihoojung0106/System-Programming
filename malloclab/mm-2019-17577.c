/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * To keep track of free, allocated blocks of memory, I implemented the implicit list/
 * and for how to look for free blocks, I implement next fit method
 * (together with best fit,I would prefer it to call it better fit, because it's not technically best,
 * it's the best amongst the small number of blocks of list), since it normally shows better performance than first fit.
 * and I reuse the method from the text book a lot.
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
/* single word (4) or double word (8) alignment */


#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/* Basic constants and macros */
#define WSIZE       4           /* Word and header/footer size (bytes) */
#define DSIZE       8           /* Double word size (bytes) */
#define CHUNKSIZE   (1 << 12)   /* Extend heap by this amount (bytes) */
#define INIT_CHUNKSIZE	(1<<6)  /* initialize heap by this amount (bytes) */

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)  (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)    (GET(p) & ~0x7)
#define GET_ALLOC(p)   (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)    ((char *)(bp) - WSIZE)
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)   (((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)   (((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE)))

#ifdef DEBUG
/* heap consistency checker */
int mm_check(void);
#endif

char *heap_listp;
char *last_bp;
static void *next_fit(size_t asize);
static void place(void *bp, size_t asize);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);


/*
 * mm_init - initialize the malloc package. it simply extends the heap size, and set the last_bp;
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) {
        return -1;
    }

    PUT(heap_listp, 0);                             /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));        /* Epilogue header */
    heap_listp += (2*WSIZE);

    /* Extend the empty heap with a free block of INIT_CHUNKSIZE bytes */
    if (extend_heap(INIT_CHUNKSIZE/WSIZE) == NULL) {
        return -1;
    }
    last_bp = heap_listp;

#ifdef DEBUG
    mm_check();
#endif
    return 0;
}

/*
 * mm_malloc - firsts, adjust the block size that it can be aligned.
 * and then search the free list using the next_fit method, and save the last_bp
 * if no fit found, extend the heap size and place;
 */
void *mm_malloc(size_t size) {
    size_t asize;       /* Adjusted block size */
    size_t extend_size;         /* Amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if (size == 0) {
        return NULL;
    }

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE) {asize = 2*DSIZE;}
    else {asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);}
    /* Search the free list for a fit */
    if ((bp = next_fit(asize)) != NULL) {
        place(bp, asize);
        last_bp = bp; /*since I am implementing next_fit, save the bp*/

#ifdef DEBUG
        mm_check();
#endif
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extend_size = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extend_size/WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);
    last_bp = bp; /*since I am implementing next_fit, save the bp*/

#ifdef DEBUG
    mm_check();
#endif
    return bp;
}

/*
 * mm_free - unmark the header and footer of the current block and coalesce if needed
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    //unmark the block (header and footer)
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    //coalesce if needed
    coalesce(bp);

#ifdef DEBUG
    mm_check();
#endif
}

/*
 * mm_realloc - first, adjust the size of block
 * and if the required size is smaller or equal to current block size, nothing happened
 * else, allocate the new block or simply expand the current block
 */
void *mm_realloc(void *ptr, size_t size) {
    void *newptr;
    int remainder;
    size_t asize;
    size_t esize;

    /* Ignore spurious requests*/
    if (size==0){return NULL;}
    /* Adjust block size to include overhead and alignment reqs, same as mm_malloc */
    if (size <= DSIZE){ asize = 2*DSIZE;}
    else{asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);}

    //if the required size is smaller or equal to current block size, nothing happened
    if(GET_SIZE(HDRP(ptr)) >= asize){
#ifdef DEBUG
        mm_check();
#endif
        return ptr;}
    //else,
    // make sure that next block is a not allocated block nor the epilogue block, if not reuse the current block
        if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) || !GET_SIZE(HDRP(NEXT_BLKP(ptr)))) {
            remainder = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - asize;
            // if the current block size and the next block size altogether is smaller than adjusted size, extend heap
            if (remainder < 0) {
                esize = MAX(-remainder, CHUNKSIZE);
                //if extend heap failed, return null
                if (!extend_heap(esize/WSIZE)){ return NULL;}
                remainder += esize;
            }
            PUT(HDRP(ptr), PACK(asize + remainder, 1)); //increase the size of block in header
            PUT(FTRP(ptr), PACK(asize + remainder, 1)); //increase the size of block in footer

#ifdef DEBUG
            mm_check();
#endif
            return ptr;
        }
        // else, allocate new block
        else {
            newptr = mm_malloc(asize - DSIZE);
            memcpy(newptr, ptr, MIN(size, asize));
            mm_free(ptr);

#ifdef DEBUG
            mm_check();
#endif
            return newptr;
        }

}
/*********************************************************
 * helper function *
 ********************************************************/


/*
   since there are some results that next_fit is better than
   first_fit, I am implementing next_fit combined with best_fit
   */
static void *next_fit(size_t asize) {
    char *bp = NEXT_BLKP(last_bp);
    int min_size=1000000;
    char *min_bp;
    short cnt=0;
    int diff;
    short ei=0;
    while(GET_SIZE(HDRP(bp)) > 0){
        diff=GET_SIZE(HDRP(bp))-asize; /*finding the place where the free block size is bigger than the required size*/
        if (!GET_ALLOC(HDRP(bp)) && diff>=0) {
            ei++;
            //to incorporate benefits of best_fit method, loop for a while to find the better place to fit in.
            //bigger than the required size, but smallest gap b/w block size and asize;
            //this will increase util
            if(cnt<10 && min_size>diff) {
                min_size = diff;
                min_bp = bp;
                cnt++;
            }
            last_bp = bp;
        }
        bp = NEXT_BLKP(bp);
    }
    if(cnt){last_bp=min_bp; return min_bp;}
    if(ei){return bp;}

    //if fit not found after the last_bp,restart from the start point of the list, and loop again

    ei=0;cnt=0;min_size=1000000;
    bp = heap_listp;
    while (bp < last_bp) {
        bp = NEXT_BLKP(bp);
        diff=GET_SIZE(HDRP(bp))-asize;
        //same as the above one, incorporate the best_fit strategy
        if (!GET_ALLOC(HDRP(bp)) && diff>=0) {
            ei++;
            if(cnt<10 && min_size>diff){
                min_size=diff;
                min_bp=bp;
                cnt++;
            }
            last_bp = bp;
        }
    }
    if(cnt){last_bp=min_bp;return min_bp;}
    if(ei){return last_bp;}
    return NULL;
}

//placing the block in allocated list
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));
    if ((csize - asize) >= (2 * (DSIZE))) {
        //place the new allocated block
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        //move to next block
        bp = NEXT_BLKP(bp);
        //split the block, leftover block is unmarked
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else {
        //use the block as it is
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

//coalecing : use the boundary tag to coalesing in constant time/
//check if the previous or next block (which means, we need coalescing),
//and do what needs to do for each case
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc){
        /* Case 1 : both the previous and the next block are allocated : no coalescing.*/
        if(next_alloc){last_bp = bp;return bp;}
        else{
            /* Case 2 : previous allocated, next block freed*/
            size += GET_SIZE(HDRP(NEXT_BLKP(bp))); //increase the size by next_block's size
            PUT(HDRP(bp), PACK(size, 0));  //header of current block become the header of coalescing block
            PUT(FTRP(bp), PACK(size, 0)); //unmark
            //set the bp to current block -> no need to change
        }
    }
    else{
        //since we need to coalesce previous block, reset the bp to previous block in case 3,4
        /* Case 3 : next allocated, previous block freed*/
        if(next_alloc){
            size += GET_SIZE(HDRP(PREV_BLKP(bp))); //increase the size by previous_block's size
            PUT(FTRP(bp), PACK(size, 0)); //footer of current block become the footer of coalescing block
            PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); //header of previous block become the header of coalescing block
            bp = PREV_BLKP(bp);
        }
        else {
            /* Case 4 : next freed, previous block freed*/
            size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
            //increase the size by previous_block's size + next_block's size
            PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); //header of previous block become the header of coalescing block
            PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); //footer of next block become the footer of coalescing block
            bp = PREV_BLKP(bp);
        }
    }
    last_bp = bp; //set the last_bp to bp;
    return bp;
}

/*extend the heap size : extend_heap is called when the heap is initialized
or when the mm_malloc function cannot find the fit, and need to extend heap
compute the size that goes well with alignment policy.
 initialize the block and coalesce if needed
 */
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    // Allocate an even number of words to maintain alignment
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    // initialize free block header/footer and the epilogue header
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // coalesce if the previous block was free
    return coalesce(bp);
}


#ifdef DEBUG
//check heap consistency
int mm_check(void) {
    void *bp = heap_listp;
    /* Are there free blocks that failed to be coalesced? */
    /* All valid heap addresses? */
    for (; bp; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && !GET_ALLOC(HDRP(PREV_BLKP(bp)))) {
            fprintf(stderr, "mm_check failed\n");
            return -1;
        }
        if (GET_SIZE(HDRP(bp)) > 0) {
            if (bp < heap_listp || bp >= mem_sbrk(0) || GET_SIZE(HDRP(bp)) != GET_SIZE(FTRP(bp)) ||
                GET_ALLOC(HDRP(bp)) != GET_ALLOC(FTRP(bp))) {
                fprintf(stderr, "mm_check failed\n");
                return -1;
            }
        }
    }
    return 0;
}
#endif