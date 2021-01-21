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
        "jungle_week6_team07",
        /* First member's full name */
        "youngdong kim",
        /* First member's email address */
        "zeroistfilm@naver.com",
        /* Second member's full name (leave blank if none) */
        "",
        /* Second member's email address (leave blank if none) */
        ""
};


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)
#define SIZEMASK (~0x7)
#define PACK(size, alloc) ((size)|(alloc))
#define getSize(x) ((x)->size & SIZEMASK) // x의 사이즈를 구한다.
#define WSIZE 4 //word의 크기
#define DSIZE 8 // double word의 크기를 정하고 있다
#define CHUNKSIZE (1<<12) // 초기 힙 사이즈를 설정한다.

#define OVERHEAD 8 // hear와 footer의 공간. 실제로 저장되는 공간은 아니니 오버헤드라고 표현
#define MAX(x, y) ((x)>(y)?(x):(y))
#define PACK(size, alloc) ((size)|(alloc)) //size와 alloc(a)의 값을 한 word로 묶는다. 이를 이용하여 해더와 풋터에 쉽게 저장할 수 있다.
#define GET(p) (*(size_t *)(p)) //포인터 p가 가르키는 곳의 한 word의 값을 읽어온다
#define PUT(p, val) (*(size_t *)(p) = (val)) //포인터 p가 가르키는 한 워드의 값에 val을 기록한다

#define GET_SIZE(p) (GET(p) & ~0x7) // 포인터가 가르키는 곳의 한 word를 읽은 다음, 하위 3bit를 버린다. 즉 header에서 block size를 읽는다
#define GET_ALLOC(p) (GET(p) & 0x1) // 포인터가 가르키는 곳의 한 word를 읽은 다음, 최하위 1bit를 읽는다. 즉 0이면 블록이 할당되어 있지 않고, 1이면 할당 되어 있다는 의미

#define HDRP(bp) ((char *)(bp)-WSIZE)  //주어진 포인터 bp(block pointer?)의 header의 주소를 계산한다
#define FTRP(bp) ((char *)(bp)+GET_SIZE(HDRP(bp))-DSIZE) //주어진 bp의 풋터의 주소를 계산한다.

#define NEXT_BLKP(bp) ((char *)(bp)+GET_SIZE(((char*)(bp)-WSIZE))) //주어진 포인터 bp를 이용해서 다음 블록의 주소를 얻는다
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char*)(bp)-DSIZE))) //주어진 포인터를 이용해서 이전 블록의 주소를 얻어 온다.

/*  mi
 * mm_init - initialize the malloc package.
 */
int mm_init(void);
void *mm_malloc(size_t size);
static void *find_fit(size_t asize);
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
void mm_free(void *bp);
static void *coalesce(void *bp);

static char *heap_listp;
static char *lastbp;

int mm_init(void) {
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) -1){
        return -1;
    }
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); //프롤로그 헤더
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); //프롤로그 풋터
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1)); //에필로그 풋터
    heap_listp += (2 * WSIZE);
    lastbp = heap_listp;
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

void *mm_malloc(size_t size) {
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0) {
        return 0;
    }

// 최소 16바이트 크기의 블록 구성
// 8바이트는 정렬 조건을 만족시키기 위해
// 추가적인 8바이트는 헤더아 풋터 오버헤드를 위해
//    asize = MAX(ALIGN(size) + DSIZE, MINIMUM);ZE);
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else // 8바이트를 넘는 요청은 오버헤드 바이트 내에 더해주고 인접 8의 배수로 반올림.
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if (( bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    return bp;
}

static void *find_fit(size_t asize) {
    void *bp;
    for (bp = lastbp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    for (bp = heap_listp; bp!=lastbp; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL;
}

static void *extend_heap(size_t words) {
    void *bp;
    size_t size;

    //요청한 크기를 인접 2워드의 배수로 반올림 하며, 그 후에 메모리 시스템으로 부터 추가적인 힙 공간 요청
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0)); //프리블록 해더
    PUT(FTRP(bp), PACK(size, 0)); //프리블록 풋터
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); //뉴에필로그 헤더
    return coalesce(bp);
}
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));
    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp); //다음 블록으로 이동
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}
/*
 * mm_free - Freeing a block does nothing.
 */

void mm_free(void *bp) {
    if (!bp) return;
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블럭 풋터의 할당 여부 = 이전 블럭의 할당 여부 0 no 1 yes
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블럭 헤드의 할당 여부 = 이후 블럭의 할당 여부 0 no 1 yes
    size_t size = GET_SIZE(HDRP(bp)); //현재 블럭의 사이즈


    //case 1 앞 뒤 모두 할당 된 상태
    if (prev_alloc && next_alloc) {
        return bp;
    }
        //case2  이전 블럭만 할당된 상태
        //다음블럭과 병합한 뒤 bp return
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 다음 블럭의 사이즈를 구함
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
        //case3 다음 블럭 할당 상태
        //이전 블럭과 병합한뒤 bp를 리턴
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp); //이전 블럭의 주소를 얻어 갱신한다
    }
        //case4 앞 뒤 모두 미할당
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    lastbp = bp;
    return bp;

}


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *) ((char *) oldptr - WSIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
