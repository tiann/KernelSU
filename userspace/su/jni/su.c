#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/prctl.h>

#define VERSION "8"

static void help(int status){
    printf("KernelSU\n"
           "Usage: su [options] [argument...]\n\n"
           "Options: \n"
           "-c Pass command to invoked shell\n"
           "-v Print version code and exit\n"
           "-h Display this message and exit\n"
          );
    exit(status);
}

int elevate(){

    // Talk to Daemon in Kernel Space
    prctl(0xdeadbeef, 0, 0, 0, 0);

    /*
    *See if we elevated successfully
    *I didn't see any better way to examine than test it
    */
    if(getuid() != 0){
        printf("Permission denied\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}

int su_shell(char *envp[]){

    char *shell = getenv("SHELL");

    elevate();

    if(shell) {
        execve(shell, NULL, envp);
    } else {
        execve("/system/bin/sh", NULL, envp);
    }

    return 0;
}

void su_command(char *command){

    elevate();
    int status = system(command);

    exit(status);
}

int main(int argc, char *argv[], char *envp[]){

    if(argc == 1) {
        su_shell(envp);
        exit(EXIT_SUCCESS);
    }

    int opt;
    char *command;

    while ((opt = getopt(argc, argv, "vch")) != -1) {
           switch (opt) {
           case 'v':
                printf("%s:KernelSU\n", VERSION);
                exit(EXIT_SUCCESS);
           case 'c':
                if (argc < 3) {
                    printf("ERROR: -c requires an argument\n\n");
                    help(EXIT_FAILURE);
                }
                command = argv[2];
                su_command(command);
           case 'h':
                help(EXIT_SUCCESS);
           }
    }
    exit(EXIT_SUCCESS);
}
