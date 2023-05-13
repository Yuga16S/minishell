#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h> 
#include <stdbool.h>

int executor(char[], int, int, int, int[][2]);

int main () {

	while (1) {
		SHELL_LOOP:printf("ms$ ");

		char input[1000];
		fgets(input, 1000, stdin); //NOTE: fgets adds a newline character at the end of the string

		int numberOfSpecialCharacterTypes = 0;
		if (strstr(input, "|") != NULL && strstr(input, "||") == NULL) {
			numberOfSpecialCharacterTypes++; // contains pipe
		}

		if (strstr(input, "<") != NULL || strstr(input, ">") || strstr(input, ">>")) {
			numberOfSpecialCharacterTypes++;
		}

		if (strstr(input, "&&") != NULL || strstr(input, "||") != NULL) {
			numberOfSpecialCharacterTypes++;
		}

		if ((strstr(input, "&") != NULL) && (strstr(input, "&&") == NULL)) {
			numberOfSpecialCharacterTypes++;
		}

		if (strstr(input, ";") != NULL) {
			numberOfSpecialCharacterTypes++;
		}

		if (numberOfSpecialCharacterTypes > 1) {
			printf("Input contains more than 1 different types of special characters\n");
			goto SHELL_LOOP;
		}

		if (strstr(input, "|") != NULL && strstr(input, "||") == NULL) { // piping operation to not confuse with logical OR (||)
			int numberOfPipes = 0;
			for (int i = 0; i < strlen(input); i++) {
				if (input[i] == '|') {
					numberOfPipes++;
				}
			}

			if (numberOfPipes > 5) {
				printf("Input exceeds maximum number of pipes (5).\n");
				goto SHELL_LOOP;
			}

			char * subInputs[numberOfPipes + 1];
			for (int i = 0; i < numberOfPipes+1 ; i++) {
				subInputs[i] = strtok(i == 0 ? input : NULL, "|");
		    }
 
		    //printf("Found %d number of pipes\n", numberOfPipes);

		    int fd[numberOfPipes][2]; // Using a 2d Array to store file descriptors from the pipe(s).
		    for (int i = 0; i < numberOfPipes; i++) {
		    	if (pipe(fd[i]) == -1) {
		    		printf("Error creating pipe \n");
		    		exit(0);
		    	}

		    	//printf("Pipe %d created with fds %d %d \n", i, fd[i][0], fd[i][1]);
		    }

		    int recentChildPid;
		    for (int i = 0; i < numberOfPipes + 1; i++) {
		    	//printf("executing %s \n", subInputs[i]);

		    	int inputFd = (i == 0) ? 0 : fd[i-1][0];
		    	int outputFd = (i == numberOfPipes) ? 1 : fd[i][1];

		    	recentChildPid = executor(subInputs[i], numberOfPipes, inputFd, outputFd, fd);
		    	if (recentChildPid == -1) {
					goto SHELL_LOOP;
				}
		    }

		    /**
		     * NOTE:
		     * Closing of pipes in main should only happen after creating all the child processes so that the child processes 
		     * are able to inherit the file descriptors of the pipe BEFORE they are closed.
			*/
			for (int i = 0; i < numberOfPipes; i++) {
				close(fd[i][0]);
				close(fd[i][1]);
			}

			//Wait for the most recent child to complete execution and not for each child process because the piping occurs over a continous stream
		    waitpid(recentChildPid, NULL, 0);

		} else if (strstr(input, "<") != NULL) { // input redirection
			char * subInput = strtok(input, "<");
			char * filename = strtok(NULL, "<");

			if (filename[strlen(filename) - 1] == '\n') { // trailing new line from fgets (this is normally handled in the executor())
				filename[strlen(filename) - 1] = ' '; // this space is handled by strtok that follows
			}

			filename = strtok(filename, " "); //ignoring spaces that are not permitted in filename
			//printf("File name is:[%s]test\n", filename);

			int fd = open(filename, O_RDONLY);
			int childPid = executor(subInput, 0, fd, 1, NULL);
			if (childPid == -1) {
				goto SHELL_LOOP;
			}
			waitpid(childPid, NULL, 0);
		} else if (strstr(input, ">>") != NULL) { // redirection output append
			char * subInput = strtok(input, ">>");
			char * filename = strtok(NULL, ">>");

			if (filename[strlen(filename) - 1] == '\n') { // trailing new line from fgets (this is normally handled in the executor())
				filename[strlen(filename) - 1] = ' '; // this space is handled by strtok that follows
			}

			filename = strtok(filename, " "); //ignoring spaces that are not permitted in filename
			//printf("File name is:[%s]test\n", filename);

			int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0777);
			int childPid = executor(subInput, 0, 0, fd, NULL);
			if (childPid == -1) {
				goto SHELL_LOOP;
			}
			waitpid(childPid, NULL, 0);
		} else if (strstr(input, ">") != NULL) { // redirection output 
			char * subInput = strtok(input, ">");
			char * filename = strtok(NULL, ">");

			if (filename[strlen(filename) - 1] == '\n') { // trailing new line from fgets (this is normally handled in the executor())
				filename[strlen(filename) - 1] = ' '; // this space is handled by strtok that follows
			}

			filename = strtok(filename, " "); //ignoring spaces that are not permitted in filename
			//printf("File name is:[%s]test\n", filename);

			int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
			int childPid = executor(subInput, 0, 0, fd, NULL);
			if (childPid == -1) {
				goto SHELL_LOOP;
			}
			waitpid(childPid, NULL, 0);

		} else if ((strstr(input, "&") != NULL) && (strstr(input, "&&") == NULL)) { // background
			char * subInput = strtok(input, "&");
			executor(subInput, 0, 0, 1, NULL);

		} else if ((strstr(input, ";") != NULL)) { // sequential
			int semicolonCount = 0;
			for (int i = 0; i < strlen(input); i++) {
				if (input[i] == ';') {
					semicolonCount++;
				}
			}

			if (semicolonCount > 4) {
				printf("Input contains more than 5 sequential commands \n");
				goto SHELL_LOOP;
			}

			char * subInputs[semicolonCount + 1];
			for (int i = 0; i < semicolonCount + 1; i++ ) {
				subInputs[i] = strtok(i == 0 ? input : NULL,  ";");
			}

			for (int i = 0; i < semicolonCount + 1; i++ ) {
				int childPid = executor(subInputs[i], 0, 0, 1, NULL);
				if (childPid == -1) {
					goto SHELL_LOOP;
				}
				waitpid(childPid, NULL, 0);				
			}

		} else if (strstr(input, "&&") != NULL || strstr(input, "||") != NULL) { // conditional
			int numberOfTokens = 0;
			bool tokens[5]; // stores 0 for || and 1 for &&

			char * inputToCheck = input;
			while (strstr(inputToCheck, "&&") != NULL || strstr(inputToCheck, "||") != NULL) {
				if (numberOfTokens == 5) {
					printf("Input contains more than 5 conditional operators\n");
					goto SHELL_LOOP;
				}

				char* andSubString = strstr(inputToCheck, "&&");
				char* orSubString = strstr(inputToCheck, "||");

				if (andSubString == NULL && orSubString != NULL) { // only or condition exists in the substring
					tokens[numberOfTokens] = 0;
					numberOfTokens++;
					inputToCheck = orSubString + 2; //skipping past ||
				} else if (andSubString != NULL && orSubString == NULL) { // only and condition exists in substring
					tokens[numberOfTokens] = 1;
					numberOfTokens++;
					inputToCheck = andSubString + 2; //skipping past &&
				} else { // both or and add condition exists
					if (strlen(andSubString) > strlen(orSubString)) { // and comes first 
						tokens[numberOfTokens] = 1;
						numberOfTokens++;
						inputToCheck = andSubString + 2; //skipping past &&
					} else { // or comes first
						tokens[numberOfTokens] = 0;
						numberOfTokens++;
						inputToCheck = orSubString + 2; //skipping past ||
					}
				}

				//printf("Encounted token #%d - %d\n", numberOfTokens, tokens[numberOfTokens -1]);
			}

			char * subInputs[numberOfTokens + 1];
			bool prevOperationSuccess = true;
			for (int i = 0; i < (numberOfTokens +  1); i++) {
				char * subInput = strtok(i == 0 ? input : NULL, "&&||");
				subInputs[i] = subInput;
			}

			for (int i = 0; i < (numberOfTokens + 1); i++) {
				char * subInput = subInputs[i];
				if (i != 0) {
					if (tokens[i-1] == 1 && ! prevOperationSuccess) { // prev token was && and the prev operation was failure
						break; 
					} 

					if (tokens[i-1] == 0 && prevOperationSuccess) { // prev token was || and the prev operation was success
						break;
					}
				}
				int status;
				int childPid = executor(subInput, 0, 0, 1, NULL);
				if (childPid == -1) {
					goto SHELL_LOOP;
				}
				waitpid(childPid, &status, 0);

				prevOperationSuccess = WIFEXITED(status) && WEXITSTATUS(status) == 0;
			}
		} else {
			//Wait for the current command to complete execution before showing the shell prompt again
			int childPid = executor(input, 0, 0, 1, NULL);
			if (childPid == -1) {
				goto SHELL_LOOP;
			}
			waitpid(childPid, NULL, 0);
		}
	}

}


