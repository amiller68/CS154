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

int flag = 1;
/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
//#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) (printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

/*Copied mm-implicit mcros for now... */
#define WSIZE       4       /* word size (bytes) */  
#define DSIZE       8       /* doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* initial heap size (bytes) */
#define PTR_OVERHEAD  (2 * DSIZE)       /* overhead of ptrs (bytes) */
#define HF_OVERHEAD	(2 * WSIZE) /* overhead of H and F (bytes) */
#define OVERHEAD	(PTR_OVERHEAD + HF_OVERHEAD)
#define INIT_SIZE (CHUNKSIZE - (2*DSIZE)) //Chunk -  back padding (fron pad taken out in init)
#define MAX_HEAP_SIZE ( ((size_t)(2)) << 32 )
#define MAX(x, y) ((x) > (y)? (x) : (y))  

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
/* NB: this code calls a 32-bit quantity a word */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/*Read and write 8 byte address at p*/
//Get Address (DWord), cast to (void *)
#define GET_ADR(p)  	((void *) (*(unsigned long *)(p)))
//Cast void *P to UL ptr, dereference and assign adr (cast to unisgned long)
#define PUT_ADR(p, adr) ( ( *(unsigned long *) (p) ) = ((unsigned long) adr))

/* Read the size and allocated fields from address p | Use for headers and footers*/
#define GET_SIZE(p)  (GET(p) & ~0x7) //only care about mult of 8
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
//Cast void *bp to generic pointer (char *) and perform necessary arthimetic
#define HDRP(bp)       ((char *)(bp) - WSIZE)  
#define FTRP(bp)       ((char  *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute the addresses of the prev and next free blocks */
#define GET_PREV(bp)	GET_ADR(bp)
#define GET_NEXT(bp)	GET_ADR(bp + DSIZE)

/* Given block bp, put addr into its prev/next ptr space */
#define PUT_PREV(bp, adr)	PUT_ADR(bp, adr)
#define PUT_NEXT(bp, adr)	PUT_ADR(bp + DSIZE, adr)


/* Given a block ptr bp, shift bps prev and next ptrs to a diff block */
#define SHIFT_PREV(bp, dest) PUT_PREV(dest, (GET_PREV(bp)))
#define SHIFT_NEXT(bp, dest) PUT_NEXT(dest, (GET_NEXT(bp)))


/* Given block ptr bp, compute address of next and previous blocks */
//ret ptr is to payload
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(( (char *)(bp) - WSIZE )))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(( (char *)(bp) - DSIZE )))
/* $end mallocmacros */
static size_t align_buff = 0;
static void *heap_listp = 0;  /* pointer to first block */
static void *root = 0;

#define ALIGNMENT 8


/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)
#define WALIGN(p) (((size_t)(p) + 3) & ~0x3)

//Can a block ptr take this size?
#define FIT(bp, asize) ( ((GET_SIZE(HDRP(bp))) >= (asize) ) ? 1 : 0 )
/* THINGS TO KEEP IN MIND
	Largest size of the heap is 32 bytes; address size will never be great than 2^32	

*/

/*
 * Initialize: return -1 on error, 0 on success.
 */

static void *extend_heap(size_t words); 
static void *place(void *bp, size_t asize); 
static void *find_fit(size_t asize); 
static void *coalesce(void *bp); 
///static void print_block(const void *bp);  
//static void checkblock(void *bp); 
static void test_list(int thresh);



int mm_init(void)
{
	size_t pad;
	dbg_printf("Beginning Init...\n");	
	
	if(( heap_listp = mem_sbrk(CHUNKSIZE) ) == NULL )
		return -1;
		//If smallest possible heap not possible -> FAILURE
	
	dbg_printf("Intial Chunk Allocated\n");	
	dbg_printf("Starts at: %p\n", heap_listp);
	dbg_printf("Ends at: %p\n", heap_listp + CHUNKSIZE);
	
	pad = (size_t) heap_listp;
	heap_listp = (void *) ALIGN(heap_listp);
	pad = ((size_t) heap_listp) - pad;
	heap_listp += WSIZE;
	align_buff = 8 - pad;
	
	dbg_printf("Front Padding: %zu\n", pad);
	dbg_printf("Align Buff: %zu\n", align_buff);
	dbg_printf("Aligned HLP: %p\n", heap_listp);

	//HEADER
	PUT( heap_listp, PACK(INIT_SIZE, 0) );
	heap_listp += WSIZE;
	//make root point to the start of new payload
	root = heap_listp;	
	//place NULL addresses at tags
	PUT_PREV(heap_listp, 0); //PREV
	PUT_NEXT(heap_listp, 0); //NEXT
	//FOOTER
	PUT( FTRP(heap_listp), PACK(INIT_SIZE, 0) );
	//Need epilogue for edge coalessing
	PUT( HDRP((NEXT_BLKP(heap_listp))), PACK(0, 1) );
//	dbg_printf("***Init Block***\n");
//	print_block(heap_listp);
//	dbg_printf("***EP Head***\n");
//	dbg_printf("Starts at: %p\n", HDRP((NEXT_BLKP(heap_listp))));
	dbg_printf("Heap Initialized!\n");
	test_list(100);
	return 0;
}



