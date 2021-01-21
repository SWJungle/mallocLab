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
        "YH",
        /* First member's email address */
        "cyhh1994@naver.com",
        /* Second member's full name (leave blank if none) */
        "",
        /* Second member's email address (leave blank if none) */
        ""
};


#define ALIGNMENT 8 // 더블 워드 정렬을 할 것이기 때문에 더블 워드 크기인 8(byte)에 해당하는 값을 매크로에 담았다 
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7) // 주소값 p에 가장 가까운 8의 배수 블럭을 찾는다. 

#define WSIZE 4 //워드 사이즈이다. 32bit 운영체제 기준 1word = 4byte 이다
#define DSIZE 8 // 더블 워드 사이즈이다. 32bit 운영체제 기준 2word = 8byte 이다
#define CHUNKSIZE (1<<12) // 힙을 확장할 때 사용하는 사이즈이다. 보통 4kb(= 1<<12)씩 확장한다.

#define MAX(x, y) ((x)>(y)?(x):(y)) // 삼항연산자를 이용해 MAX 함수를 매크로로 구현하였다.
#define PACK(size, alloc) ((size)|(alloc)) //size와 alloc 을 함께 묶어 1word로 표현하는 매크로이다.
#define GET(p) (*(size_t *)(p)) //p에 담겨있는 주소값을 1word 만큼 얻는다. 32bit 운영체제에서 size_t = 32bit 이다.
#define PUT(p, val) (*(size_t *)(p) = (val)) //p에 val (주소값)을 1word 만큼 담는다

#define GET_SIZE(p) (GET(p) & ~0x7) // 포인터가 가르키는 곳의 한 word를 읽은 다음, 하위 3bit를 버린다. 주로 header나 footer에서 size만 얻을 때 사용한다.
#define GET_ALLOC(p) (GET(p) & 0x1) // 포인터가 가르키는 곳의 한 word를 읽은 다음, 최하위 1bit를 읽는다. 주로 header나 footer에서 alloc만 얻을 때 사용한다.

#define HDRP(bp) ((char *)(bp)-WSIZE)  // 해당 bp(block point)의 header 주소를 얻는다.
#define FTRP(bp) ((char *)(bp)+GET_SIZE(HDRP(bp))-DSIZE) // 해당 bp의 footer  주소를 얻는다.

#define NEXT_BLKP(bp) ((char *)(bp)+GET_SIZE(((char*)(bp)-WSIZE))) // 해당 bp의 다음 블록 bp를 얻는다.
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char*)(bp)-DSIZE))) //해당 bp의 이전 블록 bp를 얻는다.


static void 	*coalesce(void *bp);
static void 	*extend_heap(size_t words);
static void 	*find_fit(size_t asize);
static void 	place(void *bp, size_t asize);
void 			*mm_malloc(size_t size);
void			*mm_realloc(void *ptr, size_t size);
void 			mm_free(void *ptr);
int 			mm_init(void);

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

static void *coalesce(void *bp) { // 인접 가용 블록 합치기
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록의 footer를 이용해 이전 블록의 크기를 파악한다.
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록의 header를 이용해 다음 블록의 크기를 파악한다.
    size_t size = GET_SIZE(HDRP(bp)); // 현재 블록의 header를 이용해 현재 블록의 크기를 파악한다.


    //Case 1 : 현재 블록의 앞 뒤 블록이 모두 할당 되어있는 상태
    if (prev_alloc && next_alloc) {
        return bp;      // 합칠게 없기 때문에 bp 그대로 리턴한다.
    }

    //Case 2 : 다음 블록만 가용 블록인 경우
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); 	// 다음 블록의 header를 이용해 다음 블록의 크기를 구하고 size에 더한다. 
        PUT(HDRP(bp), PACK(size, 0));			// 뒷 가용블록과 합치면서 현재 블록을 가용블록으로 만들기 때문에 현재 bp의 header에 size 및 alloc = 0 을 할당한다. 
        PUT(FTRP(bp), PACK(size, 0));			// footer 에도 마찬가지로 할당해준다.
    }
    
	//Case 3 :  이전 블록만 가용 블록인 경우
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));		// 이전 블록의 footer를 이용해 이전 블록의 크기를 구하고 size에 더한다.
        PUT(FTRP(bp), PACK(size, 0));				// 현재 bp의 footer가 이후 합쳐질 블록의 footer 위치와 같기 때문에 현재 bp footer에 size 및 alloc을 담는다. 
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));	// 이전 bp의 header가 이후 합쳐질 블록의 header 위치와 같기 때문에 이전 bp header에 size 및 alloc을 담는다.
        bp = PREV_BLKP(bp); //이전 블럭의 주소를 얻어 갱신한다
    }
	//Case 4 : 앞 뒤 모두 가용 블록인 경우
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    lastbp = bp;
    return bp;

}



void mm_free(void *bp) {
    if (!bp) return;
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    //요청한 크기를 인접 2워드의 배수로 반올림 하며, 그 후에 메모리 시스템으로 부터 추가적인 힙 공간 요청
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long) (bp = mem_sbrk(size)) == -1)
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