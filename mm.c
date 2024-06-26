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
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7) // size보다 큰 가장 가까운 ALIGN 의 배수로 만들어 줌 (정)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t))) // 8바이트
// 기본 size 상수 정의
#define WSIZE 4 // 워드 크기
#define DSIZE 8 // 더블워드 크기
#define CHUNKSIZE (1<<12) // 초기 가용 블록과 힙확장을 위한 기본 크기
#define MAX(x,y) ((x) > (y) ? (x) : (y))
// 헤더와 푸터에 저장하는 값을 리턴하는 매크로
#define PACK(size,alloc) ((size) | (alloc)) // (0-free, 1-alloc 삽입)
// GET - 주소 P가 참조하는 워드를 읽는 함수
#define GET(p) (*(unsigned int*) (p))
// PUT - 주소 P가 가리키는 워드에 val을 저장하는 함수
#define PUT(p,val) (*(unsigned int*)(p) = (val)) // void형을 int로변환 후 역참조
// GET-SIZE - 주소 P가 가리키는 워드의 사이즈 정보를 가져오는 함수
#define GET_SIZE(p) (GET(p) & ~0x7) // 블록의 크기가 8의 배수로 정렬되어 있기 때문
// GET-ALLOC - 주소 P가 가리키는 워드의 a(할당여부)를 가져오는 함수
#define GET_ALLOC(p) (GET(p) & 0x1)
// HDRP - bp가 속한 블록의 헤더를 알려주는 함수
#define HDRP(bp) ((char*)(bp) - WSIZE)
// FTRP - bp가 속한 블록의 푸터를 알려주는 함
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
// 블록 포인터를 인자로 받아서 이후와 이전 블록 주소를 리턴
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE( ((char *)(bp) - WSIZE )))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE( ((char*)(bp) - DSIZE)))

// 가용블록을 찾는 매크로
#define GET_SUCC(bp) (*(void**)(bp+WSIZE)) // 다음 가용 블록 주소
#define GET_PRED(bp) (*(void**)(bp)) // 이전 가용 블록 주소

static char* heap_listp;
static char* free_listp = NULL;
static void *next_bp;
int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *bp);
void *mm_realloc(void *ptr,size_t size);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t adjust_size);
static void place(void *bp, size_t adjust_size);
void delete_free_list(void *bp);
void put_free_list(void *bp);

