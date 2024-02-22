#include "sshell_utils.h"

/*
 * Scans the user input and checks if the user enters only whitespace
 */
int isEmptyCommand(char * cmd) {
	for (int i = 0; cmd[i] != '\0'; i++) {
		/* Command is not empty if a non-whitespace character is detected */
		if (cmd[i] != ' ' && cmd[i] != '\t') {
			return 0;
		}
	}

	return 1;
}

/*
 * Checks whether a single string is a redirect to an output file
 */
int isFileRedirect(char * string) {
	return string[1] == '\0' && (string[0]==OVERWRITE || string[0]==APPEND);
}

/*
 * Checks whether a single string is a pipe to another command
 */
int isPipe(char * string) {
	return string[1] == '\0' && string[0]==PIPE;
}

/*
 * Checks whether a single string is a redirect or pipe
 */
int isRedirect(char * string) {
	return string[1] == '\0' && (string[0]==OVERWRITE || string[0]==APPEND || string[0]==PIPE);
}

/*
 * Deallocates the array of Parameter structs, when finished executing the current pipeline
 */
void freeParameterArray(struct Parameter * parameters[]){
	for (int i=0; parameters[i]!=NULL; i++) {
		/* Individual arguments are freed within freeArgArray, and only the structs are freed in this function */
		free(parameters[i]->arguments);
		free(parameters[i]);
	}
	
	free(parameters);
}

/*
 * Deallocates the array of single parameters, when finished executing the current pipeline
 */
void freeArgArray(char ** args){
	for (int i=0; args[i] != NULL; i++) {
		free(args[i]);
	}

	free(args);
}

/*
 * Counts the amount of parameters, by counting the number of redirect and pipe characters.
 * Every single command, with the exception of the last command, is followed by a pipe character.
 * The amount of parameters can be calculated by the number of redirects and pipes, plus one.
 * The case where there are zero parameters, when the user enters a blank command, is handled by prior error checking.
 */
int countParameters(char ** args) {
	int numRedirects = 0;
	for (int i=0; args[i] != NULL; i++) {
		if (isRedirect(args[i])) {
			numRedirects++;
		}
	}

	return numRedirects+1;
}

/*
 * Parses an array of individual arguments into an array of Parameter objects, along with a list of redirects and pipes
 */
void argsToParameterList(char ** args, int numParameters, struct Parameter ** parameters[], char * redirects[]){
	/* Allocate array of Parameter pointers, and set its last element to a null pointer */
	struct Parameter ** paramArray = malloc(sizeof(struct Parameter *) * (numParameters+1));
	paramArray[numParameters] = NULL;

	/* Allocate array of redirect characters, and set its last element to a null character */
	int numRedirects = numParameters-1;
	char * redirections = malloc(sizeof(char) * (numRedirects+1));
	redirections[numRedirects] = '\0';

	/* Split the array of arguments into individual Parameter structs, using redirects and pipes as delimiters */
	int parameterIndex = 0;
	int left = 0, right;

	for (right=0; args[right] != NULL; right++) {
		/* Upon encountering a redirect, put the range of the parameter into a struct */
		if (isRedirect(args[right])) {
			/* Allocate an array for a single command's arguments */
			int curLength = right-left;
			char ** curArgs = malloc(sizeof(char*) * (curLength+1));
			curArgs[curLength] = NULL;

			/* Add arguments within range to the single command's argument array */
			for (int i=0; i<curLength; i++) {
				curArgs[i] = args[left+i];
			}

			/* Create a new Parameter struct and assign the argument array to the object */
			struct Parameter * curParameter = malloc(sizeof(struct Parameter));
			curParameter->parameterName = curArgs[0];
			curParameter->arguments = curArgs;

			/* Add the newly-created Parameter to the array of Parameter pointers */
			paramArray[parameterIndex] = curParameter;

			/* Add the current redirect/pipe to the list of redirect/pipe characters */
			redirections[parameterIndex] = (char)(args[right][0]);

			parameterIndex++;
			left = right+1;
		}
	}

	/* The final Parameter is not terminated by a redirect, so it cannot be detected in the loop */
	/* Allocate an array for a single command's arguments */
	int curLength = right-left;
	char ** curArgs = malloc(sizeof(char*) * (curLength+1));
	curArgs[curLength] = NULL;

	/* Add arguments within range to the single command's argument array */
	for (int i=0; i<curLength; i++) {
		curArgs[i] = args[left+i];
	}

	/* Create a new Parameter struct and assign the argument array to the object */
	struct Parameter * curParameter = malloc(sizeof(struct Parameter));
	curParameter->parameterName = curArgs[0];
	curParameter->arguments = curArgs;

	/* Add the newly-created Parameter to the array of Parameter pointers */
	paramArray[parameterIndex] = curParameter;

	/* Return the array of Parameters and the redirect/pipe list */
	*parameters = paramArray;
	*redirects = redirections;
}

