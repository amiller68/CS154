/*
 * mm.c
 *
 * This is the only file you should modify.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

/*Copied mm-implicit mcros for now... */
#define WIZE       4       /* word size (bytes) */  
#define DSIZE       8       /* doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* initial heap size (bytes) */
#define PTR_OVERHEAD  (2 * DZISE)       /* overhead of ptrs (bytes) */
#define HF_OVEREHAD	(2 * WSIZE) /* overhead of H and F (bytes) */
#define OVERHEAD	(PTR_OVERHEAD + HF_OVERHEAD)

#define MAX(x, y) ((x) > (y)? (x) : (y))  

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
/* NB: this code calls a 32-bit quantity a word */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/*Read and write 8 byte address at p*/
#define GET_ADR(p)  (*(void *)(p))
#define PUT_ADR(p, adr) (* (void *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7) //only care about mult of 8
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((void *)(bp) - WSIZE)  
#define FTRP(bp)       ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute the addresses of the prev and next free blocks */
#define GET_PREV(bp)	GET_ADR(bp)
#define GET_NEXT(bp)	GET_ADR(bp + DSIZE)

/* Given block bp, put addr into its prev/next ptr space */
#define PUT_PREV(bp, adr)	PUT_ADR(bp, adr)
#define PUT_NEXT(bp, adr)	PUT_ADR(bp, adr + DSIZE)


/* Given a block ptr bp, shift bps prev and next ptrs to a diff block */
#define SHIFT_PREV(bp, dest) PUT_PREV(dest, (GET_PREV(bp)))
#define SHIFT_NEXT(bp, dest) PUT_NEXT(dest, (GET_NEXT(bp)))


/* Given block ptr bp, compute address of next and previous blocks */
//ret ptr is to payload
#define NEXT_BLKP(bp)  ((void *)(bp) + GET_SIZE(((void *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((void *)(bp) - GET_SIZE(((void *)(bp) - DSIZE)))
/* $end mallocmacros */

/*End of mm-implicit macros...*/
static char *heap_listp = 0;  /* pointer to first block */

void *root = NULL;

#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

//Can a block ptr take this size?
#define FIT(bp, asize) ((GET_SIZE(HDRP(bp))) >= (asize) ? 1 : 0 )
/* THINGS TO KEEP IN MIND
	Largest size of the heap is 32 bytes; address size will never be great than 2^32	

*/

/*
 * Initialize: return -1 on error, 0 on success.
 */

static void *extend_heap(size_t words); 
static void place(void *bp, size_t asize); 
static void *find_fit(size_t asize); 
static void *coalesce(void *bp); 
static void printblock(void *bp);  
static void checkblock(void *bp); 

int mm_init(void)
{
	//initial heap size should be be the smallest possible BLOCK size (6W) plus needed padding (2W) plus epilogue header(1)
	if(( heap_listp = mem_sbrk(9*WSIZE) ) == NULL )
		return -1;
		//If smallest possible heap not possible -> FAILURE

	//Pad out for alignment
	PUT(heap_listp, 0); 
	heap_listp += WSIZE;
	PUT(heap_listp, 0);
	heap_listp += WSIZE;

	//HEADER
	PUT( heap_listp, PACK(OVERHEAD, 0) );
	heap_listp += WSIZE;

	//make root point to the start of new payload
	root = heap_listp;	

	//place NULL addresses at tags
	PUT_PREV(heap_listp, NULL); //PREV
	PUT_NEXT(heap_listp, NULL); //NEXT
	heap_listp += 2*DSIZE;

	//FOOTER
	PUT(heap_listp, PACK(OVERHEAD, 0) );
	heap_listp += WSIZE;

	//Epilogue Header for extension
	PUT(heap_listp, PACK(0, 1) );

	if( extend_heap(CHUNKSIZE/WSIZE) == NULL )
		return -1;
	
	return 0;
}



void *mm_malloc (size_t size) {
	size_t asize;      /* adjusted block size */
	size_t extendsize; /* amount to extend heap if no fit */
	void *bp;      
	if (heap_listp == 0){
		mm_init();
	}

	/* Ignore spurious requests */
	if (size <= 0)
		return NULL;

	/* Adjust block size to include overhead and alignment reqs. */
	if (size <= PTR_OVERHEAD)
		asize = OVERHEAD;
	else
		//		HF Pad		  Align along DWord
		asize = HF_OVERHEAD + ALIGN(size);

	/* Search the free list for a fit */
	if ((bp = find_fit(asize)) != NULL) {
		bp = place(bp, asize);
		return bp;
	}

	/* No fit found. Get more memory and place the block */
	extendsize = MAX(asize,CHUNKSIZE);
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
		return NULL;
	
	bp = place(bp, asize);
	return bp;
}	


/*
 * free
 */
