#ifndef __BFLOCK_H
#define __BFLOCK_H
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define BFLOCK_HASH_TABLE_SIZE 2048 /* Customizable. 2048 should be more than enough for any one instance of bflock */
#define BFLOCK_RESOURCE_NAME_SIZE 22 /* File names have a maximum of 21 characters + the null character. This is to http standards by default */

/*
*/
typedef struct LockNode{
    char resourceName[BFLOCK_RESOURCE_NAME_SIZE]; /* File names have a maximum of 21 characters + the null character. This is to http standards by default */
    pthread_mutex_t *writeLock;
    pthread_mutex_t *readLock;
    pthread_cond_t *startWriting;
    pthread_cond_t *startReading;
    bool canRead;
    bool canWrite;
    struct LockNode *next;
} LockNode;

typedef struct SharedLocks{
    LockNode **lockTable;
    uint16_t size;
    pthread_mutex_t *localHashLock;
} SharedLocks;


SharedLocks *sharedFileLockTable; // Global Lock Table

/* This returns the lock node associated with the file given by the resource name. Thread safe */ 
LockNode *getLockNode(char *resourceName, SharedLocks *fileLockTable);

/* This inserts a LockNode into the hash table. Thread safe. */ 
uint8_t insertLock(LockNode *lockNode, SharedLocks *fileLockTable);

/* This creates a lock node. Thread safe */
LockNode *createLockNode(char *resourceName);

/* Constructor. Thread safe. */ 
SharedLocks *createSharedLocks(uint16_t size);

/* Initialized the global lock table. Should only be used once. Will replace the address in the global variable if used again. */
int8_t initFileLockTable();

/* 
djb2 hashing algorithm by Dan Bernstein
*/
unsigned long hash(char *resourceName);

// IMPLEMENTATION
#ifndef __BFLOCK_IMPLEMENTATION
#define __BFLOCK_IMPLEMENTATION

LockNode *getLockNode(char *resourceName, SharedLocks *fileLockTable){
    pthread_mutex_lock(fileLockTable->localHashLock);
    // Critical Section. We do not want multiple people to get the lock at the same time. Also, the lock node could be being added to the hash table at the same time
    uint16_t index = hash(resourceName) % fileLockTable->size; // Find the index in the hash table
    LockNode *lockNode = NULL; // The return value by default
    if(fileLockTable->lockTable[index]){
        LockNode *n = fileLockTable->lockTable[index]; // Find the first node at that index
        while(n){
            if(strcmp(n->resourceName, resourceName) == 0){ // Loop until we know it is the right one. Hopefully this only happens once if that hash works well enough.
                lockNode = n; // Mark down that lock
                break; // break from the search loop
            }
            n = n->next; // Go to the next node at this entry
        }
    }
    else{
        lockNode = createLockNode(resourceName);
        fileLockTable->lockTable[index] = lockNode;
    }
    pthread_mutex_unlock(fileLockTable->localHashLock); // Exit the critical section
    return lockNode; // Return the lock
}



#endif


#endif
