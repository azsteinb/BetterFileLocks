# BetterFileLocks
Making a multithreaded program in C? Are you dealing with multithreaded IO operations? This is an easier, more digestable alternative to 'flock'. My end goal is to get this working perfectly, and then make a distributed systems counterpart. More on that later.

## Features
+ Very simple to use. Simply include the .h file and add a couple lines of code to your project.
+ No dead locks, ever
+ Efficient space complexity that is negligible compared to performance gain of concurrent operations
+ Easy to use and expand upon, you can define your own implementation on the functions if you wish to do so.
+ Easy to declare default global bflock table can be enabled by simply running INIT_B_FLOCK(); at the start of your program.
+ Easy to deconstruct by simply running DECONSTRUCT_B_FLOCK(); at the end of your program.