/*
 * mm_init - initialize the malloc package.
 초기 가용영역 4워드를 설정해줌 에러시 -1 반
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(6*WSIZE)) == (void*)-1)
        return -1;
    PUT(heap_listp,0);
    PUT(heap_listp + WSIZE, PACK(2 * DSIZE,1)); // 가용블록 헤더
    PUT(heap_listp + (2*WSIZE),NULL); // 이전 가용블럭의 주소
    PUT(heap_listp + (3*WSIZE),NULL); // 이후 가용블럭의 주소
    PUT(heap_listp + (4*WSIZE),PACK(2 * DSIZE, 1)); // 가용블록 푸터
    PUT(heap_listp + (5*WSIZE), PACK(0,1)); // 에필로그 

    free_listp = heap_listp + DSIZE;// 첫 가용 블록 시작점 (가용리스트에 블록이 추가 될 때마다 제일 앞에 추가됨)
    printf(free_listp);
    //next_bp = heap_listp; // 다음 블록 포인터를 가리키는 변수 지정
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) // 워드단위로 받음 
        return -1;
    return 0;
}

static void *extend_heap(size_t words) {
    char *bp; // 블록포인터
    size_t size; // size 는 힙의 총 바이트 수

    size = (words % 2 == 1) ? ( (words + 1) * DSIZE ) : words * DSIZE; 

    if ((long)(bp = mem_sbrk(size) ) == -1)
        return NULL; // 새메모리의 첫 부분을 bp로 지정
    // 들어오는 가용 블록의 헤더와 푸터 지정
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));

    return coalesce(bp); // 이전 블록이 가용이면 연결
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
// 가용리스트에서 블록 할당하
void *mm_malloc(size_t size)
{
    size_t adjust_size; // 블록 사이즈 조정
    size_t extend_size; // 만약 사이즈가 안 맞을 경우 사이즈 확장
    char *bp; // 블록 포인터
    if (size == 0) return NULL;
    // 최소 16바이트 크기의 블록 구성(8바이트는 정렬 요건, 추가적인 8바이트는 헤더와 푸터의 오버헤드를 위함)
    if (size <= DSIZE) {
        adjust_size = 2 * DSIZE;
    }
    else {
        adjust_size = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    }
    // fit 한 가용 블럭 찾기
    if ( (bp = find_fit(adjust_size)) != NULL ) {
        place(bp,adjust_size);
        return bp;
    }
    extend_size = MAX(adjust_size, CHUNKSIZE);
    if ( (bp = extend_heap(extend_size / DSIZE) ) == NULL)
        return NULL;
    //printf("사이즈 부족으로 Chuncksize %d 연장\n", extend_size);
    place(bp,adjust_size);
    return bp;
}

// first - fit 
static void *find_fit(size_t adjust_size) {
    // 처음부터 찾아가면서, 에필로그 블럭전까지 찾아감 - 가용블럭 부터
    void *bp ;

    for (bp = free_listp; GET_ALLOC(HDRP(bp)) == 0; bp = GET_SUCC(bp)) {
        if (GET_SIZE(HDRP(bp)) >= adjust_size) 
            return bp;
    }
    return NULL;
}

static void place(void *bp,size_t adjust_size){
    size_t csize = GET_SIZE(HDRP(bp)); // 할당가능한 가용 블럭의 후보 주소

    delete_free_list(bp); // 할당 전에 가용블록의 주소를 없앰

    // 분할이 가능한 경우 (가용블록의 시작 부분)
    if (csize - adjust_size >= (2*DSIZE)) {
        
        // 가용 리스트의 가장 처음에 넣음
        // 앞으로는 할당
        PUT(HDRP(bp),PACK(adjust_size, 1));
        PUT(FTRP(bp),PACK(adjust_size, 1));
        //printf("block 위치 %p | 들어갈 list의 크기 %d | 넣어야할 size 크기 %d\n", (unsigned int *)bp, csize, adjust_size);
        
        // 뒤는 가용블록
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp),PACK(csize - adjust_size,0));
        PUT(FTRP(bp),PACK(csize - adjust_size,0));
          
        put_free_list(bp); // 새로 할당된 블록을 가용리스트 가장 앞에 붙임
        //printf("free block 위치 %p | 나머지 block 크기 %d\n", (unsigned int *)bp, csize - adjust_size);
    }
    // 분할이 불가능한 경우 (남은 부분은 padding)
    else {
    PUT(HDRP(bp),PACK(csize,1));
    PUT(FTRP(bp),PACK(csize,1));
    //printf("block 위치 %p | padding으로 넣은 size 크기 %d\n", (unsigned int *)bp, csize);
    }
}

void put_free_list(void *bp) {
    // 새로 생성된 가용블록을 맨 앞에 추가
    GET_SUCC(bp) = free_listp; // bp의 succ은 루트가 가리키던 블록
    GET_PRED(bp) = NULL; 
    GET_PRED(free_listp) = bp; // 루트였던 블록의 이전가용블록을 추가된 블록에 연결
    free_listp = bp; // 루트를 현재 블록으로 변경한다. 
}

// 가용블럭 중에 할당후보로 선택된 블록의 주소를 삭제
void delete_free_list(void *bp) {
    
    // 만약 첫번째로 시작한다면 가용블록을 제거 
    if (bp == free_listp) {

        GET_PRED(GET_SUCC(bp)) = NULL;
        free_listp = GET_SUCC(bp); // bp는 다음 블록의 주소를 가리킴 
    }
    else { // 앞 뒤 가용블록이 있는 경우
        GET_SUCC(GET_PRED(bp)) = GET_SUCC(bp);
        GET_PRED(GET_SUCC(bp)) = GET_PRED(bp);

    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    // 주소값의 사이즈의 정보를 가져옴
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));

    coalesce(bp); // 앞 뒤가 가용이면 연결
}

// free 블록 반환 후 경계 태그 연결사용해서 상수시간에 인접 가용블록과 통합
static void *coalesce(void *bp) {

    // (헤더) 이전 할당 여부 사이즈 - 이전에 할당한 여부를 헤더의 주소에서 가져오기
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    // (푸터) 이후 할당 여부 사이즈 - 이후에 할당한 여부를 푸터의 주소에서 가져오기
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    // 현재 반환할 주소
    size_t size = GET_SIZE(HDRP(bp));
    // case 1 -> 이전, 이후 다음 블록이 모두 할당된 경우
    if (prev_alloc && next_alloc) {
        // return bp;
    }
    // case 2 -> 이전은 할당 다음은 가용
    else if (prev_alloc && !next_alloc) {
        delete_free_list(NEXT_BLKP(bp)); // 통합하기 전 이전의 가용블록은 제거
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        // 현재 블록의 헤더는 현재 + 다음 블록의 크기를 합한 것
        PUT(HDRP(bp), PACK(size, 0));
        // 다음 블록의 푸터는 현재 + 다음 블록의 크기를 합한 것
        PUT(FTRP(bp), PACK(size, 0));
    }
    // case 3 -> 이전은 가용 다음은 할당
    else if (!prev_alloc && next_alloc) {
        delete_free_list(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    // case 4 -> 모두 가용
    else {
        delete_free_list(NEXT_BLKP(bp));
        delete_free_list(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    //next_bp = bp;
    put_free_list(bp); // 통합한 뒤 다시 넣어준다. 
    return bp;

}
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size)
{
    void *oldptr = bp;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDRP(bp));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize); // 복사받을 메모리, 복사할 메모리, 길이
    mm_free(oldptr);

    return newptr;
}

// void *mm_realloc(void *ptr,size_t size) {
//     void *oldptr = ptr;
//     void *newptr;

//     size_t originsize = GET_SIZE(HDRP(oldptr));
//     size_t newsize = size + DSIZE;

//     if (newsize <= originsize) {
//         return oldptr;
//     } else {
//         size_t addsize = originsize + GET_SIZE(HDRP(NEXT_BLKP(oldptr)));
//         if (GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) == 0 && (newsize <= addsize)) {
//             PUT(HDRP(oldptr),PACK(addsize,1));
//             PUT(FTRP(oldptr),PACK(addsize,1));
//             return oldptr;
//         } else {
//             newptr = mm_malloc(newsize);
//             if (newptr == NULL)
//                 return NULL;
//             memmove(newptr,oldptr,newsize); // 출발지:oldptr에서 목적지:newptr로 newsize길이만큼 복사이동
//             mm_free(oldptr);
//             return newptr;
//         }
//     }
// }