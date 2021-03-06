/*
 * (C) Copyright 2011, 2012, 2013, Dario Hamidi <dario.hamidi@gmail.com>
 *
 * This file is part of Challoc.
 * 
 * Challoc is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Challoc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Challoc.  If not, see <http://www.gnu.org/licenses/>.
 */ 
#include "challoc.h"
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <tgmath.h>

#define BITMAPSIZE (sizeof(uint64_t)*8)

struct chunk_allocator {
     size_t n_chunks;            /* number of chunks this allocator holds */
     size_t chunk_size;          /* size of single chunk in bytes         */
     size_t free_chunk;          /* Number of free Chunks               */
     ChunkAllocator* next;       /* next allocator, if this one is full         */
     uint64_t * chunks;     /* bit map of free blocks bit is zero when free     */
     void * memory;      /* challoc returns memory from here      */
};

static uint8_t getFreeChunkBit(size_t * chunks, size_t idx){
     size_t byte_sel, bit_sel;

     byte_sel = idx/BITMAPSIZE; /* which byte */
     bit_sel  = idx - (byte_sel * BITMAPSIZE); /* which bit */
     return (chunks[byte_sel]>>bit_sel)&0x01;
}

static void setFreeChunkBit(size_t * chunks, size_t idx, size_t val){
     size_t byte_sel, bit_sel;
     
     byte_sel = idx/BITMAPSIZE;
     bit_sel  = idx - (byte_sel * BITMAPSIZE);
     /* shift bit to correct location */
     val = (val&0x01)<<bit_sel;
     /* always clear bit then || in the val */
     chunks[byte_sel] = (chunks[byte_sel] & ~(1<<bit_sel) ) | val;
}

static size_t get_chunkidx_from_pointer(ChunkAllocator * root, void* ptr){
     //TODO: Saftey Check
/*     fprintf(stderr,"chunkIdx - mem: %lu, ptr: %lu, ptr-mem: %u, chunksize: %u, idx: %u\n",
               root->memory, ptr, root->memory-ptr, ptr-root->memory, root->chunk_size, (ptr-root->memory)/root->chunk_size);
*/
     return (ptr-root->memory)/root->chunk_size;

}

static
ChunkAllocator* get_first_allocator_with_free_chunk(ChunkAllocator* start) {
   ChunkAllocator* iter = NULL;

   for (iter = start; iter; iter = iter->next) {
      if (iter->free_chunk > 0) {
         return iter;
      }

      /* all allocators are full, so we append a new one and return
       * it */
      if (iter->next == NULL) {
         iter->next = chcreate(iter->n_chunks,iter->chunk_size);
         return iter->next;
      }

   }

   /* never reached */
   assert(0);
   return NULL;
}

static size_t get_first_free_chunk(ChunkAllocator* root){
     size_t i,byte_sel, bit_sel;
     size_t lookups;
          
     /* find the first free chunk in the allocator */
     byte_sel = (root->n_chunks - root->free_chunk)/BITMAPSIZE; //approx where data will be
     lookups=0;
     for(i=0; i<(root->n_chunks/BITMAPSIZE)+1; i++){
          /* bitmap block has a high bit some where - ie a free chunk */
          if( root->chunks[byte_sel] ){
               for(bit_sel=1; bit_sel != 1ull<<63; bit_sel<<=1){
                    lookups++;
                    if( root->chunks[byte_sel] & bit_sel ){
                         if(((byte_sel*BITMAPSIZE)+bit_sel)>root->n_chunks) continue; //rounding err
                         //fprintf(stderr, "chunk idx: %u, lookups: %u, bit_sel: %u\n",  ((byte_sel*BITMAPSIZE)+bit_sel), i+lookups, bit_sel );
                         return ((byte_sel*BITMAPSIZE)+log2(bit_sel));
                    }
               }
          }
          byte_sel = (byte_sel+1) % ((root->n_chunks/BITMAPSIZE)+1);
     }
     fprintf(stderr, "chunk idx: %u, lookups: %u, bit_sel: %u\n",  ((byte_sel*BITMAPSIZE)+bit_sel), i+lookups, bit_sel );
     return -1; //Should never happen
}

static
ChunkAllocator* get_allocator_from_chunk(ChunkAllocator* start, void * p){
     ChunkAllocator* iter = NULL;
     for (iter = start; iter; iter = iter->next) {
          if( p >= iter->memory && p <= iter->memory+(iter->chunk_size*iter->n_chunks) ){
               return iter;
          }
     }
     return NULL;
}

