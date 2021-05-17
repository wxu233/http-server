#include "hashtable.h"

/*
    ht_create(): allocate the requisite memory for a new hashtable and its indexed array with positive size.
    Assign the size in ht->size.

    Return: 0 if successful, 1 otherwise
*/
HashTable* ht_create( int size ){
    HashTable *ht;
    if( size < 1 ){
        return NULL;
    }

    ht = (HashTable*) malloc( sizeof(HashTable) );
    if( ht == NULL ){
        return (NULL);
    }

    ht->array = (List**) malloc( size * sizeof(List) );
    if( ht->array == NULL ){
        return (NULL);
    }

    memset( ht->array, 0, size * sizeof(List) );
    
    ht->size = size;
    // printf("ht_create()\n\tsize: %d\n", ht->size);
    return ht;
}

/*
    hash(): find the index of the key passed, or return an empty array index if it does not exist. 
    @key: the key to find in the hashtable, aka filename
    @size: size of the hashtable

    Return: the index of the item.
*/
unsigned int hash( const char* key, int size ){
    unsigned int hash;
    unsigned int i;

    hash = 0;
    i = 0;
    while( key && key[i] ){
        hash = (hash + key[i]) % size;
        ++i;
    }
    // printf("hash()\n\tsize: %d\n\thash value: %d\n", size, hash);
    return (hash);  
}

/*
    node_handler(): if the index is a linked list, traverse it to unsure there is no prexisting item 
    with the key passed. If there is, skip it because same mutex can be used. 

    @hashtable: the hashtable of Lists.
    @node: the linked list to add a node to.

    Return: void.
*/
void node_handler( HashTable* hashtable, List* node ){
    // printf("entered node_handler\n");
    unsigned int i = hash(node->key, hashtable->size);
    // printf("i = %d\n", i);
    List* temp = hashtable->array[i];
    
    if( hashtable->array[i] != NULL ){  // it's a linked list
        temp = hashtable->array[i];
        while( temp != NULL ){
            if( strcmp(temp->key, node->key) == 0 ){    // node with the same key
                break;
            }
            temp = temp->next;
        }
        if( temp == NULL ){ // no existing node with given key
            // printf("new node created\n");
            node->next = hashtable->array[i];   // prepend to the list
            pthread_mutex_init(&node->mutex, NULL);
            pthread_cond_init(&node->cond, NULL);
            hashtable->array[i] = node;
        }
        else{
            // printf("lock already exists\n");
        }
        // don't need an else, because don't need to change the value
    }
    else{   // first node in the list
        // printf("first node in list\n");
        node->next = NULL;  
        hashtable->array[i] = node;
        pthread_mutex_init(&node->mutex, NULL);
        pthread_cond_init(&node->cond, NULL);
    }
}

/*
    ht_put(): allocates memory for a new node and calls the node_handler() to either insert the node if
    the key does not exist, or update a prexisting node.
    
    @key: key(filename) to add to the hashtable.

    Return: 1 if memory allocation fails, 0 if successful. 
*/
int ht_put( HashTable* hashtable, const char* key){
    List* node;

    if( hashtable == NULL){
        return 1;
    }
    // printf("ht_put\n\thash size: %d\n", hashtable->size);
    node = (List*) malloc( sizeof(List) );
    if( node == NULL ){
        return 1;
    }
    node->key = strdup(key);
    // printf("passing to node_handler\n");
    node_handler(hashtable, node);

    return 0;
}



/*
    ht_get(): travers the list that is at the corresponding array location in the hashtable. If a node with 
    the same key is found, then return a pinter to the Node. Otherwise return NULL.

    @hashtable: the table
    @key: filename to search for. 

    Return: pointer to the Node if found, otherwise NULL.
*/  
List* ht_get( HashTable* hashtable, const char* key ){

    unsigned int i;
    List* temp;

    if( hashtable == NULL ){
        return NULL;
    }
    i = hash(key, hashtable->size);
    temp = hashtable->array[i];

    while( temp != NULL ){
        if( strcmp(temp->key, key) == 0 ){
            break;
        }
        temp = temp->next;
    }

    if( temp == NULL ){ // no value is found
        return NULL;
    }

    return temp;
}

/*
    ht_free(): free the items in a hashtable. 

    Return: nothing
*/
void ht_free( HashTable* hashtable ){

    List* temp;
    unsigned int i;

    if( hashtable == NULL ){
        return;
    }

    for( i = 0; i < hashtable->size; i++ ){
        while( hashtable->array[i] != NULL ){
            temp = hashtable->array[i]->next;
            free(hashtable->array[i]->key);
            pthread_mutex_destroy(&(hashtable->array[i]->mutex));
            pthread_cond_destroy(&(hashtable->array[i]->cond));
            hashtable->array[i] = temp;
        }
        free(hashtable->array[i]);
    }
    free(hashtable->array);
    free(hashtable);
}