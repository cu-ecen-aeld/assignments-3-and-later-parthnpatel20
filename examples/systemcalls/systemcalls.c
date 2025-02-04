#include "systemcalls.h"
#include <stdio.h>      // For printf, perror
#include <stdlib.h>     // For system, exit, EXIT_FAILURE
#include <unistd.h>     // For fork, execv, dup2, STDOUT_FILENO
#include <sys/types.h>  // For pid_t
#include <sys/wait.h>   // For waitpid, WIFEXITED, WEXITSTATUS
#include <fcntl.h>      // For open, O_WRONLY, O_CREAT, O_TRUNC
#include <stdarg.h>     // For variable argument handling


/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{



    if (cmd == NULL)
        return false;

    int ret = system(cmd);
    if (ret == -1) 
        return false;
    
    // Check if command exited normally and returned 0
    return WIFEXITED(ret) && WEXITSTATUS(ret) == 0;
   
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    
    // Allocate memory for command arguments
    char *command[count+1];
    
    for (int i = 0; i < count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;  // execv() requires a NULL-terminated argument list

    va_end(args);  //cleans up the va_list 

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork failed");
        return false;
    }

    if (pid == 0)  // Child process
    {
        execv(command[0], command);  // Execute the command
        perror("execv failed");  // If execv returns, there was an error
        exit(EXIT_FAILURE);
    }

    // Parent process waits for the child to finish
    int status;
    if (waitpid(pid, &status, 0) == -1)
    {
        perror("waitpid failed");
        return false;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;  // Return true if child exited successfully  
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
     if (outputfile == NULL || count< 1)
        return false;

    va_list args;
    va_start(args, count);
    
    char *command[count+1];
    for (int i = 0; i < count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    va_end(args);

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork failed");
        return false;
    }

    if (pid == 0)  // Child process
    {
        // Open the output file
        int fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1)
        {
            perror("open failed");
            exit(EXIT_FAILURE);
        }

        // Redirect stdout to the file
        if (dup2(fd, STDOUT_FILENO) == -1)
        {
            perror("dup2 failed");
            exit(EXIT_FAILURE);
        }

        close(fd);  // Close file descriptor, stdout is now redirected

        execv(command[0], command);
        perror("execv failed");  // If execv() returns, an error occurred
        exit(EXIT_FAILURE);
    }

    // Parent process waits for child
    int status;
    if (waitpid(pid, &status, 0) == -1)
    {
        perror("waitpid failed");
        return false;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
   
}
