#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

int main(int argCount, char *argValues[]) {
    // Start syslog for logging purposes
    openlog("writer_program", LOG_PID | LOG_CONS, LOG_USER);

    // Validate the number of arguments
    if (argCount != 3) {
        syslog(LOG_ERR, "Error: Wrong number of arguments. Usage: %s <file_path> <string_to_write>", argValues[0]);
        fprintf(stderr, "Usage: %s <file_path> <string_to_write>\n", argValues[0]);
        closelog();
        exit(1);
    }

    const char *fileLocation = argValues[1];
    const char *contentToWrite = argValues[2];

    // Open the file for writing
    FILE *filePointer = fopen(fileLocation, "w");
    if (filePointer == NULL) {
        syslog(LOG_ERR, "Error: Couldn't open the file %s. Reason: %s", fileLocation, strerror(errno));
        perror("File opening failed");
        closelog();
        exit(1);
    }

    // Write the string to the file
    if (fprintf(filePointer, "%s", contentToWrite) < 0) {
        syslog(LOG_ERR, "Error: Couldn't write to file %s. Reason: %s", fileLocation, strerror(errno));
        perror("Writing to file failed");
        fclose(filePointer);
        closelog();
        exit(1);
    }

    // Log the successful write operation
    syslog(LOG_DEBUG, "Successfully wrote '%s' to file '%s'", contentToWrite, fileLocation);

    // Close the file and syslog
    fclose(filePointer);
    closelog();

    return 0;
}

