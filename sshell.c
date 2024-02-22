#include "sshell_utils.h"

int main(void)
{
	//Original command, and command with all instances of >> replaced with an append character and a space
	char cmd[CMDLINE_MAX];
	char parsedCmd[CMDLINE_MAX];

	//Event loop scanning for user inputs
	while (1) {
		/* Print prompt */
		printf("sshell@ucd$ ");
		fflush(stdout);

		/* Get command line */
		fgets(cmd, CMDLINE_MAX, stdin);

		/* Print command line if stdin is not provided by terminal */
		if (!isatty(STDIN_FILENO)) {
			printf("%s", cmd);
			fflush(stdout);
		}

		/* Remove trailing newline from command line */
		char *nl;
		nl = strchr(cmd, '\n');
		if (nl) *nl = '\0';

		//If the user entered only whitespace, discard the input without parsing and continue to the next input
		if (isEmptyCommand(cmd)) continue;

		//Store the user's original command, which will be output when the current command finishes execution.
		//Make a copy into parsedCmd, which will be edited by parseAppends, where all >> will be replaced.
		memcpy(parsedCmd, cmd, CMDLINE_MAX);

		//Get an array of Parameters and an array of pipes and file redirects
		struct Parameter ** parameters;
		char * redirects;
		char ** argArray;
		int parameterCount;

		int parseRetval = getParametersAndRedirects(parsedCmd, &parameters, &redirects, &argArray, &parameterCount);

		//Error handling: print an appropriate error message and discard current input if an error occurs
		if (parseRetval != NO_ERRORS) {
			switch (parseRetval) {
				case MISSING_COMMAND:
					fprintf(stderr, "Error: missing command\n");
					break;
				case NO_OUTPUT_FILE:
					fprintf(stderr, "Error: no output file\n");
					break;
				case MISPLACED_REDIRECT:
					fprintf(stderr, "Error: mislocated output redirection\n");
					break;
				case TOO_MANY_ARGUMENTS:
					fprintf(stderr, "Error: too many process arguments\n");
					break;
			}

			//No parameters are created in the case of a parsing error, so nothing needs to be freed
			continue;
		}

		//Builtin exit command
		if (!strcmp(parameters[0]->parameterName, "exit")) {
			//No memory deallocation is needed, as program immediately exists anyways
			fprintf(stderr, "Bye...\n+ completed '%s' [0]\n", cmd);
			break;
		}

		//Builtin cd command
		if (!strcmp(parameters[0]->parameterName, "cd")) {
			int retval = cd(parameters[0]);

			//Free the parameters, redirect array, and argument array
			freeParameterArray(parameters);
			free(redirects);
			freeArgArray(argArray);

			fprintf(stderr, "+ completed '%s' [%d]\n", cmd, retval);
			continue;
		}

		//Builtin sls command
		if (!strcmp(parameters[0]->parameterName, "sls")) {
			int retval = sls();

			//Free the parameters, redirect array, and argument array
			freeParameterArray(parameters);
			free(redirects);
			freeArgArray(argArray);

			fprintf(stderr, "+ completed '%s' [%d]\n", cmd, retval);
			continue;
		}

		//Builtin pwd command
		if (!strcmp(parameters[0]->parameterName, "pwd")) {
			int retval = pwd();

			//Free the parameters, redirect array, and argument array
			freeParameterArray(parameters);
			free(redirects);
			freeArgArray(argArray);

			fprintf(stderr, "+ completed '%s' [%d]\n", cmd, retval);
			continue;
		}
		
		//Determine output mode depending on the last redirect in the redirect array, with stdout by default
		int outputMode = WRITE_TO_STDOUT;
		if (parameterCount > 1) {
			if (redirects[parameterCount-2] == APPEND) {
				outputMode = APPEND_TO_FILE;
			}

			else if (redirects[parameterCount-2] == OVERWRITE) {
				outputMode = WRITE_TO_FILE;
			}
		}

		//If a file redirect is detected, set the output file to the final parameter's name.
		char * outputFile = NULL;
		if (outputMode != WRITE_TO_STDOUT) {
			outputFile = parameters[parameterCount-1]->parameterName;

			//Because the final parameter is a file, do not count it in the pipeline execution.
			parameterCount--;
		}

		//Fork the current execution, with the child executing the pipeline.
		//This is so the child can set output to either stdout or a file, without influencing future commands.
		if (fork()) {
			//Wait for child to finish execution
			int retval;
			wait(&retval);
		} else {
			//Try to set file output- if it fails, print an error message and exit
			if (stdoutRedirect(outputMode, outputFile)) {
				fprintf(stderr, "Error: cannot open output file\n");
				exit(1);
			}

			//Execute the pipeline and exit the child process
			executePipeline(parameters, parameterCount, cmd);
			exit(0);
		}

		//Free the parameters, redirect array, and argument array
		freeParameterArray(parameters);
		free(redirects);
		freeArgArray(argArray);
	}

	return EXIT_SUCCESS;
}
