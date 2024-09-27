
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/dirent.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <syscall.h>

#define SHELL_MAX_ARGS 5
#define SHELL_MAX_LINE 256

void DispatchCommand(char *buf);

int
main(int argc, const char *argv[])
{
    char buf[256];

    printf("System Shell\n");

    while (1) {
	fputs("Shell> ", stdout);
	fgets(buf, sizeof(buf), stdin);

	DispatchCommand(buf);
    }
}

void
Cmd_Help(int argc, const char *argv[])
{
    printf("bkpt       Trigger a kernel breakpoint\n");
    printf("exit       Exit shell\n");
    printf("help       Display the list of commands\n");
}

void
Cmd_Exit(int argc, const char *argv[])
{
    int val = 0;

    if (argc != 1 && argc != 2) {
	printf("Invalid number of arguments\n");
	return;
    }

    if (argc == 2) {
	val = atoi(argv[1]);
    }

    exit(val);
}

const char *searchpath[] = {
    "",
    "/sbin/",
    "/bin/",
    "/tests/",
    NULL
};

void
Cmd_Run(int argc, const char *argv[])
{
    int i, status;
    char path[256];
    struct stat sb;

    i = 0;
    while (searchpath[i] != NULL) {
	strcpy(path, searchpath[i]);
	strcat(path, argv[0]);

	status = OSStat(path, &sb);
	if (status != 0) {
	    i++;
	    continue;
	}

	argv[argc] = NULL;
	status = spawn(path, &argv[0]);
	if (status > 100000) {
	    printf("Spawn failed!\n");
	}

	pid_t pid = wait(&status);
	printf("Process result: %d (pid %d)\n", WEXITSTATUS(status), pid);
	return;
    }

    printf("Unknown command '%s'\n", argv[0]);
}

void
DispatchCommand(char *buf)
{
    int i;
    int argc;
    char *argv[SHELL_MAX_ARGS+1];
    char *nextArg;

    // Remove newline
    for (i = 0; buf[i] != 0; i++) {
	if (buf[i] == '\n') {
	    buf[i] = '\0';
	    break;
	}
    }

    // parse input
    nextArg = strtok(buf, " \t\r\n");
    for (argc = 0; argc < SHELL_MAX_ARGS; argc++) {
        if (nextArg == NULL)
            break;

        argv[argc] = nextArg;
	nextArg = strtok(NULL, " \t\r\n");
    }

    // execute command
    if (argc == 0) {
	// Ignore empty lines
    } else if (strcmp(argv[0], "help") == 0) {
	Cmd_Help(argc, (const char **)argv);
    } else if (strcmp(argv[0], "bkpt") == 0) {
	asm volatile("int3");
    } else if (strcmp(argv[0], "exit") == 0) {
	Cmd_Exit(argc, (const char **)argv);
    } else if (strcmp(argv[0], "#") == 0) {
	// Ignore comments
    } else if (buf[0] == '\0') {
	// Ignore empty lines
    } else {
	Cmd_Run(argc, (const char **)argv);
    }
}

