/*
    for threads
    defines a block queue and threadpool
*/

#ifndef THREADPOOL_THREADPOOL_H
#define THREADPOOL_THREADPOOL_H
#include <pthread.h>
#include <stdbool.h>
#include "hashtable.h"

// block queue
typedef struct BlockQueue{
    int capacity;
    int front;
    int back;
    int size;
    int* items;

    pthread_mutex_t mutex;
    pthread_cond_t cond;
}BlockQueue;

extern int block_queue_init( BlockQueue* q, int n );
extern int block_queue_enqueue( BlockQueue* q, int socket_fd );
extern int block_queue_dequeue( BlockQueue* q );
extern int block_queue_delete( BlockQueue* q );

/* 
    pool of threads
*/
typedef struct ThreadPool{
    BlockQueue* queue;
    HashTable* ht;
    pthread_t *threads;
    void* (*run)(void*);
    int thread_num;
    bool shutdown;
}ThreadPool;

extern int thread_pool_init( ThreadPool* pool, HashTable* hashtable, int thread_num, int queue_length, void* (*run)(void*) );
extern int thread_pool_start( ThreadPool* pool );
extern int thread_pool_add( ThreadPool* pool, int socket_fd );
extern int thread_pool_shutdown( ThreadPool* pool );
extern int thread_pool_delete( ThreadPool* pool);

#endif 