/* Programmer: Gregory Smith
	Date: 03/20/2023
	Program: PS2
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
	https://man7.org/linux/man-pages/man2/dup.2.html,
	https://linux.die.net/man/3/strcat,
	https://linux.die.net/man/3/strcmp
*/

/* Purpose:
	This program recreates some basic shell functionality, including
	direct system calls with exec as well as some defined functions
	to recreate some other expected commands such as cat, cd, and exit
*/


/* Headers */
#include <stdio.h> // needed for getline(), printf()
#include <string.h> // needed for strtok(), strcmp()
#include <stdlib.h> // needed for malloc(), free(), exit()
#include <sys/wait.h> // needed for waitpid()
#include <unistd.h> // needed for execvp(), fork(), chdir(), dup2()


/* Prototypes */
char * getInput(void);
char ** getArgs(char *);
void execArgs(char **);
void execChildArgs(char **);
void cd(char *);
char * getAbsolutePath(char *);
void cat(char *);


int main(void) {
	char * input;
	char ** argsArr;

	while (1) {
		// allocate and populate input command in buffer
		input = getInput();

		// retrieve split of input for execution
		argsArr = getArgs(input);

		// attempt execution
		execArgs(argsArr);

		// free buffered variables
		free(input);
		free(argsArr);

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

char ** getArgs(char * input) {
	// let us use the default buffer size of getline(), 120, as a baseline
	// TODO: have buffer dynamically expand
	char ** tokenArr = malloc(sizeof(char) * 120);

	// by implementation getline() will result in \n terminated string
	const char delim[2] = " \n";

	// we iterate through all of the tokens and put them in the 2-d array
	char * token = strtok(input, delim);
	int index = 0;
	while (token != NULL ) {
		tokenArr[index++] = token;
		// go to next token
		token = strtok(NULL, delim);
	}

	return tokenArr;
}


/* Function attempts to execute user inputted command
*/

void execArgs(char ** argsArr) {
	char * firstArg = argsArr[0];

	if (strcmp(firstArg, "cd") == 0) {
		if (argsArr[1] != NULL) {
			char * path;
			path = getAbsolutePath(argsArr[1]);
			chdir(path);

		} else {
			printf("Unable to change directory.\n");

		}

	} else if (strcmp(firstArg, "exit") == 0) {
		exit(0);

	// } else if (strcmp(firstArg, "cat") == 0) {
	// 	cat(argsArr[1]);

	} else { // we attempt an exec call of the arguments
		execChildArgs(argsArr);

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
		if (strcmp(argsArr[0], "cat") == 0) {
			char * path = getAbsolutePath(argsArr[1]);
			execl("/bin/cat", path, (char *) NULL);
		}

		// since we are not addressing the file by filepath we use 'p'
		// since the arguments are in the form of an array we use 'v'
		execvp(argsArr[0], argsArr);

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
