/* Programmer: Gregory Smith
	Date: 03/28/2023
	Program: PA3
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


/* Prototypes */
char * getInput(void);
void getArgs(char *, char **, char **, int *, int *);
void execArgs(char **, char **, int *, int);
void execChildArgs(char **, char **, int *, int);
void cd(char **);
char * getAbsolutePath(char *);


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


/* Function attempts to execute user inputted command
*/

void execArgs(char ** argsArr, char ** ioArr, int * indexArr, int cmdCount) {
	// no commands given
	if (indexArr[1] == -1) {
		return;
	}

	char * firstArg = argsArr[0];
	if (strcmp(firstArg, "cd") == 0) {
		cd(argsArr);

	} else if (strcmp(firstArg, "exit") == 0) {
		exit(0);

	} else { // we attempt an exec call of the arguments
		execChildArgs(argsArr, ioArr, indexArr, cmdCount);

	}

}


/* Function attempts to execute commands for which a corresponding
	exec call exists for
*/

void execChildArgs(char ** argsArr) {
	pid_t child;
	int status;
	child = fork();

	if (child == 0) { // child process
		// child I/0 redirection
		int redirIndex;
		char * redirFd;
		for (redirIndex = 0; redirIndex < 2; redirIndex++) {
			if ( (redirFd = argsArr[redirIndex]) != NULL ) {
				int fd = open(argsArr[redirIndex], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
				dup2(fd, redirIndex);
				close(fd);
			}
		}

		// since we are not addressing the file by filepath we use 'p'
		// since the arguments are in the form of an array we use 'v'
		// the first two elements are possible redirection, so the command is second element
		// and for the second argument we want all elements except first two
		execvp(argsArr[2], argsArr+2);

		// we only return upon failure of exec
		printf("Command not found.\nExecution unsuccessful.\n");
		exit(0);

	} else { // parent process
		waitpid(child, &status, 0);

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

void cd(char ** argsArr) {
	if (argsArr[1] != NULL) {
		char * path;
		path = getAbsolutePath(argsArr[1]);
		if ( chdir(path) == -1 ) {
			printf("Unable to change directory.\n");

		} else {
			printf("Directory changed.\n");

		}

	} else {
		printf("Unable to change directory.\n");

	}
}