int executor(char input[], int numberOfPipes, int inputFd, int outputFd, int fd[][2]) {
	int numberOfSpaces = 0;
	char prevChar = '\0';

	/**
	 * NOTE: The following block of code counts the number of spaces in between arguments and at the end (trailing space.)
	 * It DOES NOT count the leading whitespace(s) and treats a block of whitespace as 1. 
	 * It also replaces any trailing new line character (that is supposedly obtained from fgets) to a space character.
	*/ 
	for (int i = 0; i < strlen(input); i++) {
		if (input[i] == ' ' && prevChar != ' ' && prevChar != '\0') {
			numberOfSpaces++;
		} else if (input[i] == '\n') { // replacing the trailing new line character to space
			input[i] = ' ';

			if (prevChar != ' ') {
				numberOfSpaces++; 
			}
		}

		prevChar = input[i];
	}

	//printf("Executing command [%s] with spaces %d\n", input, numberOfSpaces);

	int argc = numberOfSpaces + 1;
	if (strlen(input) == 0) {
		argc = 0;
	}  else {
		if (isspace(input[strlen(input) - 1])) { // trailing space
			argc--;
		}
	}
	

	//printf("Number of args %d \n", argc);

	if(argc > 6) {
		printf("Invalid number of arguments \n");
		return -1;
	} else {

		char * args[argc+1]; // +1 to store NULL in the last index of the array

		int i = 0;
		for (i = 0; i < argc; i++) {
			args[i] = strtok(i == 0 ? input : NULL, " ");
		}
		args[i] = NULL;

		int childPid = fork();

		if (childPid == 0) { // child

			if (inputFd != 0) {
				dup2(inputFd, 0);
				close(inputFd);
			}

			if (outputFd != 1) {
				dup2(outputFd, 1);
				close(outputFd);
			}

			if (fd != NULL) {
				for (int i = 0; i < numberOfPipes; i++) {
					close(fd[i][0]);
					close(fd[i][1]);
				}
			}

			execvp(args[0], args); 

			//NOTE: execvp never returns on success so the below two lines are in case of error
			printf("error executing : %s\n", input);
			exit(1);
		} else {
			// main simply returns
			return childPid;
		}
	}
}


