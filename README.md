## CHILD PROCESS
->Every 0.25s, the child writes a random number to shared memory (uint32_t) If this number is greater
 than the defined threshold, it will send SIGUSER1 to the parent PROCESS.

-> Any time the child writes to the shared memory, it locks the binary mutext, preventing the PARENT
 from accessing, also removing the need of pause() in the child.

-> The semaphore uses sem_timedwait() to make sure the process will end if it cannot aquire the
 semaphore in less than 5s (ex: press ctrl+C while parent/child has the semaphore)

## PARENT PROCESS
-> The parent loops while a sig_count < 10 (print 10 times)

-> If the parent was signaled, it will aquire the semaphore, allowing it to access the memory

-> After printing the random number, reset the print_rand flag, increment sig_count\

-> The semaphore uses sem_timedwait() to make sure the process will end if it cannot aquire the
 semaphore in less than 5s (ex: press ctrl+C while parent/child has the semaphore)


-> After both processes finish, the shared memory is destroyed, along with the named semaphore

-> Compile with "make", uses pthread for named semaphore
