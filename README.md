# BetterFileLocks
Making a multithreaded program in C? Are you dealing with multithreaded IO operations? This is an easier, more digestable alternative to 'flock'. My end goal is to get this working perfectly, and then make a distributed systems counterpart. More on that later.

## Features
+ Very simple to use. Simply include the .h file and add a couple lines of code to your project.
+ No dead locks, ever
+ Efficient space complexity that is negligible compared to performance gain of concurrent operations
+ Easy to use and expand upon, you can define your own implementation on the functions if you wish to do so.
+ Easy to declare default global bflock table can be enabled by simply running INIT_B_FLOCK(); at the start of your program.
+ Easy to deconstruct by simply running DECONSTRUCT_B_FLOCK(); at the end of your program.

## To-do
+ Add support to table files based on file descriptors instead of file-name. The difficulty with this is keeping track of closed files. Might add more for the user to do.
+ In most use cases, such as a server returning files, the file names will be known anyway, but this is definitely not always the case. I will figure out an alternative.

## Examples

### Example 1:
```c
include "bflock.h"

int readNBytes(int fd, int nBytes, char *buf);
int writeNBytes(int fd, int nBytes, char *buf);

int main(int argc, char *argv[])
{
    if(INIT_B_FLOCK()) // init b flock and check if there is an error
        return -1

    // make two threads, one for thread1() and one for thread2()
    // these functions will not try to access the file at the same time.
    // multiple read operations (thread 1) can happen at the same time,
    // but write operations can only happen one at a time and when no read operations are happening
    int fd = open('file-name', O_RDWR);
    void *pfd = (void *)&fd;
    pthread_t threads[7];
    pthread_create(&threads[0], NULL, thread1, pfd);
    pthread_create(&threads[1], NULL, thread1, pfd);
    pthread_create(&threads[2], NULL, thread2, pfd);
    pthread_create(&threads[3], NULL, thread2, pfd);
    pthread_create(&threads[4], NULL, thread1, pfd);
    pthread_create(&threads[5], NULL, thread1, pfd);
    pthread_create(&threads[6], NULL, thread1, pfd);
    for (int i = 0; i < 7; i++){
        pthread_join(threads[i], NULL);
    }

}

void* thread2(void* arg)
{
    int fd = *arg;
    LockNode *ln = getLockNode('file-name', B_FLOCK_TABLE);
    char buf[2048];
    initWrite(ln);
    writeNBytes(fd, 1024, buf);
    endWrite(ln);
}

void* thread1(void* arg)
{
    int fd = *arg;
    LockNode *ln = getLockNode('file-name', B_FLOCK_TABLE);
    char buf[2048];
    int16_t concurrentReadersStart = initRead(ln);
    readNBytes(fd, 1024, buf);
    int16_t concurrentReadersEnd = endRead(ln);
}
```