void *mm_malloc (size_t size) {
	size_t asize;      /* adjusted block size */
	size_t extendsize; /* amount to extend heap if no fit */
	void *bp;
	dbg_printf("Allocating %zu Bytes...\n", size);	
	if (heap_listp == 0)
		mm_init();
	/* Ignore spurious requests */
	if (size <= 0)
		return NULL;
	/* Adjust block size to include overhead and alignment reqs. */
	if (size <= PTR_OVERHEAD)
		asize = OVERHEAD;
	else
		asize = HF_OVERHEAD + ALIGN(size);
	dbg_printf("Resizing to %zu Bytes\n", asize);	
	/* Search the free list for a fit */
	if ((bp = find_fit(asize)) != NULL) {
		dbg_printf("***Found FIT***\n");
//		print_block(bp);
		bp = place(bp, asize);
		dbg_printf("Block placed at: %p\n", bp);
//		dbg_printf("***New Block**&\n");
//		print_block(bp);
		return bp;
	}
	dbg_printf("NO FIT found\n");
	/* No fit found. Get more memory and place the block */
	extendsize = MAX(asize,CHUNKSIZE);
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL){
		dbg_printf("FAILED to extend heap\n");
		return NULL;
	}	
	bp = place(bp, asize);
	dbg_printf("Block placed at: %p\n", bp);
//	dbg_printf("**BLOCK**\n");
//	print_block(bp);
//	test_list(100);
	return bp;
}	


/*
 * free
 */
