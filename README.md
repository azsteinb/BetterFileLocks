# BetterFileLocks
Making a multithreaded program in C? Are you dealing with multithreaded IO operations? This is an easier, more digestable alternative to 'flock'

## Features
+ Independent read and write locks so you can stack more concurrent read operations while no write operations are happening, while also blocking future concurrent write operations until every read operation has finished.
+ No dead locks, ever
+ Efficient space complexity that is negligible compared to performance gain of concurrent operations
+ Easy to use and expand upon
