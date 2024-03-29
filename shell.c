/* Programmer: Gregory Smith
	Date: 05/08/2023
	Program: PS5
*/

/* References:
	https://man7.org/linux/man-pages/man3/strtok_r.3.html,
	https://man7.org/linux/man-pages/man3/getline.3.html,
	https://man7.org/linux/man-pages/man2/fork.2.html,
	https://man7.org/linux/man-pages/man2/wait.2.html,
	https://man7.org/linux/man-pages/man3/exec.3.html,
	https://man7.org/linux/man-pages/man3/malloc.3.html,
	https://man7.org/linux/man-pages/man2/chdir.2.html,
	https://man7.org/linux/man-pages/man3/getcwd.3.html,
	https://man7.org/linux/man-pages/man3/exit.3.html,
	https://man7.org/linux/man-pages/man3/fopen.3.html,
	https://man7.org/linux/man-pages/man2/dup.2.html,
	https://man7.org/linux/man-pages/man2/open.2.html,
	https://man7.org/linux/man-pages/man2/pipe.2.html,
	https://man7.org/linux/man-pages/man2/mkdir.2.html,
	https://linux.die.net/man/3/strcat,
	https://linux.die.net/man/3/strcmp
*/

/* Purpose:
	This program recreates some basic shell functionality, including
	direct system calls with exec as well as some defined functions
	to recreate some other expected commands such as cd and exit
*/


/* Headers */
#include <stdio.h> // needed for getline(), printf()
#include <string.h> // needed for strtok(), strcmp()
#include <stdlib.h> // needed for malloc(), free(), exit()
#include <sys/wait.h> // needed for waitpid()
#include <unistd.h> // needed for execvp(), fork(), chdir(), dup2(), pipe(), write(), read()
#include <fcntl.h> // needed for open()
#include <sys/stat.h> // needed for mkdir()


/* Prototypes */
char * getInput(void);
void getArgs(char *, char **, char **, int *, int *);
void execArgs(char **, char **, int *, int);
void execChildArgs(char **, char *, int, int, char **);
void cd(char **, int);
char * getAbsolutePath(char *);
void pwd(void);
void echo(char **, int, int);
void mkDir(char **, int);


int main(void) {
	char * input;
	char ** argsArr;
	char ** redirArr;
	int * cmdIndexArr;
	int cmdCount;

	while (1) {
		// allocate and populate input command in buffer
		input = getInput();

		// retrieve split of input for execution
		// let us use the default buffer size of getline(), 120, as a baseline
		// TODO: have buffer dynamically expand like vector/arraylist
		argsArr = malloc(sizeof(char) * 120);
		redirArr = malloc(sizeof(char) * 120);
		cmdIndexArr = malloc(sizeof(int) * 60);
		getArgs(input, argsArr, redirArr, cmdIndexArr, &cmdCount);

		// attempt execution
		execArgs(argsArr, redirArr, cmdIndexArr, cmdCount);

		// free buffered variables
		free(input);
		free(argsArr);
		free(redirArr);
		free(cmdIndexArr);

		printf("\n");
	}	

	return 0;
}


/* Function returns a string of user input
*/

char * getInput(void) {
	printf("Please enter a command: ");

	// getline() will set buffer for us by setting NULL to ptr
	char * input = NULL;
	// size will be assigned on call, but is not currently used
	size_t size = 0;

	getline(&input, &size, stdin);
	return input;
}


/* Function returns tokenized array of strings
*/

