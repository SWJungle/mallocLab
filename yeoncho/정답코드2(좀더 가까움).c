#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "mm.h"
#include "memlib.h"
team_t team = {
    /* Team name */
    "j2team",
    /* First member's full name */
    "Smith",
    /* First member's email address */
    "22@naver.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)
#define MAX(x,y) ((x)>(y)? (x) : (y))
#define PACK(size, alloc) ((size)| (alloc))  //size와alloc을 합친다(size자체가 8단위라서 000이라 상관없음)
#define GET(p)            (*(int *)(p))           //p를 받는다
#define PUT(p, val)       (*(int *)(p) = (val))   //value값을 넣는다
#define GET_SIZE(p)       (GET(p) & ~0x7) // 0x 는 16진수를 뜻함. 00000007이니깐 반대면 FFFFFFF8이 된다.
#define GET_ALLOC(p)      (GET(p) & 0x1)  //0001
#define HDRP(bp)          ((void *)(bp) - WSIZE)
#define FTRP(bp)          ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp)     ((void *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)     ((void *)(bp) - GET_SIZE(HDRP(bp) - WSIZE))
#define NEXT_FREE(bp)     (*(void **)(bp+WSIZE))
#define PREV_FREE(bp)     (*(void **)(bp))
static char *heap_listp = 0;
static char *free_listp = 0;
extern int mm_init(void);
extern void *mm_malloc(size_t size);
extern void mm_free(void *ptr);
static void* coalesce(void *bp);
static void* extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void* bp, size_t asize);
static void removeBlock(void* bp);
void* mm_realloc(void* bp, size_t size);
int mm_init(void)
{
    if((heap_listp = mem_sbrk(6*WSIZE)) == (void*)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE*2, 1));
    PUT(heap_listp + (2*WSIZE), NULL);
    PUT(heap_listp + (3*WSIZE), NULL);
    PUT(heap_listp + (4*WSIZE), PACK(DSIZE*2, 1));
    PUT(heap_listp + (5*WSIZE), PACK(0, 1));
    free_listp = heap_listp + DSIZE;
    if(free_listp = extend_heap(CHUNKSIZE/WSIZE)==NULL)
        return -1;
    return 0;
}
static void* extend_heap(size_t words)
{
    char* bp;
    size_t size;
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if (size < 4*WSIZE)
        size = 4*WSIZE;
    if((long)(bp = mem_sbrk(size))==-1)
        return NULL;
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    return coalesce(bp);
}
void mm_free(void* bp)
{
    if(!bp) return;
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}
static void* coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    if (prev_alloc && next_alloc){}
    else if (prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        removeBlock(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        removeBlock(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else{
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        removeBlock(PREV_BLKP(bp));
        removeBlock(NEXT_BLKP(bp)); 
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    if(free_listp == NULL){
        free_listp = bp;
        PREV_FREE(bp) = NULL;
        NEXT_FREE(bp) = NULL;
    }
    else{
        NEXT_FREE(bp) = free_listp;
        PREV_FREE(free_listp) = bp;
        PREV_FREE(bp) = NULL;
        free_listp = bp;
    }
    return bp;
}
void* mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char* bp;
    if (size <= 0)
        return NULL;
    asize = MAX(((((size_t)(size) + 7) & ~0x7) + DSIZE), 4*WSIZE);
    // if (size <= DSIZE)
    //     asize = 2*DSIZE;
    // else
    //     asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/DSIZE);
    if ((bp = find_fit(asize)) != NULL){
        place(bp, asize);                  
        return bp;
    }
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE))==NULL)
        return NULL;
    place(bp, asize);
    return bp;
}
static void *find_fit(size_t asize)
{
    void *bp;
    for (bp = free_listp; bp != NULL; bp = NEXT_FREE(bp)){
        if (asize <= GET_SIZE(HDRP(bp))){
            return bp;
        }
    }
    return NULL;
}
static void place(void* bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    if ((csize - asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        removeBlock(bp);
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        coalesce(bp);
    }
    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        removeBlock(bp);
    }
}
void* mm_realloc(void* bp, size_t size)
{
    void* old_bp = bp;
    void* new_bp;
    size_t copySize;
    new_bp = mm_malloc(size);
    if (new_bp == NULL)       
        return NULL;
    copySize = GET_SIZE((char*)old_bp - WSIZE); 
    if (size < copySize)
        copySize = size;
    memcpy(new_bp, old_bp, copySize);
    mm_free(old_bp);
    return new_bp;
}
static void removeBlock(void* bp)
{
    if(PREV_FREE(bp))
        NEXT_FREE(PREV_FREE(bp)) = NEXT_FREE(bp);
    else
        free_listp = NEXT_FREE(bp);
    if(NEXT_FREE(bp))
        PREV_FREE(NEXT_FREE(bp)) = PREV_FREE(bp);
}