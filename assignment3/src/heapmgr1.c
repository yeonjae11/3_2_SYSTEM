/*--------------------------------------------------------------------*/
/* heapmgr1.c                                                         */
/* Author: Yeonjae Kim                                                */
/*--------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "chunk.h"

#define FALSE 0
#define TRUE  1

enum {
   MEMALLOC_MIN = 1024,
};

/* g_free_head: point to first chunk in the free list */
static Chunk_T g_free_head = NULL;

/* g_heap_start, g_heap_end: start and end of the heap area.
 * g_heap_end will move if you increase the heap */
static void *g_heap_start = NULL, *g_heap_end = NULL;

#ifndef NDEBUG
/* check_heap_validity:
 * Validity check for entire data structures for chunks. Note that this
 * is basic sanity check, and passing this test does not guarantee the
 * integrity of your code. 
 * Returns 1 on success or 0 (zero) on failure. 
 */
static int
check_heap_validity(void)
{
   Chunk_T w;

   if (g_heap_start == NULL) {
      fprintf(stderr, "Uninitialized heap start\n");
      return FALSE;
   }

   if (g_heap_end == NULL) {
      fprintf(stderr, "Uninitialized heap end\n");
      return FALSE;
   }

   if (g_heap_start == g_heap_end) {
      if (g_free_head == NULL)
         return 1;
      fprintf(stderr, "Inconsistent empty heap\n");
      return FALSE;
   }
   for (w = (Chunk_T)g_heap_start; 
        w && w < (Chunk_T)g_heap_end;
        w = chunk_get_next_adjacent(w, g_heap_start, g_heap_end)) {
      if (!chunk_is_valid(w, g_heap_start, g_heap_end)) 
         return 0;
   }

   for (w = g_free_head; w; w = chunk_get_next_free_chunk(w)) {
      Chunk_T n;

      if (chunk_get_status(w) != CHUNK_FREE) {
         fprintf(stderr, "Non-free chunk in the free chunk list\n");
         return 0;
      }

      if (!chunk_is_valid(w, g_heap_start, g_heap_end))
         return 0;

      n = chunk_get_next_adjacent(w, g_heap_start, g_heap_end);
      if (n != NULL && n == chunk_get_next_free_chunk(w)) {
         fprintf(stderr, "Uncoalesced next chunks\n");
         return 0;
      }

      n = chunk_get_prev_adjacent(w, g_heap_start, g_heap_end);
      if (n != NULL && n == chunk_get_prev_free_chunk(w)) {
         fprintf(stderr, "Uncoalesced prev chunks\n");
         return 0;
      }
   }
   return TRUE;
}
#endif

/*--------------------------------------------------------------*/
/* size_to_units:
 * Returns capable number of units for 'size' bytes. 
 */
/*--------------------------------------------------------------*/
static size_t
size_to_units(size_t size)
{
  return (size + (CHUNK_UNIT-1))/CHUNK_UNIT;
}
/*--------------------------------------------------------------*/
/* get_chunk_from_data_ptr:
 * Returns the header pointer that contains data 'm'. 
 */
/*--------------------------------------------------------------*/
static Chunk_T
get_chunk_from_data_ptr(void *m)
{
  return (Chunk_T)((char *)m - CHUNK_UNIT);
}
/*--------------------------------------------------------------------*/
/* init_my_heap: 
 * Initialize data structures and global variables for
 * chunk management. 
 */
/*--------------------------------------------------------------------*/
static void
init_my_heap(void)
{
   /* Initialize g_heap_start and g_heap_end */
   g_heap_start = g_heap_end = sbrk(0);
   if (g_heap_start == (void *)-1) {
      fprintf(stderr, "sbrk(0) failed\n");
      exit(-1);
   }
}
/*--------------------------------------------------------------------*/
/* delete_chunk_from_free:
 * delete 'c' chunk from free list which contains 'c'
 * Then, connect prev chunk with next chunk.
 * If 'c' is head of free list, next will be head of free list. */