void getArgs(char * input, char ** tokenArr, char ** ioArr, int * indexArr, int * cmdCount) {
	// since the heap may be modified, we default redirects to NULL
	// in case they are not assigned in the command
	ioArr[0] = NULL;
	ioArr[1] = NULL;

	// by implementation getline() will result in \n terminated string
	char delim[] = " \n";

	// we iterate through all of the tokens and put them in the 2-d array
	char * token = strtok(input, delim);
	int cmdStartIndex = 0;
	int cmdIndex = 0;
	int argIndex = 0;
	int type = 0;
	while (token != NULL ) {
		// next token STDIN
		if ('<' == *token) {
			type = 1;

		// next token STDOUT
		} else if ('>' == *token) {
			type = 2;

		// end of command
		} else if ('|' == *token) {
			indexArr[cmdIndex] = cmdStartIndex;
			// we currently have only accessed the token at the previous index
			indexArr[cmdIndex + 1] = argIndex - 1;
			cmdStartIndex = argIndex;
			cmdIndex = cmdIndex + 2;

		// normal token
		} else if (type == 0) {
			tokenArr[argIndex++] = token;

		// STDIN
		} else if (type == 1) {
			ioArr[0] = token;
			type = 0;

		// STDOUT
		} else if (type == 2) {
			ioArr[1] = token;
			type = 0;

		}

		// go to next token
		token = strtok(NULL, delim);
	}

	indexArr[cmdIndex] = cmdStartIndex;
	// we currently have only accessed the token at the previous index
	indexArr[cmdIndex + 1] = argIndex - 1;
	cmdIndex = cmdIndex + 2;
	* cmdCount = cmdIndex / 2;
}


/* Function attempts to execute user inputted commands
*/

