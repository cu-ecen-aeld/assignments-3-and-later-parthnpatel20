#ifndef THREADING_H
#define THREADING_H

#include <stdbool.h>
#include <pthread.h>

/**
 * This structure should be dynamically allocated and passed as
 * an argument to your thread using pthread_create.
 * It should be returned by your thread so it can be freed by
 * the joiner thread.
 */
struct thread_data {
    /*
     * Thread function parameters for mutex handling
     */
    pthread_mutex_t *mutex; // Pointer to the mutex the thread will lock
    int wait_to_obtain_ms;  // Time to wait before locking the mutex
    int wait_to_release_ms; // Time to hold the mutex before releasing it

    /**
     * Set to true if the thread completed with success, false
     * if an error occurred.
     */
    bool thread_complete_success;
};

/**
* Start a thread which sleeps @param wait_to_obtain_ms number of milliseconds, then obtains the
* mutex in @param mutex, then holds for @param wait_to_release_ms milliseconds, then releases.
*
* The start_thread_obtaining_mutex function should only start the thread and should not block
* for the thread to complete.
*
* The function should dynamically allocate memory for the thread_data structure, which will
* be returned to the thread.
*
* If a thread was started successfully, @param thread should be filled with the pthread_create thread ID
* corresponding to the thread which was started.
*
* @return true if the thread could be started, false if a failure occurred.
*/
bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms);

#endif // THREADING_H