/*
 * Replaces every instance of a file append >> with a single character APPEND, followed by a space.
 * This way, every single redirect and pipe is a single character, which simplified parsing.
 * Returns an error code if there is a parsing error.
 */
int parseAppends(char cmd[]) {
	int left = 0, right;

	/* Keeps track of if a current character is a file redirect character > */
	int onAppendChar = 0;

	/* Scans through every character in the current command, looking for instances of > */
	for (right = 0; cmd[right] != '\0'; right++) {
		/* When detecting a new >, set the left pointer to the current index */
		if (cmd[right] == OVERWRITE) {
			if (!onAppendChar) {
				onAppendChar = 1;
				left = right;
			}
		}

		/* When detecting a non-redirect character that follows a redirect character */
		else if (onAppendChar) {
			onAppendChar = 0;

			/* Determine whether the sequence of redirect characters should be treated as: */
			/* A misplacement, where there are three or more redirects in a row */
			if (right-left > 2) {
				return MISPLACED_REDIRECT;
			}
			/* An append, where there are two redirects in a row */
			if (right-left == 2) {
				cmd[left] = APPEND;
				cmd[left+1] = ' ';
			}
			/* Otherwise, there is only a single redirect, and it is unchanged. */
		}
	}
	return NO_ERRORS;
}

/*
 * Count the number of arguments in a command input by the user.
 * This number is dependent on the number of whitespace separating the words, in addition to the number of redirect characters.
 * Redirect characters may or may not have whitespace between them and other arguments.
 * This function additionally accounts for quotation marks, which treat all arguments within the range of quotations as a single argument.
 */
int countArgs(char cmd[]) {
	/* Flags checking whether or not the current characters is part of a quotation mark substring or part of an argument */
	int inQuotation = 0;
	int curArg = 0;

	int numArgs = 0;
	for (int i=0; cmd[i] != '\0'; i++) {
		char cur = cmd[i];		

		/* When finding a quotation mark, invert the inQuotation flag */
		if (cur == '\"') {
			inQuotation = 1-inQuotation;
		}

		/* Upon encountering whitespace */
		else if (cur == ' ' || cur == '\t') {
			/* If the whitespace is contained in quotation, ignore it and treat it as part of a single parameter */
			if (inQuotation) continue;

			/* If there are repeated whitespaces, treat it as a single whitespace... */
			/* Only increase argument counter if there is a non-whitespace argument prior. */
			if (curArg) {
				numArgs++;
				curArg = 0;
			}
		}

		/* Upon encountering a pipe or redirect */
		else if (cur == PIPE || cur == OVERWRITE || cur == APPEND) {
			/* Increase argument counter if on a current parameter */
			if (curArg) numArgs++;

			/* Increase argument counter regardless, because the pipe or redirect is a parameter */
			numArgs++;
			curArg = 0;
		}

		/* Otherwise, when encountering a regular character, it is part of an argument. */
		else curArg = 1;
	}

	/* The very last character has no whitespace following it. Increase argument count if the curArg flag is true */
	if (curArg) {
		numArgs++;
	}

	/* If there are an odd number of quotation marks, return an invalid amount of arguments */
	if (inQuotation) return -1;
	else return numArgs;
}

/*
 * Error checking for the array of arguments. This function is run before the arguments are split into Parameter structs.
 */
int checkValidArgs(char ** argArray, int numArgs) {
	/* Error: number of arguments greater than the maximum allowed amount */
	if (numArgs > MAX_ARGUMENTS) {
		return TOO_MANY_ARGUMENTS;
	}

	/* Error: the first argument is a redirect or pipe */
	if (isRedirect(argArray[0])) {
		return MISSING_COMMAND;
	}

	/* Error: the final argument is a file redirect */
	if (isFileRedirect(argArray[numArgs-1])) {
		return NO_OUTPUT_FILE;
	}

	/* Error: the final argument is a pipe */
	else if (isPipe(argArray[numArgs-1])) {
		return MISSING_COMMAND;
	}

	/* Scan the arguments to ensure that redirects and pipes are in the right place, using two flags. */
	/* Flags checking whether or not a file redirect has been found, and if the previous element is a pipe or file redirect */
	int foundOutputRedirect = 0;
	int prevIsRedirect = 0;

	for (int i=1; i<numArgs; i++){
		char * curArg = argArray[i];

		/* Error: the current and previous elements are both pipes or redirects */
		int curIsRedirect = isRedirect(curArg);
		if (prevIsRedirect && curIsRedirect) {
			return MISSING_COMMAND;
		}
		prevIsRedirect = curIsRedirect;

		/* Error: there is a file redirect or pipe after a file redirect */
		if (isRedirect(curArg) && foundOutputRedirect) return MISPLACED_REDIRECT;
		else if (isFileRedirect(curArg)) foundOutputRedirect = 1;
	}

	return NO_ERRORS;
}