void mm_free (void *bp) {
    //Dont free NULL ptrs
	dbg_printf("Freeing ptr: %p\n", bp);
	if (!bp) return;	
	if (!heap_listp){ 
		mm_init(); 
	} 
	unsigned int size = GET_SIZE(HDRP(bp)); 
	dbg_printf("	Size: 0x%x\n", size);
	PUT(HDRP(bp), PACK(size, 0)); 
	PUT(FTRP(bp), PACK(size, 0));
	
	PUT_PREV(bp, 0);
	PUT_NEXT(bp, root);
	if(root)
		PUT_PREV(root, bp);
	root = bp;
//	dbg_printf("***NEW free BLK***\n");
//	print_block(bp);
	bp = coalesce(bp);
//	dbg_printf("***Coaled New Block***\n");
//	print_block(bp);
//	mm_checkheap(0);
//	test_list(100);
	return;
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *mm_realloc(void *bp, size_t size) {
	dbg_printf("Reallocating blok...\n");
//	print_block(bp);
	dbg_printf("New Size: %zu\n", size);
	if (bp == NULL)
	{
		dbg_printf("Malloc Condition!\n");
		return mm_malloc(size);
	}
	else if (!size)
	{
		dbg_printf("Free Condition!\n");
		mm_free(bp);
		return NULL;
	}
	else
	{
		void *hp = HDRP(bp);
		//actual size a requested realloc would take up
		size_t asize;
		if (size <= PTR_OVERHEAD)
			asize = OVERHEAD;
		else
			asize = HF_OVERHEAD + ALIGN(size);
		size_t bsize = GET_SIZE(hp);
		int rem = (int) bsize - asize;
		if(bsize == asize)
			return bp;
		else if( rem >= 24 )
		{
			dbg_printf("Shrinking block to new size...\n");
			PUT(hp, PACK(asize, 1));
			PUT(FTRP(bp), PACK(asize, 1));
			//Put header and footers on leftover for FREE
			PUT(HDRP(bp + asize), PACK( (bsize - asize), 0 ));
			PUT(FTRP(bp + asize), PACK( (bsize - asize ), 0));
			mm_free(bp + asize);
			return bp;
		}
		else if( rem >= 0 )
		{
			dbg_printf("Moving block to a better fit...\n");			
			size_t buff_size = size;
			char buff[buff_size];
			int i;
			for(i = 0; i < buff_size; i++)
				buff[i] = *((char *) bp + i);
			mm_free(bp);
			//Malloc appropriate space for new blcok
			bp = mm_malloc(asize - HF_OVERHEAD);
			for(i = 0; i < buff_size; i++)
				*((char *) bp + i) = buff[i];
//			test_list(100);
			return bp;
		}
		else /* Extending malloc area beyong available space */
		{
			dbg_printf("Mallocing for extra space...\n");
			//# of bytes to copy
			//Size of the block being moved - HF_OVERHEAD
			size_t buff_size = bsize - HF_OVERHEAD;
			char buff[buff_size];
			int i;
			for(i = 0; i < buff_size; i++)
				buff[i] = *((char *) bp + i);
			mm_free(bp);
			//Malloc appropriate space for new blcok
			bp = mm_malloc(asize - HF_OVERHEAD);
			for(i = 0; i < buff_size; i++)
				*((char *) bp + i) = buff[i];
//			test_list(100);
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
		*((char *) bp + i) = 0;
	}
//	test_list(100);
	return bp;
}

/*
static void print_block(const void *bp)
{
	if(!bp)
	{
		dbg_printf("NULL ptr!\n");
		return;
	}

	dbg_printf("Block at Addr: %p\n", bp);
	dbg_printf("Allocated: %d\n", GET_ALLOC(HDRP(bp)));
	if(!GET_ALLOC(HDRP(bp)))
	{
		dbg_printf("	PREV Free: %p\n", GET_PREV(bp));
		dbg_printf("	NEXT FRee: %p\n", GET_NEXT(bp));
	}
	dbg_printf("Size: 0x%x\n", GET_SIZE(HDRP(bp)));
	dbg_printf("HDRP: %p\n", HDRP(bp));
	dbg_printf("FTRP: %p\n", FTRP(bp));
	dbg_printf("***********\n");
	return;
}
*/

/*
 * mm_checkheap
*/
void mm_checkheap(int verbose) {
	return;
}

static void *place(void *bp, size_t asize)
{
	void *hp = HDRP(bp);	
	size_t bsize = GET_SIZE(hp);
	//if block bigger than needed
	//And enough room for both block and base block
	//No need to mess with pointers!
	int rem = (int) bsize - asize;
	if( rem >= 24 )
	{
		//make the head and foot reflect the remainder
		PUT(hp, PACK(rem, 0));
		PUT(FTRP(bp), PACK(rem, 0));
		//inc by remainder, malloc address
		bp += rem;
		//get header of new malloc address
		hp = HDRP(bp);
		PUT(hp, PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
	}
	//If b !> a, and bp palced there, must mean they are equal
	//or rem not big enouggh
	else 
	{
		//mark block as allocated
		PUT(hp, PACK(bsize, 1));
		PUT(FTRP(bp), PACK(bsize, 1));
		// FREE LIST POINTERS
		void *prev_alloc = GET_PREV(bp);
		void *next_alloc = GET_NEXT(bp);
		if( prev_alloc )
			SHIFT_NEXT(bp, prev_alloc);
		if( next_alloc )
			SHIFT_PREV(bp, next_alloc);
		if( root == bp )
			root = next_alloc;
	}
	return bp;
}

static void *find_fit(size_t asize)
{
	dbg_printf("Looking for fit of size: 0x%zx\n", asize);
	dbg_printf("Root: %p\n", root);
	void *bp = root;
	while(bp){
		dbg_printf("Looking at: %p...\n", bp);
		dbg_printf("	size = 0x%x\n", GET_SIZE((HDRP(bp))));
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
	/* Every word needs a pair */
	//Not sure if I need this but ok
	size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
	size = ((size + CHUNKSIZE - 1) / CHUNKSIZE) * CHUNKSIZE;
	dbg_printf("Extending heap by %zu Bytes...\n", size);	
	//Dont go above MAX heap size
	if( mem_heapsize() + size > MAX_HEAP_SIZE)
	{
		dbg_printf("Too much Extension!\n");
		return NULL;
	}
	dbg_printf("Extending From: %p\n", mem_heap_hi() + 1);
	if ((bp = mem_sbrk(size)) == NULL)
  	{
		dbg_printf("Failed to Extend!\n");
		return NULL;
	}
	dbg_printf("Heap now ends at: %p\n", mem_heap_hi() + 1);
	//Dec to payload addr
	bp -= align_buff;
	dbg_printf("Placing H&F\n");
	/* Initialize free block header/footer and the epilogue header */
	PUT(HDRP(bp), PACK(size, 0));         /* free block header (from previous epilogue */
	PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
	dbg_printf("Placing EpHead at %p\n", NEXT_BLKP(bp) - WSIZE);
	//prime epilogue header plus 
	PUT( (NEXT_BLKP(bp) - WSIZE), PACK(0, 1) );
	dbg_printf("	EP ALLOC: %d\n", GET_ALLOC((NEXT_BLKP(bp) - WSIZE)));
	dbg_printf("	EP SIZE: %d\n", GET_SIZE((NEXT_BLKP(bp) - WSIZE)));

	dbg_printf("Placing in free list\n");
	PUT_PREV(bp, 0);
	PUT_NEXT(bp, root);
	if(root)
		PUT_PREV(root, bp);
	root = bp;

//	dbg_printf("**New Free Block***\n");
//	print_block(bp);

	/* Coalesce if the previous block was free */
	return_ptr = coalesce(bp);
	return return_ptr;
}

/*
 * malloc
 */

static void *coalesce(void *bp)
{

	dbg_printf("Coalescing Block...\n");
	//block before bp in memory
	void *prev_block = PREV_BLKP(bp);
	//block after bp in memory
	void *next_block = NEXT_BLKP(bp);

	size_t prev_alloc = GET_ALLOC(HDRP(prev_block));
	size_t next_alloc = GET_ALLOC(HDRP(next_block));
	size_t size = GET_SIZE(HDRP(bp));

//	dbg_printf("PREV ALLOC: %zu\n", prev_alloc);
//	if(!prev_alloc){
//		dbg_printf("***PREV Free BLK***\n");
//		print_block(prev_block);
//	}

//	dbg_printf("NEXT ALLOC: %zu\n", next_alloc);
//	if(!next_alloc){
//		dbg_printf("***NEXT Free BLK***\n");
//		print_block(next_block);
//	}

	if (prev_alloc && next_alloc) {            /* Case 1 */
  		dbg_printf("CASE 1\n");
		return bp;
	}

	if (!next_alloc) {      /* Case 2 */

		dbg_printf("CASE 2\n");  		
		
		if( GET_NEXT(next_block) ) // IF next_blk is not at end of free list
			SHIFT_PREV( next_block , GET_NEXT(next_block) ); //Move its prev ptr to next block
			
		//Move the NEXT ptr of next_block to its preceding free block
		//NEver have to worry about coallescing with block at root, taken care of in extend and free
		SHIFT_NEXT( next_block , GET_PREV(next_block) );
		//Dont need to mess with bp pointers because should be done in free, extend
		size += GET_SIZE(HDRP(next_block));
	}

	if (!prev_alloc) {      /* Case 3 */
  		//Move the PREV pointer of next_block to its follwoing free block
		dbg_printf("CASE 3\n");  	
		if( GET_NEXT(prev_block) )	
			SHIFT_PREV( prev_block , GET_NEXT(prev_block) );
		//Move the NEXT ptr of next_block to its preceding free block
		SHIFT_NEXT( prev_block , GET_PREV(prev_block) );
		
		

//		dbg_printf("Shifting down bp ptrs\n");
		SHIFT_NEXT( bp, prev_block );
		SHIFT_PREV( bp, prev_block );
		

		size += GET_SIZE(HDRP(prev_block));
		bp = prev_block;

		if( GET_NEXT(bp) )
			PUT_PREV( GET_NEXT(bp), bp);
	}
	
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size,0));

//	dbg_printf("***Coalesced Block***\n");
//	print_block(bp);

	root = bp;
	return bp;
}


//tests for loops in lists
static void test_list(int thresh)
{
	void *bp = root;
	void *back = GET_PREV(root);
	int c = 0;
	void *test = 0;
	int found = 0;
	while(bp)
	{
		if(bp == test)
		{
			printf("Cycle detected\n");
			printf("Starts at: %p\n", bp);
			found = 1;
			break;
		}
		if( c == thresh )
			test = bp;

		//want to conserve bp in second test
		if( GET_NEXT(bp) == 0 )
			break;
		back = bp;
		bp = GET_NEXT(bp);
		if( GET_PREV(bp) != back){
			printf("NONCONSITENCY\n");
			exit(2);
		}
		c++;
	}
	
	if( found ){
		bp = root;
		c = 0;
		while(bp){
			printf("P : %p\n", bp);
			printf("	prev : %p\n", GET_PREV(bp));
			if (bp == test) c++;
			if ( c == 2) exit(2);
			bp = GET_NEXT(bp);
		} 
		exit(2);
	}
	
	//check for back cylces
	c = 0;
	test = 0;
	while(bp)
	{
		if(bp == test)
		{
			printf("Back cycle detected\n");
			exit(2);
		}
		if( c == thresh )
			test = bp;
		if( (GET_PREV(bp) == 0) && (bp != root) )
		{
			printf("Unexpected end to prev list\n");
			exit(2);
		}
		bp = GET_PREV(bp);
		c++;
	}
	
	return;
}
