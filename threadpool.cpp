#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "threadpool.h"

#define check_pointer(this) do{ if( this == NULL ) return -1;}while(0)
#define check_number(n) do{ if( n < 0 )return -2;}while(0)

// queue
int block_queue_init( BlockQueue* q, int n ){
    check_pointer(q);
    check_number(n);
    q->capacity = n;
    q->front = 0;
    q->back = 0;
    q->size = 0;
    q->items = (int*) malloc( n * sizeof(int) );
    memset(q->items, 0, n * sizeof(int));
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);

    return 0;
}

int block_queue_enqueue( BlockQueue* q, int socket_fd ){
    check_pointer(q);
    check_number(socket_fd);
    // critical cond
    pthread_mutex_lock(&q->mutex); // stops other thread from adding to queue
    // wait while queue is full, 
    while( q->size >= q->capacity ){
        pthread_cond_wait(&q->cond, &q->mutex);
    }
    q->items[q->back] = socket_fd;
    q->back = (q->back + 1) % q->capacity;
    q->size++;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

int block_queue_dequeue( BlockQueue* q ){
    check_pointer(q);
    // critical cond
    pthread_mutex_lock(&q->mutex); // stops other thread from dequeue when queue is empty
    while( q->size == 0 ){
        pthread_cond_wait(&q->cond, &q->mutex);
    }
    int ret = *(q->items + q->front); // returns client fd
    q->front = (q->front + 1) % q->capacity;
    q->size--;
    pthread_cond_signal(&q->cond); 
    pthread_mutex_unlock(&q->mutex);
    return ret;
}

int block_queue_delete( BlockQueue* q ){
    check_pointer(q);
    free(q->items);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
    return 0;
}

// thread pool
int thread_pool_init( ThreadPool* q, HashTable* hashtable, int thread_num, int queue_length, void* (*run)(void*) ){
    check_pointer(q);
    check_pointer(run);
    check_number(thread_num);
    q->queue = (BlockQueue* ) malloc(sizeof(BlockQueue));
    memset(q->queue, 0, sizeof(BlockQueue));
    block_queue_init(q->queue, queue_length);
    q->ht = hashtable;
    q->run = run;
    q->thread_num = thread_num;
    q->threads = (pthread_t *) malloc(thread_num * sizeof(pthread_t));
    q->shutdown = false;
    return 0;
}

// start pool and fill with threads
int thread_pool_start( ThreadPool* pool ){
    check_pointer(pool);
    check_pointer(pool->run);
    for( int i = 0; i < pool->thread_num; ++i ){
        pthread_create(pool->threads + i, NULL, pool->run, pool);
    }
    return 0;
}

// add socket to queue
int thread_pool_add( ThreadPool* pool, int socket_fd){
    check_pointer(pool);
    int code = block_queue_enqueue(pool->queue, socket_fd);
    if( code < 0 ) return code; // something is wrong
    return 0;
}

// shouldn't need, spec says server does not need to shutdown
int thread_pool_shutdown( ThreadPool* pool ){
    check_pointer(pool);
    while( pool->queue->size > 0 ){
        pthread_mutex_lock(&pool->queue->mutex); // prevent changes to queue
        pthread_cond_wait(&pool->queue->cond, &pool->queue->mutex); // wait for the queue to finish dequeue
        pthread_mutex_unlock(&pool->queue->mutex); // unlock
    }
    pool->shutdown = true; 
    return 0;
}

int thread_pool_delete( ThreadPool* pool ){
    check_pointer(pool);
    for( int i = 0; i < pool->thread_num; ++i ){
        pthread_join(*(pool->threads + i), NULL); // 
    }
    free(pool->threads);
    block_queue_delete(pool->queue);
    ht_free(pool->ht);
    free(pool->queue);
    free(pool);
    return 0;
}