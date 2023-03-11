#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_FILES 100
#define MAX_FILENAME_LEN 256

int num_files = 0;
char filenames[MAX_FILES][MAX_FILENAME_LEN];
int pipe_fds[MAX_FILES][2];  // array of pipes for sending data back to parent
pid_t child_pids[MAX_FILES]; // array of child process IDs

void child_process(int i) {
    close(pipe_fds[i][0]);
    char filename[MAX_FILENAME_LEN];
    if (strcmp(filenames[i], "SIG") == 0) {
        printf("Child process %d handling SIGINT\n", getpid());
        kill(getpid(), SIGINT);
    } else {
        strcpy(filename, filenames[i]);
        int fd = open(filename, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "Could not open %s\n", filename);
        }

        int n;
        int length = lseek(fd, 0, SEEK_END);
        char *buffer = malloc(sizeof(char) * length);
        lseek(fd, 0, SEEK_SET);
        int char_counts[26] = {0};
        char c;
        while ((n = read(fd, buffer, length)) > 0) {

            for (int i = 0; i < n; i++) {
                if (isalpha(buffer[i])) {
                    c = tolower(buffer[i]);
                    char_counts[c - 'a']++;
                }
            }
            if (n < 0) {
                fprintf(stderr, "read error");
            }
        }

        free(buffer);
        close(fd);
        write(pipe_fds[i][1], char_counts, sizeof(char_counts));
        close(pipe_fds[i][1]); // close write end of pipe
        int sleep_time = 10 + 2 * i;
        printf("Child process %d sleeping for %d seconds\n", getpid(),
               sleep_time);
        sleep(sleep_time);
        exit(0);
    }
}

void OutputHistogram(int i, int *A) {
    char *outputfile = malloc(sizeof(char) * 50);
    sprintf(outputfile, "%s%d%s", filenames[i], child_pids[i], ".hist");
    int fd = open(outputfile, O_CREAT | O_RDWR | O_APPEND, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        printf("Error creating output file\n");
        exit(-2);
    }

    for (int j = 0; j < 26; j++) {
        char letter = 'a' + j;
        char freq_str[10];
        sprintf(freq_str, "%d", A[j]);
        char output[20];
        sprintf(output, "%c %s\n", letter, freq_str);
        write(fd, output, strlen(output));
    }
    close(fd);
    free(outputfile);
}

void handle_sigchld(int sig) {
    int child_status;
    pid_t child_pid;
    int histogram[26];
    while ((child_pid = waitpid(-1, &child_status, WNOHANG)) > 0) {
        printf("Parent caught SIGCHLD from child process %d\n", child_pid);
        int i;
        for (i = 0; i < num_files; i++) {
            if (child_pids[i] == child_pid) {
                if (WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0) {
                    printf("Child process %d terminated normally.\n",
                           child_pid);
                } else {
                    printf("Child process %d terminated abnormally.\n",
                           child_pid);
                    if (WIFSIGNALED(child_status)) {
                        printf("Terminated by signal: %s\n",
                               strsignal(WTERMSIG(child_status)));
                    }
                }
                read(pipe_fds[i][0], histogram, sizeof(histogram));
                close(pipe_fds[i][0]);
                OutputHistogram(i, histogram);
                break;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    /* Requires at least one file to read */
    if (argc < 2) {
        fprintf(stderr,
                "Incorrect Usage must enter at least one file as argument\n");
        exit(1);
    }

    num_files = argc - 1;

    /* Reads files into the global array */
    for (int i = 1; i < argc; i++) {
        strcpy(filenames[i - 1], argv[i]);
    }

    // int histogram[26];
    for (int i = 0; i < num_files; i++) {
        if (pipe(pipe_fds[i]) == -1) {
            return -1;
        }
        pid_t pid = fork();
        // Child Process Handler:
        if (pid == 0) {
            child_process(i);
        } else if (pid == -1) {
            fprintf(stderr, "Error: Failed to fork child process.\n");
            exit(1);
            // Parent Process Handler:
        } else {
            child_pids[i] = pid;
        }
    }

    // Parent Process
    while (1) {
        sleep(1);
        signal(SIGCHLD, handle_sigchld);
        if (waitpid(-1, NULL, WNOHANG) == -1) {
            printf("All processes terminated\n");
            break;
        }
    }
    return 0;
}
