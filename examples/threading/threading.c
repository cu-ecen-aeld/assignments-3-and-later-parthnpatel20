#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// Optional debug macros
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

// Thread function
void* threadfunc(void* thread_param)
{
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    
    // Sleep for the specified time before obtaining mutex
    usleep(thread_func_args->wait_to_obtain_ms * 1000);

    // Try to obtain the mutex
    if (pthread_mutex_lock(thread_func_args->mutex) != 0) {
        ERROR_LOG("Failed to lock mutex");
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    DEBUG_LOG("Mutex obtained by thread");

    // Sleep for the specified time while holding the mutex
    usleep(thread_func_args->wait_to_release_ms * 1000);

    // Release the mutex
    if (pthread_mutex_unlock(thread_func_args->mutex) != 0) {
        ERROR_LOG("Failed to unlock mutex");
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    DEBUG_LOG("Mutex released by thread");

    // Mark thread execution as successful
    thread_func_args->thread_complete_success = true;
    
    return thread_param;  // Return pointer for cleanup in joiner thread
}

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    // Allocate memory for thread_data
    struct thread_data *thread_args = (struct thread_data *)malloc(sizeof(struct thread_data));
    if (!thread_args) {
        ERROR_LOG("Memory allocation failed for thread_data");
        return false;
    }

    // Initialize thread data
    thread_args->mutex = mutex;
    thread_args->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_args->wait_to_release_ms = wait_to_release_ms;
    thread_args->thread_complete_success = false; // Default to failure

    // Create the thread
    if (pthread_create(thread, NULL, threadfunc, (void *)thread_args) != 0) {
        ERROR_LOG("Failed to create thread");
        free(thread_args);  // Clean up allocated memory
        return false;
    }

    DEBUG_LOG("Thread successfully created");

    return true;
}

