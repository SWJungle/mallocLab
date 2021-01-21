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


#define ALIGNMENT 8
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)
#define SIZEMASK (~0x7)
#define PACK(size, alloc) ((size)|(alloc))
#define getSize(x) ((x)->size & SIZEMASK) // x�� ����� ���Ѵ�.

#define WSIZE 4 //word�� ũ��
#define DSIZE 8 // double word�� ũ�⸦ ���ϰ� �ִ�
#define CHUNKSIZE (1<<12) // �ʱ� �� ����� �����Ѵ�.

#define OVERHEAD 8 // hear�� footer�� ����. ������ ����Ǵ� ������ �ƴϴ� ��������� ǥ��
#define MAX(x, y) ((x)>(y)?(x):(y))
#define PACK(size, alloc) ((size)|(alloc)) //size�� alloc(a)�� ���� �� word�� ���´�. �̸� �̿��Ͽ� �ش��� ǲ�Ϳ� ���� ������ �� �ִ�.
#define GET(p) (*(size_t *)(p)) //������ p�� ����Ű�� ���� �� word�� ���� �о�´�
#define PUT(p, val) (*(size_t *)(p) = (val)) //������ p�� ����Ű�� �� ������ ���� val�� ����Ѵ�

#define GET_SIZE(p) (GET(p) & ~0x7) // �����Ͱ� ����Ű�� ���� �� word�� ���� ����, ���� 3bit�� ������. �� header���� block size�� �д´�
#define GET_ALLOC(p) (GET(p) & 0x1) // �����Ͱ� ����Ű�� ���� �� word�� ���� ����, ������ 1bit�� �д´�. �� 0�̸� ����� �Ҵ�Ǿ� ���� �ʰ�, 1�̸� �Ҵ� �Ǿ� �ִٴ� �ǹ�

#define HDRP(bp) ((char *)(bp)-WSIZE)  //�־��� ������ bp(block pointer?)�� header�� �ּҸ� ����Ѵ�
#define FTRP(bp) ((char *)(bp)+GET_SIZE(HDRP(bp))-DSIZE) //�־��� bp�� ǲ���� �ּҸ� ����Ѵ�.

#define NEXT_BLKP(bp) ((char *)(bp)+GET_SIZE(((char*)(bp)-WSIZE))) //�־��� ������ bp�� �̿��ؼ� ���� ����� �ּҸ� ��´�
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char*)(bp)-DSIZE))) //�־��� �����͸� �̿��ؼ� ���� ����� �ּҸ� ��� �´�.


static void 	*coalesce(void *bp);
static void 	*extend_heap(size_t words);
static void 	*find_fit(size_t asize);
static void 	place(void *bp, size_t asize);
void 			*mm_malloc(size_t size);
void			*mm_realloc(void *ptr, size_t size)
void 			mm_free(void *ptr);
int 			mm_init(void);

static char *heap_listp;
static char *lastbp;

int mm_init(void) {
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) -1){
        return -1;
    }
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); //���ѷα� ���
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); //���ѷα� ǲ��
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1)); //���ʷα� ǲ��
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

// �ּ� 16����Ʈ ũ���� ��� ����
// 8����Ʈ�� ���� ������ ������Ű�� ����
// �߰����� 8����Ʈ�� ����� ǲ�� ������带 ����
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else // 8����Ʈ�� �Ѵ� ��û�� ������� ����Ʈ ���� �����ְ� ���� 8�� ����� �ݿø�.
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

static void *coalesce(void *bp) { // ���� ���� ��� ��ġ��
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // ���� ����� footer�� �̿��� ���� ����� ũ�⸦ �ľ��Ѵ�.
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // ���� ����� header�� �̿��� ���� ����� ũ�⸦ �ľ��Ѵ�.
    size_t size = GET_SIZE(HDRP(bp)); // ���� ����� header�� �̿��� ���� ����� ũ�⸦ �ľ��Ѵ�.


    //Case 1 : ���� ����� �� �� ����� ��� �Ҵ� �Ǿ��ִ� ����
    if (prev_alloc && next_alloc) {
        return bp;      // ��ĥ�� ���� ������ bp �״�� �����Ѵ�.
    }

    //Case 2 : ���� ��ϸ� ���� ����� ���
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); 	// ���� ����� header�� �̿��� ���� ����� ũ�⸦ ���ϰ� size�� ���Ѵ�. 
        PUT(HDRP(bp), PACK(size, 0));			// �� �����ϰ� ��ġ�鼭 ���� ����� ���������� ����� ������ ���� bp�� header�� size �� alloc = 0 �� �Ҵ��Ѵ�. 
        PUT(FTRP(bp), PACK(size, 0));			// footer ���� ���������� �Ҵ����ش�.
    }
    
	//Case 3 :  ���� ��ϸ� ���� ����� ���
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));		// ���� ����� footer�� �̿��� ���� ����� ũ�⸦ ���ϰ� size�� ���Ѵ�.
        PUT(FTRP(bp), PACK(size, 0));				// ���� bp�� footer�� ���� ������ ����� footer ��ġ�� ���� ������ ���� bp footer�� size �� alloc�� ��´�. 
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));	// ���� bp�� header�� ���� ������ ����� header ��ġ�� ���� ������ ���� bp header�� size �� alloc�� ��´�.
        bp = PREV_BLKP(bp); //���� ���� �ּҸ� ��� �����Ѵ�
    }
	//Case 4 : �� �� ��� ���� ����� ���
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

    //��û�� ũ�⸦ ���� 2������ ����� �ݿø� �ϸ�, �� �Ŀ� �޸� �ý������� ���� �߰����� �� ���� ��û
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long) (bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0)); //������� �ش�
    PUT(FTRP(bp), PACK(size, 0)); //������� ǲ��
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); //�����ʷα� ���
    return coalesce(bp);
}

static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));
    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp); //���� ������� �̵�
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