void execArgs(char ** argsArr, char ** ioArr, int * indexArr, int cmdCount) {
	// no commands given
	if (indexArr[1] == -1) {
		return;

	// no need to create subshells
	// allows "exit" and "cd" to work as expected
	} else if (cmdCount == 1) {
		execChildArgs(argsArr, argsArr[0], indexArr[0], indexArr[1], ioArr);
		return;

	} // end of if/else

	// exec variables
	char * execFile;
	int startIndex;
	int endIndex;

	// child variables
	pid_t child;
	int status;

	// i/o redirection variable
	char * redirFd;
	int iofd;

	// piping variables
	int pipefdSize = cmdCount * 2;
	int pipefd[pipefdSize];
	int tempIndex;

	// created all needed pipes
	for (tempIndex = 0; tempIndex < cmdCount; tempIndex++) {
		if ( pipe(pipefd + tempIndex * 2) == -1 ) {
			exit(1);
		}
	}

	int cmdIndex;
	for (cmdIndex = 1; cmdIndex <= cmdCount; cmdIndex++) {
		child = fork();
		if (child == 0) { // is child process
			// when we have a STDIN redirect and first command to execute
			if ( (cmdIndex == 1) && ((redirFd = ioArr[0]) != NULL) ) {
				iofd = open(redirFd, O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR);
				if (dup2(iofd, 0) == -1) {
					exit(1);
				}
				close(iofd);

			// when not first command
			} else if (cmdIndex > 1) {
				if (dup2(pipefd[(cmdIndex - 2) * 2], 0) == -1) {
					exit(1);
				}
			} // end of if/else

			// when we have a STDOUT redirect and last command to execute
			if ( (cmdIndex == cmdCount) && ((redirFd = ioArr[1]) != NULL) ) {
				iofd = open(redirFd, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
				if (dup2(iofd, 1) == -1) {
					exit(1);
				}
				close(iofd);

			// when not last command
			} else if (cmdIndex < cmdCount) {
				if (dup2(pipefd[(cmdIndex * 2) - 1], 1) == -1) {
					exit(1);
				}
			} // end of if/else

			// close all pipes
			for (tempIndex = 0; tempIndex < pipefdSize; tempIndex++) {
				close(pipefd[tempIndex]);
			}

			// generate proper exec arguments
			startIndex = indexArr[(cmdIndex - 1) * 2];
			endIndex = indexArr[(cmdIndex * 2) - 1];
			execFile = argsArr[startIndex];
			argsArr[endIndex + 1] = NULL; // array elements will not be read after the last relevant one

			// we only return upon failure of exec
			execChildArgs(argsArr, execFile, startIndex, endIndex, NULL);
			exit(0);

		} else if (child == -1) { 
			exit(1);

		} // end of outer if/else

	} // end of for

	// close pipes in parent
	for (tempIndex = 0; tempIndex < pipefdSize; tempIndex++) {
		close(pipefd[tempIndex]);
	}

	// wait for children
	for (tempIndex = 0; tempIndex < cmdCount; tempIndex++) {
		wait(&status);
	}

}


/* Function attempts to execute commands for which a corresponding
	exec call exists for
*/

void execChildArgs(char ** argsArr, char * execFile, int startIndex, int endIndex, char ** ioArr) {
	// printf("%s\n", execFile);
	if (strcmp(execFile, "cd") == 0) {
		cd(argsArr, startIndex);

	} else if (strcmp(execFile, "pwd") == 0) {
		pwd();

	} else if (strcmp(execFile, "echo") == 0) {
		echo(argsArr, startIndex, endIndex);

	} else if (strcmp(execFile, "mkdir") == 0) {
		mkDir(argsArr, startIndex);

	} else if (strcmp(execFile, "exit") == 0) {
		exit(0);

	// in the single command case this is not a child process,
	// so we need to fork since exec() replaces process
	} else if (ioArr != NULL) {
		pid_t child;
		int status;

		child = fork();
		if (child == 0) {
			// no piping taking place so direct
			// child I/0 redirection performed
			char * redirFd;
			int iofd;

			if ( (redirFd = ioArr[0]) != NULL ) {
				iofd = open(redirFd, O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR);
				if (dup2(iofd, 0) == -1) {
					exit(1);
				}
				close(iofd);
			}

			if ( (redirFd = ioArr[1]) != NULL ) {
				iofd = open(redirFd, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
				if (dup2(iofd, 1) == -1) {
					exit(1);
				}
				close(iofd);
			}

			execvp(execFile, argsArr+startIndex);
			// we only return upon failure of exec
			printf("Command not found.\nExecution unsuccessful.\n");
			exit(0);

		} else if (child == -1) { 
			exit(1);

		} else {
			wait(&status);

		}

	// we attempt an exec call of the arguments without forking
	} else {
		// since we are not addressing the file by filepath we use 'p'
		// since the arguments are in the form of an array we use 'v'
		// the command is the first element
		// and for the second argument we want all elements
		execvp(execFile, argsArr+startIndex);

		// we only return upon failure of exec
		printf("Command not found.\nExecution unsuccessful.\n");
	}

}


/* Function that finds absolute path from either
	relative or absolute
*/

char * getAbsolutePath(char * path) {
	if (path[0] != '/') { // relative path
		// allocate space for cwd and retrieve
		char cwd[120];
		getcwd(cwd, sizeof(cwd));
		// make relative path ready for concat
		strcat(cwd, "/");
		// make full path
		strcat(cwd, path);
		strcpy(path, cwd);
	} 
	return path;

}


/* Function that changes process cwd
*/

void cd(char ** argsArr, int startIndex) {
	int dirIndex = startIndex + 1;
	if (argsArr[dirIndex] != NULL) {
		char * path;
		path = getAbsolutePath(argsArr[dirIndex]);
		if ( chdir(path) == -1 ) {
			printf("Unable to change directory.\n");

		} else {
			printf("Directory changed.\n");

		}

	} else {
		printf("Unable to change directory.\n");

	}
}


/* Function that prints the current working directory
*/

void pwd(void) {
	char cwd[120];
	getcwd(cwd, sizeof(cwd));
	printf("%s\n", cwd);

}


/* Function that prints out strings succeeding the command directive
*/

void echo(char ** argsArr, int startIndex, int endIndex) {
	int tempIndex;
	for (tempIndex = startIndex+1; tempIndex <= endIndex; tempIndex++) {
		printf("%s", *(argsArr+tempIndex));

		if (tempIndex < endIndex) {
			printf("%c", ' ');
		}
	}
	printf("%c", '\n');

}


/* Function that creates a subdirectory within the current directory
*/

void mkDir(char ** argsArr, int startIndex) {
	int dirIndex = startIndex + 1;
	if (argsArr[dirIndex] != NULL) {
		char * path;
		path = getAbsolutePath(argsArr[dirIndex]);
		if ( mkdir(path, 0777) == -1 ) {
			printf("Unable to create directory.\n");

		} else {
			printf("Directory created.\n");

		}

	} else {
		printf("Unable to create directory.\n");

	}

}
