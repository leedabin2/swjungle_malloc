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
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* 가용리스트 조작을 위한 기본 상수 및 매크로 정의 */

// 기본 size 상수 정의
#define WSIZE 4 // 워드 크기
#define DSIZE 8 // 더블워드 크기
#define CHUNKSIZE (1<<12) // 초기 가용 블록과 힙확장을 위한 기본 크기

#define MAX(x,y) ((x) > (y) ? (x) : (y))

// 헤더와 푸터에 저장하는 값을 리턴하는 매크로
#define PACk (size,alloc) ((size) | (alloc)) // (0-free, 1-alloc 삽입)

// GET - 주소 P가 참조하는 워드를 읽는 함수
#define GET(p)
// PUT - 주소 P가 가리키는 워드에 val을 저장하는 함수
#define PUT(p,val)

// GET-SIZE - 주소 P가 기리키는 워드의 사이즈 정보를 가져오는 함수
#define GET-SIZE(p)
// GET-ALLOC - 주소 P가 가리키는 워드의 a(할당여부)를 가져오는 함수
#define GET-ALLOC(p)

// HDRP - bp가 속한 블록의 헤더를 알려주는 함수
#define HDRP(bp)
// FTRP - bp가 속한 블록의 푸터를 알려주는 함
#define FTRT(bp)

// 블록 포인터를 인자로 받아서 이후와 이전 블록 주소를 리턴
#define NEXT_BLKP(bp)
#define PREV_BLKP(bp) 

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7) // size보다 큰 가장 가까운 ALIGN 의 배수로 만들어 줌 (정)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t))) // 8바이트
/*--------------------------------------------------*/

/* 
 * mm_init - initialize the malloc package.
 초기 가용영역 4워드를 설정해줌 에러시 -1 반
 */
static char* heap_listp;

int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1)
        return -1;
    PUT(heap_listp,0);
    PUT(heap_listp + WSIZE, PACk(DSIZE,1));
    PUT(heap_listp + (2*WSIZE),PACk(DSIZE,1));
    PUT(heap_listp + (3*WSIZE), PACk(0,1));

    heap_listp += (2*WSIZE);

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) return -1;

    return 0;
}

static void *extend_heap(size_t words) {
    char *bp; // 블록포인터
    size_t size; // size 는 힙의 총 바이트 수

    size = (words % 2 == 1) ? ( (words + 1) * WSIZE ) : size * WSIZE;
    if ( (bp = mem_sbrk(size) ) == -1) return NULL; // 새메모리의 첫 부분을 bp로 지정

    // 들어오는 가용 블록의 헤더와 푸터 지정 
    PUT(HDRP(bp),PACk(size,0));
    PUT(FTRT(bp), PACk(size,0));
    PUT(HDRP(NEXT_BLKP(bp)),PACk(0,1));

    // 이전 블록이 가용이면 연결
    return 0;

}



/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
}


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