/*
 * Allocates and returns a pointer to a single substring from the command, given left and right bounds
 */
char * getSubstring(char cmd[], int leftInc, int rightEx) {
	int length = rightEx - leftInc;

	char * substring = malloc(sizeof (char) * (length+1));
	substring[length] = '\0';

	strncpy(substring, cmd+leftInc, length);

	return substring;
}

/*
 * Splits the command into individual argument substrings, separating on whitespace or redirects/pipes
 */
char ** parseArgs(char cmd[], int numArgs) {
	/* Create an array to store substrings in, terminated with a null pointer */
	char ** res = malloc((sizeof (char*)) * (numArgs+1));
	res[numArgs] = NULL;

	int cmdIndex = 0;
	int left = 0, right;

	/* Find first parameter */
	while (left < CMDLINE_MAX && (cmd[left]==' ' || cmd[left]=='\t')) left++;

	/* Flags checking whether or not the current characters is part of a quotation mark substring or part of an argument */
	int inQuotation = 0;
	int curArg = 0;

	for (right=left; cmd[right] != '\0'; right++) {
		char cur = cmd[right];

		/* When finding a quotation mark, invert the inQuotation flag */
		if (cur == '\"') {
			inQuotation = 1-inQuotation;
		}

		/* Upon encountering whitespace */
		else if (cur == ' ' || cur == '\t') {
			/* If the whitespace is contained in quotation, ignore it and treat it as part of a single parameter */
			if (inQuotation) continue;

			/* If there are repeated whitespaces, treat it as a single whitespace... */
			/* Only increase argument counter if there is a non-whitespace argument prior. */
			if (curArg) {
				res[cmdIndex] = getSubstring(cmd, left, right);
				cmdIndex++;
				curArg = 0;
			}
		}

		/* Upon encountering a pipe or redirect */
		else if (cur == PIPE || cur == OVERWRITE || cur == APPEND) {
			/* Increase argument counter and save to array if on a current parameter */
			if (curArg) {
				res[cmdIndex] = getSubstring(cmd, left, right);
				cmdIndex++;
			}

			/* Increase argument counter regardless and save to array, because the pipe or redirect is a parameter */
			res[cmdIndex] = getSubstring(cmd, right, right+1);
			cmdIndex++;
			curArg = 0;
		}

		/* Otherwise, when encountering a regular character, it is part of an argument. */
		else {
			if (curArg == 0) {
				left = right;
				curArg = 1;
			}
		}
	}

		/* The very last character has no whitespace following it. Increase argument count if the curArg flag is true */
	if (curArg) {
		res[cmdIndex] = getSubstring(cmd, left, right);
	}

	return res;
}

/*
 * Builtin sls command
 */
int sls() {
	/* Try to open the current directory */
	DIR *curDirectory;
	curDirectory = opendir(".");

	if (curDirectory == NULL) {
		fprintf(stderr, "Error: cannot open directory\n");
		return 1;
	}

	/* Iterate through entries in current directory */
	struct dirent *entry;
	while ((entry = readdir(curDirectory)) != NULL) {
		char * name = entry->d_name;

		/* Ignore the current directory and the parent directory */
		if (name[0] == '.') continue;

		/* Get the stats of the current entry */
		struct stat curFile;
		stat(name, &curFile);
		long long fileSize = curFile.st_size;

		printf("%s (%lld bytes)\n", name, fileSize);
	}

	closedir(curDirectory);
	return 0;
}

/*
 * Builtin cd command
 */
int cd(struct Parameter * parameter) {
	/* Attempt to cd into directory, and print error message if unable to */
	if (parameter->arguments[1] == NULL || chdir(parameter->arguments[1])) {
		fprintf(stderr, "Error: cannot cd into directory\n");
		return 1;
	}
	
	else return 0;
}

/*
 * Builtin pwd command
 */
int pwd() {
	/* Get and print the current working directory */
	char cwd[PATH_MAX];
	getcwd(cwd, sizeof(cwd));
	printf("%s\n", cwd);

	return 0;
}

/*
 * Parse the user's command into an array of Parameters and an array of redirect characters, along with error checking
 */