/*--------------------------------------------------------------------*/
static void
delete_chunk_from_free(Chunk_T c)
{
   Chunk_T prev, next;


   prev = chunk_get_prev_free_chunk(c);
   next = chunk_get_next_free_chunk(c);

   chunk_set_next_free_chunk(prev,next);
   chunk_set_prev_free_chunk(next,prev);

   chunk_set_next_free_chunk(c,NULL);
   chunk_set_prev_free_chunk(c,NULL);

   if(g_free_head == c){
      assert(prev == NULL);
      g_free_head = next;
   }
}
/*--------------------------------------------------------------------*/
/* insert_front_free: 
 * Inserts the chunk 'c' at the front of the free list.
 * If the free list is empty, 'c' becomes the head.
 * If 'c' is larger than or equal to the current head, 
 * 'c' is inserted at the front, and the head is updated.
 * Otherwise, 'c' is inserted next to the head.
 * Updates the next and previous pointers of the involved chunks 
 * to maintain the integrity of the free list.
 */
/*--------------------------------------------------------------------*/
static void
insert_front_free(Chunk_T c)
{
   if(g_free_head == NULL){
      g_free_head = c;
      return;
   }
   if(chunk_get_units(g_free_head) <= chunk_get_units(c)){
      chunk_set_next_free_chunk(c,g_free_head);
      chunk_set_prev_free_chunk(g_free_head,c);
      g_free_head = c;
      return;
   }
   Chunk_T next;
   next = chunk_get_next_free_chunk(g_free_head);
   chunk_set_next_free_chunk(g_free_head,c);
   chunk_set_next_free_chunk(c,next);
   chunk_set_prev_free_chunk(next,c);
   chunk_set_prev_free_chunk(c,g_free_head);
}
/*--------------------------------------------------------------------*/
/* split_chunk:
 * Split 'c' into two chunks s.t. the size of one chunk is 'units' and
 * the size of the other chunk is (original size - 'units' - 1).
 * returns the chunk with 'units'
 * Returns the data chunk. */
/*--------------------------------------------------------------------*/
static Chunk_T
split_chunk(Chunk_T c, size_t units)
{
   Chunk_T c2;
   Chunk_T prev;
   size_t all_units;

   assert (c >= (Chunk_T)g_heap_start && c <= (Chunk_T)g_heap_end);
   assert (chunk_get_status(c) == CHUNK_FREE);
   assert (chunk_get_units(c) > units + 2);
   
   all_units = chunk_get_units(c);
   prev = chunk_get_prev_free_chunk(c);
   chunk_set_units(c, all_units - units - 2);
   chunk_set_prev_free_chunk(c,prev);

   assert(chunk_get_prev_free_chunk(c) == prev);
   assert(chunk_is_valid(c,g_heap_start,g_heap_end));

   c2 = chunk_get_next_adjacent(c, g_heap_start, g_heap_end);
   assert(c2 != NULL);
   chunk_set_units(c2, units);
   chunk_set_status(c2, CHUNK_IN_USE);
   return c2;
}
/*--------------------------------------------------------------------*/
/* allocate_more_memory: 
 * Allocate a new chunk which is capable of holding 'units' chunk
 * units in memory by increasing the heap, and return the new
 * chunk. If sbrk returns 0, this function returns NULL.
 * If the last chunk, which has the highest memory, is free,
 * this code coalesces last chunk with new allocated chunk. 
*/
/*--------------------------------------------------------------------*/
static Chunk_T
allocate_more_memory(size_t units)
{
   Chunk_T c, prev;

   if (units < MEMALLOC_MIN)
      units = MEMALLOC_MIN;
   
   /* Note that we need to allocate one more unit for header. */
   c = (Chunk_T)sbrk((units + 2) * CHUNK_UNIT);
   if (c == (Chunk_T)-1){
      return NULL;
   }
      
   
   g_heap_end = sbrk(0);
   chunk_set_units(c, units);
   prev = chunk_get_prev_adjacent(c,g_heap_start,g_heap_end);

   chunk_set_next_free_chunk(c, NULL);
   chunk_set_prev_free_chunk(c, NULL);
   chunk_set_status(c, CHUNK_FREE);

   if(prev != NULL && chunk_get_status(prev) == CHUNK_FREE){
      int all_units = chunk_get_units(prev) + units + 2;
      chunk_set_units(prev, all_units);
      delete_chunk_from_free(prev);
      return prev;
   }

   assert(check_heap_validity());
   return c;
}
/*--------------------------------------------------------------*/
/* heapmgr_malloc:
 * Dynamically allocate a memory capable of holding size bytes. 
 * Substitute for GNU malloc().                                 
 */
