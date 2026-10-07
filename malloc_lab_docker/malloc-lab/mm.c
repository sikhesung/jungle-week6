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
/* rounds up to the nearest multiple of ALIGNMENT */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

static char *heap_listp;

static void *coalesced(void *bp)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    //아래 코드는 !가 없으면 사용중인 영역임을 의미
    if (prev_alloc && next_alloc) {
        return bp;
    }

    else if (prev_alloc && !next_alloc) {
        size = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp) , PACK(size , 0));
        PUT(FTRP(bp) , PACK(size , 0));
        return bp;
    }

    else if (!prev_alloc && !next_alloc) {
        size = size + GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp) , PACK(size , 0));
        PUT(FTRP(bp) , PACK(size , 0));
        return bp;
    }

    else(!prev_alloc && next_alloc); {
        size = size + GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp) , PACK(size , 0));
        PUT(FTRP(bp) , PACK(size , 0));
        return bp;
    }
}

void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp) , PACK(size,0));
    PUT(FTRP(bp) , PACK(size,0));
    coalesced(bp);
}

static void *find_fit(size_t asize)
{
    void *bp;

    for (bp = NEXT_BLKP(heap_listp);GET_SIZE(HDRP(bp)) != 0; bp = NEXT_BLKP(bp)){
        if(GET_ALLOC(HDRP(bp)) == 0 && GET_SIZE(HDRP(bp)) >= asize)
        return bp;
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    size_t psize;
    psize = csize - asize;

    if(2 * DSIZE <=psize){
        PUT(HDRP(bp) , PACK(asize,1));
        PUT(FTRP(bp) , PACK(asize,1));

        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp) , PACK(psize,0));
        PUT(FTRP(bp) , PACK(psize,0));
    }
    else{
        PUT(HDRP(bp) , PACK(csize,1));
        PUT(FTRP(bp) , PACK(csize,1));
    }
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    if(words % 2 == 0){
    size = words * WSIZE;
    }

    else if(words % 2 == 1) {
        words = words + 1;
        size = words * WSIZE;
    }

    bp = mem_sbrk(size);
    
    if(bp == (void *)-1){
        return NULL;
    }

    PUT(HDRP(bp) , PACK(size,0));
    PUT(FTRP(bp) , PACK(size,0));

    PUT(HDRP(NEXT_BLKP(bp)) , PACK(0,1));

    bp = coalesced(bp);

    return(bp);
}

void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0){
        return NULL;
    }

    asize = (size + DSIZE + (DSIZE - 1)) / DSIZE * DSIZE;

    bp = find_fit(asize);
    if(bp != NULL){
        place(bp , asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);

    bp = extend_heap(extendsize / WSIZE);

    if (bp != NULL){
    place(bp , asize);
    return bp;
    }
    else if(bp == NULL){
        return NULL;
    }
}

int mm_init(void)
{
    char *bp;
    char *z;

    bp = mem_sbrk(4 * WSIZE);
    if(bp == (void *)-1){
        return -1;
    }

    PUT(bp , 0);
    PUT(bp + WSIZE ,PACK(DSIZE , 1));
    PUT(bp + 2 * WSIZE ,PACK(DSIZE , 1));
    PUT(bp + 3 * WSIZE ,PACK(0 , 1));

    heap_listp = bp + 2 * WSIZE;

    z = extend_heap(CHUNKSIZE / WSIZE);

    if ( z == NULL){
        return -1;
    }

    return 0;
}

void *mm_realloc(void *ptr, size_t size)
{
    void *newptr;
    size_t csize;
    size_t dsize;
    size_t copysize;
    
    if( ptr == NULL){
        newptr = mm_malloc(size);
        return newptr;
    }

    if (size == 0){
        mm_free(ptr);
        return NULL;
    }

    newptr = mm_malloc(size);
    if (newptr == NULL){
        return NULL;
    }

    csize = GET_SIZE(HDRP(ptr));

    dsize = csize - DSIZE;

    copysize = (dsize > size) ? size : dsize;

    memcpy(newptr , ptr , copysize);

    mm_free(ptr);

    return newptr;
}