int getParametersAndRedirects(char cmd[], struct Parameter ** parameters[], char * redirects[], char ** argArray[], int * parameterCount) {
	/* Replace every instance of append >> with a single character followed by a space, for easier parsing */
	int parseRetval = parseAppends(cmd);
	if (parseRetval != NO_ERRORS) return parseRetval;

	/* Count the number of arguments in order to allocate arrays */
	int numArgs = countArgs(cmd);
	if (numArgs == -1) return MISSING_COMMAND;

	/* Parse arguments into an array of substrings */
	char ** args = parseArgs(cmd, numArgs);

	/* Ensure that there are no parsing errors, such as misplaced pipes or redirects */
	/* Free argument array if an error occurs */
	int checkRetval = checkValidArgs(args, numArgs);
	if (checkRetval != NO_ERRORS) {
		freeArgArray(args);
		return checkRetval;
	}

	/* Count the number of individual commands to be parsed as Parameters */
	int numParameters = countParameters(args);

	/* Convert the command array into an array of parameters and redirects */
	argsToParameterList(args, numParameters, parameters, redirects);

	*parameterCount = numParameters;
	*argArray = args;
	return NO_ERRORS;
}

/*
 * Executes the entire pipeline
 */
void executePipeline(struct Parameter ** commands, int numCommands, char * cmd) {
	/* Create arrays of pipes, PIDs, and return values */
	int pipefds[numCommands - 1][2];
	pid_t childPIDs[numCommands];

	/* Default all return values to 0, assume successful execution */
	int retvals[numCommands];
	memset(retvals, 0, numCommands * sizeof(int));

	/* Initialize pipes, but exit if any error occurs during initialization */
	for (int i = 0; i < numCommands - 1; i++) {
		if (pipe(pipefds[i]) == -1) {
			perror("pipe");
			exit(1);
		}
	}

	/* Connect every command to a pipe */
	for (int i = 0; i < numCommands; i++) {
		pid_t processID;

		processID = fork();

		/* Child process */
		if (processID == 0) {
			/* Close unnecessary pipe file descriptors */
			if (i < numCommands - 1) {
				/* Close read end of the pipe */
				close(pipefds[i][0]);

				/* Redirect stdout to the write end of the pipe */
				dup2(pipefds[i][1], STDOUT_FILENO); 
				close(pipefds[i][1]);
			}

			if (i > 0) {
				/* Close write end of the pipe */
				close(pipefds[i-1][1]);

				/* Redirect stdin to the read end of the previous pipe */
				dup2(pipefds[i-1][0], STDIN_FILENO);
				close(pipefds[i-1][0]);
			}

			/* Close other pipes not being used */
			for (int j = 0; j < numCommands - 1; j++) {
				if (j != i) {
					close(pipefds[j][0]);
					close(pipefds[j][1]);
				}
			}

			/* Execute the command */
			retvals[i] = execvp(commands[i]->parameterName, commands[i]->arguments);

			/* If control returns to the child, execvp has failed */
			fprintf(stderr, "Error: command not found\n");
			exit(1);
		}

		/* Parent process - save the child's PID */
		else if (processID > 0) {
			childPIDs[i] = processID;
		}

		/* Fork error, return immediately */
		else {
			perror("fork");
			exit(1);
		}
	}

	/* Close all pipes in the parent process */
	for (int i = 0; i < numCommands - 1; i++) {
		close(pipefds[i][0]);
		close(pipefds[i][1]);
	}

	/* Wait for all children to finish executing */
	for (int i = 0; i < numCommands; i++) {
		waitpid(childPIDs[i], &(retvals[i]), 0);
	}

	/* Print completed message, followed by the exit statuses of all the processes */
	fprintf(stderr, "+ completed '%s' ", cmd);
	for (int i=0; i<numCommands; i++) {
		fprintf(stderr, "[%d]", WEXITSTATUS(retvals[i]));
	}

	fprintf(stderr, "\n");
}

/*
 * Redirect stdout, with the given parameters
 */
int stdoutRedirect(int outputMode, char * outputFile) {
	/* Redirect output to a file if not writing to stdout- otherwise, do nothing, as output goes to stdout by default */
	if (outputMode != WRITE_TO_STDOUT) {
		/* Open the output file with truncate or append, depending on the parameters */
		int fd;
		if (outputMode == WRITE_TO_FILE) fd = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		else if (outputMode == APPEND_TO_FILE) fd = open(outputFile, O_WRONLY | O_CREAT | O_APPEND, 0644);

		/* If unable to open the file, return */
		if (fd == -1) {
			return 1;
		}

		/* Redirect stdout to the file */
		dup2(fd, STDOUT_FILENO);
		close(fd);
	}
	return 0;
}