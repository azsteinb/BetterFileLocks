#ifndef __BFLOCK_H
#define __BFLOCK_H
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define BFLOCK_HASH_TABLE_SIZE 2048  /* Customizable. 2048 should be more than enough for any one instance of bflock */
#define BFLOCK_RESOURCE_NAME_SIZE 22 /* File names have a maximum of 21 characters + the null character. This is to http standards by default */

/*
 */
typedef struct LockNode
{
    char resourceName[BFLOCK_RESOURCE_NAME_SIZE]; /* File names have a maximum of 21 characters + the null character. This is to http standards by default */
    pthread_mutex_t *writeLock;
    pthread_mutex_t *readLock;
    pthread_cond_t *startWriting;
    pthread_cond_t *startReading;
    bool canRead;
    bool canWrite;
    struct LockNode *next;
} LockNode;

typedef struct SharedLocks
{
    LockNode **lockTable;
    uint16_t size;
    pthread_mutex_t *localHashLock;
} SharedLocks;

SharedLocks *B_FLOCK_TABLE; // Global Lock Table for this process

/* This returns the lock node associated with the file given by the resource name. Thread safe */
LockNode *getLockNode(char *resourceName, SharedLocks *fileLockTable);

/* This inserts a LockNode into the hash table. Thread safe. You don't really need to use this though, because getLockNode, unless reimplemented by the user, will automatically create and insert a lockNode for unknown resource names */
int8_t insertLock(LockNode *lockNode, SharedLocks *fileLockTable);

/* This creates a lock node. Thread safe */
LockNode *createLockNode(char *resourceName);

/* Constructor. Thread safe. */
SharedLocks *createSharedLocks(uint16_t size);

/* Initialize the global lock table. Do not touch this or call this */
int8_t _initFileLockTable();

/* deconstructor. Call this at the end of the program. Returns true if successful. Program will leak if this is not called */
int8_t DECONSTRUCT_B_FLOCK();

/*
djb2 hashing algorithm by Dan Bernstein
*/
unsigned long hash(char *resourceName);

// IMPLEMENTATION
#ifndef __BFLOCK_IMPLEMENTATION
#define __BFLOCK_IMPLEMENTATION

LockNode *getLockNode(char *resourceName, SharedLocks *fileLockTable)
{
    pthread_mutex_lock(fileLockTable->localHashLock);
    // Critical Section. We do not want multiple people to get the lock at the same time. Also, the lock node could be being added to the hash table at the same time
    uint16_t index = hash(resourceName) % fileLockTable->size; // Find the index in the hash table
    LockNode *lockNode = NULL;                                 // The return value by default
    if (fileLockTable->lockTable[index])
    {
        LockNode *n = fileLockTable->lockTable[index]; // Find the first node at that index
        LockNode *prev = NULL;
        while (n)
        {
            if (strcmp(n->resourceName, resourceName) == 0)
            {                 // Loop until we know it is the right one. Hopefully this only happens once if that hash works well enough.
                lockNode = n; // Mark down that lock
                break;        // break from the search loop
            }
            prev = n;    // saving prev in case we run into a hash collision.
            n = n->next; // Go to the next node at this entry
        }
        if (lockNode == NULL)
        {
            // At this point, the lockNode might still be NULL due to a hash collision.
            // Can not just call insertLockNode, because we already hold the localHashLock
            prev->next = createLockNode(resourceName); // insert into the collided index
            lockNode = prev->next;
        }
    }
    else
    {
        lockNode = createLockNode(resourceName);
        fileLockTable->lockTable[index] = lockNode;
    }
    pthread_mutex_unlock(fileLockTable->localHashLock); // Exit the critical section
    return lockNode;                                    // Return the lock
}

int8_t insertLockNode(LockNode *lockNode, SharedLocks *fileLockTable)
{
    if (fileLockTable && lockNode)
    {
        pthread_mutex_lock(fileLockTable->localHashLock);
        // critical section
        uint16_t index = hash(lockNode->resourceName) % fileLockTable->size;
        if (fileLockTable->lockTable[index])
        {
            LockNode *n = fileLockTable->lockTable[index];
            lockNode->next = n;                  // Put this node at the start of the linked list
            fileLockTable->lockTable[index] = n; // ^
        }
        else
        {
            fileLockTable->lockTable[index] = lockNode; // It is the first of its kind!
        }
        // end critical section
        pthread_mutex_unlock(fileLockTable->localHashLock);
        return 0;
    }
    return -1;
}

unsigned long hash(char *resourceName)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *resourceName++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

#endif

#endif
