//------------------------------------------------------------------------------
//
// memtrace
//
// trace calls to the dynamic memory manager
//
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memlog.h>
#include <memlist.h>

//
// function pointers to stdlib's memory management functions
//
static void *(*mallocp)(size_t size) = NULL;
static void (*freep)(void *ptr) = NULL;
static void *(*callocp)(size_t nmemb, size_t size);
static void *(*reallocp)(void *ptr, size_t size);

//
// statistics & other global variables
//
static unsigned long n_malloc  = 0;
static unsigned long n_calloc  = 0;
static unsigned long n_realloc = 0;
static unsigned long n_allocb  = 0;
static unsigned long n_freeb   = 0;
static item *list = NULL;

//
// init - this function is called once when the shared library is loaded
//
__attribute__((constructor))
void init(void)
{
  char *error;

  LOG_START();

  // initialize a new list to keep track of all memory (de-)allocations
  // (not needed for part 1)
  list = new_list();

  // ...
}

//
// fini - this function is called once when the shared library is unloaded
//
__attribute__((destructor))
void fini(void)
{
  // ...
  unsigned long avg=n_allocb/(n_malloc+n_calloc+n_realloc);
  LOG_STATISTICS(n_allocb, avg, n_freeb);
  
  item *it=list;
  int first=1;
  while(it!=NULL){
    if(it->cnt){
      if(first){first=0;LOG_NONFREED_START();}
      LOG_BLOCK(it->ptr,it->size,it->cnt);
    }
    it=it->next;
  }
  // free list (not needed for part 1)
  free_list(list);
  LOG_STOP();
}

void *malloc(size_t size){
  mallocp=dlsym(RTLD_NEXT,"malloc");
  void *ptr=mallocp(size);
  alloc(list,ptr,size);
  n_allocb+=size;
  LOG_MALLOC(size,ptr);
  n_malloc+=1;
  return ptr;
}

void *calloc(size_t nmemb,size_t size){
  callocp=dlsym(RTLD_NEXT,"calloc");
  void *ptr=callocp(nmemb,size);
  alloc(list,ptr,size*nmemb);
  LOG_CALLOC(nmemb,size,ptr);
  n_calloc++;
  n_allocb+=(size*nmemb);
  return ptr;
}

void *realloc(void*ptr,size_t size){
  reallocp=dlsym(RTLD_NEXT,"realloc");
  void *new_ptr=reallocp(ptr,size);
  item *it=find(list,ptr);
  if(it!=NULL && it->size!=0){
    dealloc(list,ptr);
    alloc(list,new_ptr,size);
    n_freeb+=it->size;
  }else{
    alloc(list,new_ptr,size);
  }
  LOG_REALLOC(ptr,size,new_ptr);
  n_realloc++;
  n_allocb+=(size);
  return new_ptr;
}

void free(void *ptr){
  freep=dlsym(RTLD_NEXT,"free");
  
  LOG_FREE(ptr);
  item *it=find(list,ptr);
  if(it!=NULL && it->cnt!=0){
    dealloc(list,ptr);
    n_freeb+=it->size;
    
  }
  freep(ptr);
  
}