void mm_free (void *bp) {
    //Dont free NULL ptrs
	if (!bp) return;	

/*	
	//Holdover from mm-implicit: is it necessary
	if (!heap_listp){ 
		mm_init(); 
	} 
*/

	size_t size = GET_SIZE(HDRP(bp)); 

	PUT(HDRP(bp), PACK(size, 0)); 
	PUT(FTRP(bp), PACK(size, 0));
	
	PUT_PREV(bp, NULL);
	PUT_NEXT(bp, root);
	root = bp;

	coalesce(bp);
	return;
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *mm_realloc(void *bp, size_t size) {
	if (!bp)
	{
		return mm_malloc(size);
	}
	
	else if (!size)
	{
		free(oldptr);
		return NULL;
	}
	
	else
	{
		void *hp = HDRP(bp);
		//actual size a requested realloc would take up
		size_t asize = ALIGN(size) + HF_OVERHEAD;
		size_t bsize = GET_SIZE(hp);
		if(bsize == asize)
			return oldptr;
		else if( bsize > asize )
		{
			PUT(hp, PACK(asize, 1));
			PUT(FTRP(bp), PACK(asize, 1));
			
			//Put header and footers on leftover for FREE
			PUT(HDRP(bp + asize), PACK( (bsize - asize), 0 ));
			PUT(FTRP(bp + asize), PACK( (bsize - asize ), 0));
			free(bp + asize);
			return bp;
		}

		else /* Extending malloc area beyong available space */
		{
			//copy data to a bufffer
			char buff[size];
			for(int i = 0; i < size; i++)
				buff[i] = bp + i;
			free(bp);
			bp = malloc(size);
			for(i = 0; i < size; i ++)
				*(bp + i) = buff[i];
			
			return bp;
		}
	}
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *mm_calloc (size_t nmemb, size_t size) {
	//Aligned in mm_malloc
	size_t tot_size = nmemb * size;
	void *bp;

	bp = mm_malloc(tot_size);
	for( int i = 0; i < tot_size; i++)
	{
		*(bp + i) = 0;
	}

	return bp;
}


/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p) {
    return p < mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
}

/*
 * mm_checkheap
 */
void mm_checkheap(int verbose) {
}

static void *place(void *bp, size_t asize)
{
	void *hp = HDRP(bp);	
	size_t bsize = GET_SIZE(hp);

	//if block bigger than needed
	//No need to mess with pointers!
	if( bsize > asize )
	{
		//make the head and foot reflect the remainder
		PUT(hp, PACK((bsize - asize), 0));
		PUT(FTRP(bp), PACK((bsize - asize), 0));
		//inc by remainder, malloc address
		bp += bsize - asize;
		//get header of new malloc address
		hp = HDRP(bp);
		PUT(hp, PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
	}

	else 
	{
		//mark block as allocated
		PUT(hp, PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));

		void *prev_alloc = GET_PREV(bp);
		void *next_alloc = GET_NEXT(bp);

		if( prev_alloc && next_alloc ) /* Case 1: middle of Free List */
		{
			SHIFT_PREV(bp, next_alloc);
			SHIFT_NEXT(bp, prev_malloc);
		}

		else if( prev_alloc ) /* Case 2: End of Free list */
		{
			PUT_NEXT( prev_alloc, NULL )
		}

		else if( next_alloc ) /* Case 3: Start of free list */
		{
			root = next_alloc;
		}
		
		else /* Case 4, Init Condition */ 
		{
			root = NULL;
		}
	}

	return bp;
}

static void *findfit(size_t asize)
{
	void *bp = root;
	while(bp){
		if(FIT(bp, asize)) break;
		bp = GET_NEXT(bp);
	}

	return bp;
}

//Allocate by words
static void *extend_heap(size_t words)
{
	void *bp;
	size_t size;
	void *return_ptr;

	/* Allocate in mutliples of eight to  maintain alignment */
	size = (words % 8) ? (words+2) * WSIZE : words * WSIZE;
	if ((bp = mem_sbrk(size)) == NULL)
  		return NULL;

	Dont go above MAX heap size
	if( mem_heapsize() + size > CHUNKSIZE)
		return NULL;
	
	/* Initialize free block header/footer and the epilogue header */
	PUT(HDRP(bp), PACK(size, 0));         /* free block header */
	PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */

	//PREV
	PUT_PREV(bp, NULL); //NEW block, put at ebggining of list
	//NEXT
	PUT_NEXT(bp, root); //make next block old root val
	//root points to new block
	root = bp;

	/* Coalesce if the previous block was free */
	return_ptr = coalesce(bp);
	mm_checkheap(0);
	return return_ptr;
}

/*
 * malloc
 */

static void *coalesce(void *bp)
{
	//block before bp in memory
	void *prev_block = PREV_BLKP(bp);
	//block after bp in memory
	void *next_block = NEXT_BLKP(bp);

	size_t prev_alloc = GET_ALLOC(FTRP(prev_block));
	size_t next_alloc = GET_ALLOC(HDRP(next_block));
	size_t size = GET_SIZE(HDRP(bp));

	if (prev_alloc && next_alloc) {            /* Case 1 */
  		return bp;
	}

	if (!next_alloc) {      /* Case 2 */
  		//Move the PREV pointer of next_block to its follwoing free block
		SHIFT_PREV( next_block , GET_NEXT(next_block) );
		//Move the NEXT ptr of next_block to its preceding free block
		SHIFT_NEXT( next_block , GET_PREV(next_block) );
		//Dont need to mess with bp pointers because should be done in free, extend
		size += GET_SIZE(HDRP(next_block));
	}

	if (!prev_alloc) {      /* Case 3 */
  		//Move the PREV pointer of next_block to its follwoing free block
		SHIFT_PREV( prev_block , GET_NEXT(prev_block) );
		//Move the NEXT ptr of next_block to its preceding free block
		SHIFT_NEXT( prev_block , GET_PREV(prev_block) );

		SHIFT_NEXT( bp, prev_block );
		SHIFT_PREV( bp, prev_block );		

		size += GET_SIZE(HDRP(prev_block));
		bp = prev_block;
	}
	
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size,0));

	return bp;
}
