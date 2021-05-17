/*
    Header file for hashtable
    implementation from: 
        https://medium.com/@bennettbuchanan/an-introduction-to-hash-tables-in-c-b83cbf2b4cf6
*/

#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_ 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

/*
    @key: key to the hashtable

    @mutex: corresponding mutex for the given file
    @cond: condition var for mutex
    @next: pointer to the next node in the list
*/
typedef struct List{
    char* key;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    struct List *next;
}List;

/*
    @size: size fo the array

    @array: array of size @size
    Each cell of this array is a pointer to the first node of a linked list
*/
typedef struct HashTable{
    unsigned int size;
    List **array;
}HashTable;

/*
    @key: key to hash

    @size: size of hashtable

    @return: an integer N:= 0 <= N < @size
    the integer represents the index of @key in an array of size @size
*/
unsigned int hash( const char* key, int size );

/*
    @size: size of the array in hashtable

    @return: a pointer to the newly created hashtable
*/
HashTable* ht_create( int size );

/*
    @hashtable: pointer to the hashtable to be modified

    @node: a List pointer to a node containing the key that wish to be inserted

    @return: void
*/
void node_handler( HashTable* hashtable, List* node );

/*
    @hashtable: pointer to the hashtable to be modified

    @key: filename to add to the hashtable

    @return: 1 if memory allocation fails, 0 if successful. 
*/
int ht_put( HashTable* hashtable, const char* key );

/*
    @hashtable: pointer to the hashtable to be modified

    @key: filename to search in the hashtable

    @return: pointer to the Node if found, otherwise NULL.
*/
List* ht_get( HashTable* hashtable, const char* key);


void ht_free( HashTable* hashtable );


#endif