/*--------------------------------------------------------------*/
void *
heapmgr_malloc(size_t size)
{
   static int is_init = FALSE;
   Chunk_T c;
   size_t units;
   
   if (size <= 0)
      return NULL;
   
   if (is_init == FALSE) {
      init_my_heap();
      is_init = TRUE;
   }

   /* see if everything is OK before doing any operations */
   assert(check_heap_validity());

   units = size_to_units(size);

   for (c = g_free_head; 
        c != NULL; 
        c = chunk_get_next_free_chunk(c)) {
      if (chunk_get_units(c) >= units) {
         if (chunk_get_units(c) > units + 2) 
            c = split_chunk(c, units);
         else{
            assert (chunk_get_status(c) == CHUNK_FREE);
            chunk_set_status(c, CHUNK_IN_USE);
            delete_chunk_from_free(c);
         }
         assert(check_heap_validity());
         return (void *)((char *)c + CHUNK_UNIT);
      }
   }

   /* allocate new memory */
   c = allocate_more_memory(units);
   if (c == NULL) {
     assert(check_heap_validity());
     return NULL;
   }
   assert(chunk_get_next_free_chunk(c)==NULL);
   assert(chunk_get_prev_free_chunk(c)==NULL);

   assert(chunk_get_units(c) >= units);

   if (chunk_get_units(c) > units + 2){
      insert_front_free(c);
      c = split_chunk(c, units);
   }
   else{
      chunk_set_status(c, CHUNK_IN_USE);
   }
      
   assert(check_heap_validity());
   return (void *)((char *)c + CHUNK_UNIT);

}
/*--------------------------------------------------------------*/
/* heapmgr_free:
 * Releases dynamically allocated memory. 
 * Substitute for GNU free().                                   */
/*--------------------------------------------------------------*/
void
heapmgr_free(void *m)
{
   Chunk_T c, next, prev;
   
   if (m == NULL)
      return;

   /* check everything is OK before freeing 'm' */
   assert(check_heap_validity());

   /* get the chunk header pointer from m */
   c = get_chunk_from_data_ptr(m);
   assert (chunk_get_status(c) != CHUNK_FREE);

   chunk_set_status(c,CHUNK_FREE);
   
   next = chunk_get_next_adjacent(c, g_heap_start, g_heap_end);
   prev = chunk_get_prev_adjacent(c, g_heap_start, g_heap_end);

   if(prev != NULL && chunk_get_status(prev) == CHUNK_FREE){
      delete_chunk_from_free(prev);
      int all_units;
      all_units = chunk_get_units(c) + chunk_get_units(prev) + 2;
      chunk_set_units(prev,all_units);
      c = prev;
   }

   if(next != NULL && chunk_get_status(next) == CHUNK_FREE){
      delete_chunk_from_free(next);
      int all_units;
      all_units = chunk_get_units(c) + chunk_get_units(next) + 2;
      chunk_set_units(c,all_units);  
   }
   chunk_set_prev_free_chunk(c,NULL);
   chunk_set_next_free_chunk(c,NULL);
   insert_front_free(c);

   /* double check if everything is OK */
   assert(check_heap_validity());
}