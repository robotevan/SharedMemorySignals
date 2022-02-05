#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#define SM_SIZE sizeof(uint32_t)    // will be using uint32_t random number
#define SM_KEY 1234                 // define key for shared memoryfor 
#define THRESHOLD 10000000          // print if larger than this
#define P_SLEEP 250000              // sleep 0.25S between sends
#define SEM_TIMEOUT 5               // Sem timeout of 5 seconds!
#define MAX_RAND_PRINTS 10          // max number of times parent can print
#define SEM_NAME "named_sem"        // name for named semaphore

// Use a macro to access contents of shared memory, returns a pointer to sm_contents_t
#define SM_CONTENTS(sm_ptr) *((uint32_t *)sm_ptr)

static int print_rand = 0; // flag to print from parent
static int sig_count = 0; // number of times parent signaled

/*
 * generate a random number between 0 and 2^32 - 1
 * @return random number
 */
u_int32_t generate_random_number()
{
    return random();
}

/*
 * Create a new shared memory segment
 * @parem key for the shared memory
 * @param size to allocate in bytes
 * @return id of shared memory on success
 */
int create_sm(int key, size_t size)
{ 
    printf("Creating shared memory with key %d\n", (int)key);
    int shmid = shmget((key_t)key, size, 0666 | IPC_CREAT);
    if (shmid < 0) { // make sure success on allocation
        fprintf(stderr, "Failed to create shared memory!");
        exit(EXIT_FAILURE);
    }
    return shmid;
}

/*
 * Attach to shared memory segment
 * @param shared memory id to attach to 
 * @param pointer to shared memory reference
 */
void *attach_sm(int shmid)
{
    void *sm = shmat(shmid, (void *)0, 0);
    if (sm == (void *)-1) {
        fprintf(stderr, "Failed to attach to shared memory!\n");
        exit(EXIT_FAILURE);
    }
    return sm;
}

/*
 * Detach from shared memory segment
 * @param pointer to shared memory reference
 */
void detach_sm(void *sm)
{
    if (shmdt(sm) == -1) {
        fprintf(stderr, "Failed to detach from shared memory at %lu!\n", (long int)sm);
        exit(EXIT_FAILURE);
    }
}

/*
 * Destroy to shared memory segment at shmid
 * @param shared memory id to destroy 
 */
void destroy_sm(int shmid)
{
        if (shmctl(shmid, IPC_RMID, 0) == -1) {
        fprintf(stderr, "Failed to destroy shared memory segment!\n");
        exit(EXIT_FAILURE);
    }
}

/*
 * Used to set the timeout for the timed semaphore, current time + timeout
 * @param pointer to timepsec struct
 */
void set_sem_timeout(struct timespec *ts){
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += SEM_TIMEOUT;
}

/*
 * SigHandler, parent will print when flag is set to true
 */
void printParentHandler(){
    print_rand = 1;
    sig_count++;
}

int main(void)
{
    // create a named semaphore, named semaphores reside in shared mem!
    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0644, 0); // use 0 to initially block
    sem_unlink(SEM_NAME); // unlink, will destory when both processes close it!
    struct timespec ts; // hold timeout for semaphore
    int shmid = create_sm(SM_KEY, SM_SIZE); // create shared memory
    pid_t pid = fork(); // create child
    sem_post(sem); // unlock semaphore initially, anyone can grab it

    if (pid < 0) { 
        perror("fork failed");
        exit(EXIT_FAILURE);

    } else if (pid == 0) { // child process
        void *shared_mem_child = attach_sm(shmid);
        int printed_nums = 0;
        printf("Child with pid %d attached to sm @%ld\n", getpid(), (long int)shared_mem_child);
        while(printed_nums <= MAX_RAND_PRINTS){
            // wait 5s, else break and kill process
            set_sem_timeout(&ts);
            if (sem_timedwait(sem, &ts) < 0){
                perror("Failed to wait!"); 
                break; // timeout, break to exit loop
            }
            uint32_t rnum = generate_random_number();
            SM_CONTENTS(shared_mem_child) = rnum; // write to sm
            if (rnum > THRESHOLD){
               // signal parent!
               printed_nums++;
               kill(getppid(), SIGUSR1);
            }
            if (printed_nums > MAX_RAND_PRINTS) {
                break; // will stop loop
            }
            sem_post(sem); // unlock semaphore
            usleep(P_SLEEP);
        }
        // Child process is done!
        printf("Child done, detatching from %lu\n", (long int)shared_mem_child);
        detach_sm(shared_mem_child);
        sem_close(sem);

    } else {  // parent process
        void *shared_mem_parent = attach_sm(shmid);
        (void) signal(SIGUSR1, printParentHandler); // attach sig handler
        printf("Parent with pid %d attached to sm @%ld\n", getpid(), (long int)shared_mem_parent);
        while (sig_count <= MAX_RAND_PRINTS) {
            // if the parent needs to print, aquire semaphore
            if (print_rand){
                // Attempt to unlock Semaphore, wait 5s, else break and kill process
                set_sem_timeout(&ts);
                if (sem_timedwait(sem, &ts) < 0){
                    perror("Failed to wait!");
                    break; // timeout, break to exit loop
                }
                printf("%d Greater than %d\n", SM_CONTENTS(shared_mem_parent), THRESHOLD);
                print_rand = 0; // reset flag                
                sem_post(sem); // unlock semaphore
            }
            usleep(P_SLEEP);
        }
        // Parent Proccess is done!
        printf("Parent done, detatching from %lu\n", (long int)shared_mem_parent);
        detach_sm(shared_mem_parent);
        destroy_sm(shmid); // Only destroy once, so done in parent process
        sem_close(sem);
    }
    exit(EXIT_SUCCESS);
}