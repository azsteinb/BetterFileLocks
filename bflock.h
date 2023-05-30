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
    pthread_mutex_t *flagLock;
    pthread_cond_t *ioCondition;
    bool isWriting;
    int16_t activeReaders; // number of readers
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

/* This inserts a LockNode into the hash table. Thread safe. You don't really need to use this though, because getLockNode, unless reimplemented by the user, will automatically create and insert a lockNode for unknown resource names. You should only use this if you have a scenario where you ~need~ to insert a file before you access it for the first time. */
int8_t insertLock(LockNode *lockNode, SharedLocks *fileLockTable);

/* This creates a lock node. Thread safe */
LockNode *createLockNode(char *resourceName);

/* Constructor. Thread safe. */
SharedLocks *createSharedLocks(uint16_t size);

/* Initialize the global lock table. Call this at the beginning of the program. If you do not want to use the global BFLOCK table, then use createSharedLocks() instead and use the returned SharedLocks pointer locally */
int8_t INIT_B_FLOCK();

/* deconstructor. Call this at the end of the program. Returns true if successful. Program will leak if this is not called */
int8_t DECONSTRUCT_B_FLOCK();

/*
djb2 hashing algorithm by Dan Bernstein
*/
unsigned long hash(char *resourceName);

/* Put this before a write operation, returns nothing. Thread safe*/
void initWrite(LockNode *lockNode);

/* Put this after a write operation, returns nothing. Thread safe*/
void endWrite(LockNode *lockNode);

/* Put this before a read operation, returns current number of active readers on this LockNode's resource directly before the read operation begins and directly after it is considered safe to do so. Thread safe*/
int16_t initRead(LockNode *lockNode);

/* Put this after a read operation, returns current number of active readers on this LockNode's resource directly after the read and before its flag lock is given up.*/
int16_t endRead(LockNode *lockNode)

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

LockNode *createLockNode(char *resourceName)
{
    LockNode *n = (LockNode *)malloc(sizeof(LockNode));
    if (n)
    {
        strcpy(n->resourceName, resourceName);
        n->isWriting = false;
        n->activeReaders = 0;
        n->flagLock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
        n->ioCondition = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
        pthread_mutex_init(n->flagLock, NULL);
        pthread_cond_init(n->ioCondition, NULL);
    }
    return n;
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

SharedLocks *createSharedLocks(uint16_t size)
{
    SharedLocks *s = (SharedLocks *)malloc(sizeof(SharedLocks));
    if (s)
    {
        s->lockTable = (LockNode **)calloc(sizeof(LockNode *), size);
        s->localHashLock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(s->localHashLock, NULL);
        s->size = size;
    }
    return s;
}

int8_t initBFLOCK()
{
    B_FLOCK_TABLE = createSharedLocks(BFLOCK_HASH_TABLE_SIZE); // B_FLOCK_TABLE is a shared global pointer to a file lock table for the current process running bflock
    if (B_FLOCK_TABLE)
    {
        return 0; // :)
    }
    return -1; // :(
}

void initWrite(LockNode *lockNode)
{
    pthread_mutex_lock(lockNode->flagLock);
    while (lockNode->isWriting || lockNode->activeReaders > 0)
    {
        pthread_cond_wait(lockNode->ioCondition, lockNode->flagLock);
    }
    lockNode->isWriting = true;
    pthread_mutex_unlock(lockNode->flagLock);
    return;
}

void endWrite(LockNode *lockNode)
{
    pthread_mutex_lock(lockNode->flagLock);
    lockNode->isWriting = false;
    pthread_cond_signal(lockNode->ioCondition);
    pthread_mutex_unlock(lockNode->flagLock);
    return;
}

int16_t initRead(LockNode *lockNode)
{
    pthread_mutex_lock(lockNode->flagLock);
    while (lockNode->isWriting)
    {
        pthread_cond_wait(lockNode->ioCondition, lockNode->flagLock);
    }
    lockNode->activeReaders += 1;
    int returnValue = lockNode->activeReaders;
    pthread_mutex_unlock(lockNode->flagLock);
    return returnValue;
}

int16_t endRead(LockNode *lockNode)
{
    pthread_mutex_lock(lockNode->flagLock);
    lockNode->activeReaders -= 1;
    int16_t returnValue = lockNode->activeReaders;
    pthread_cond_signal(lockNode->ioCondition);
    pthread_mutex_unlock(lockNode->flagLock);
    return returnValue;
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