void* challoc(ChunkAllocator* root) {
     size_t idx;
     void * ptr;
     
     if (!root)
          return NULL;

     root = get_first_allocator_with_free_chunk(root);
     idx = get_first_free_chunk(root);

     if(idx==-1){
          fprintf(stderr,"-1 free chunk, %u\n", root->free_chunk);
     }
     
     root->free_chunk--;
     setFreeChunkBit(root->chunks, idx, 0); /* mark as not free */
     ptr = root->memory+(root->chunk_size * idx );

     //fprintf(stderr,"challoc: %p, %u\n", ptr, idx);
     return ptr;
}

void chfree(ChunkAllocator* root,void* p) {
     ChunkAllocator * found = NULL;
     size_t idx;
     
     if (!root)
          return;

     found = get_allocator_from_chunk(root, p);
     idx = get_chunkidx_from_pointer(found,p);

     //Free the chunk in its allocator
     setFreeChunkBit(found->chunks, idx, 1);
     found->free_chunk++;
     //fprintf(stderr, "chfree: %p\n", p );
}

void chclear(ChunkAllocator* root) {
     ChunkAllocator* iter = NULL;
     size_t i;
     
     if (!root)
          return;

     for (iter = root; iter; iter = iter->next) {
        iter->free_chunk = iter->n_chunks;
        for(i=0; i < (iter->n_chunks/BITMAPSIZE)+1 ; i++){
             iter->chunks[i] = -1 ; /* Free all chunks */
        } 
     }
}

ChunkAllocator* chcreate(size_t n_chunks, size_t chunk_size) {
     ChunkAllocator* s = NULL;
     size_t i;

     s = malloc(sizeof(ChunkAllocator));

     if (!s)
          goto FAIL;

     s->chunks = calloc((n_chunks/BITMAPSIZE)+1,sizeof(uint64_t)); /*BITMAP 1 bit per chunk*/
     fprintf(stderr,"bit map is %u block, %u chunks per block\n", (n_chunks/BITMAPSIZE)+1, BITMAPSIZE );
     if (!s->chunks) goto CLEAR1;

     s->memory = calloc(n_chunks,chunk_size);
     if (!s->memory) goto CLEAR2;
     
     s->n_chunks = n_chunks;
     s->free_chunk = n_chunks;
     s->chunk_size = chunk_size;
     s->next = NULL;
     
     /* Set all chunks free */
     for(i=0; i< (n_chunks/BITMAPSIZE)+1 ; i++){
          s->chunks[i]= SIZE_MAX;
     }

     return s;

  CLEAR1:
     free(s);
  CLEAR2:
     free(s->chunks);
  FAIL:
     return NULL;
}

void chdestroy(ChunkAllocator** root) {
   ChunkAllocator* cur = NULL;
   ChunkAllocator* next = NULL;

   for (cur = *root; cur; cur = next) {
      next = cur->next;

      free(cur->memory);
      free(cur->chunks);
      free(cur);
   }

   *root = NULL;
}

/**
 * TEST
 * has to be included here, otherwise the internals of
 * struct chunk_allocator are not accessible.
 */

#ifdef CHALLOC_TEST
#include <stdio.h>
#include <assert.h>

int main(int argc, char** argv) {
     /* arbitrary list */
     struct point_list {
          int x;
          int y;
          struct point_list *next, *prev;
     } *head, *cur, *next;

     const size_t NODE_COUNT = 100;
     size_t i;
     ChunkAllocator* nodes = chcreate(NODE_COUNT,sizeof(struct point_list));

     assert(nodes                                             );
     assert(nodes->free_chunk    == NODE_COUNT                );
     assert(nodes->n_chunks      == NODE_COUNT                );
     assert(nodes->chunk_size    == sizeof(struct point_list) );
     assert(nodes->next          == NULL                      );

     head = challoc(nodes);
     assert(head);

     head->x = 1;
     head->y = 1;
     head->prev = NULL;
     head->next = NULL;
     
     cur = head;
     next = NULL;

     /* create nodes and do something with them */
     for ( i = 0; i < NODE_COUNT; i++) {
          next = challoc(nodes);

          next->x = cur->x;
          if (cur->prev)
               next->x += cur->prev->x;
          next->y = i;
          
          cur->next = next;
          next->prev = cur;

          cur = next;
     }

     /*
      * by now `nodes' should have a `next' ChunkAllocator
      */

     assert(nodes->next                                      );
     assert(nodes->next->free_chunk == NODE_COUNT - 1     );
     assert(nodes->next->n_chunks      == NODE_COUNT         );
     assert(nodes->next->chunk_size    == nodes->chunk_size  );
     assert(nodes->next->next          == NULL               );

     assert(nodes->free_chunk == 0);

     chdestroy(&nodes);

     assert(nodes == NULL);
     return 0;
}
#endif /* CHALLOC_TEST */
