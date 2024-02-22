#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGUMENTS 16
#define CMDLINE_MAX 512
#define APPEND '\n'
#define OVERWRITE '>'
#define PIPE '|'

/*
 * Output options: printing to stdout, and writing or appending to an output file
 */
enum OutputOptions {
	WRITE_TO_STDOUT,
	WRITE_TO_FILE,
	APPEND_TO_FILE
};

/*
 * Errors occurring during command parsing
 */
enum ParseErrors{
	NO_ERRORS,
	MISSING_COMMAND,
	NO_OUTPUT_FILE,
	MISPLACED_REDIRECT,
	TOO_MANY_ARGUMENTS
};

/*
 * Struct that represents a single command, that is separated from other commands by pipes or redirects
 */
struct Parameter {
	char * parameterName;
	char ** arguments;
};

/*
 * Scans the user input and checks if the user enters only whitespace
 */
int isEmptyCommand(char * cmd);

/*
 * Checks whether a single string is a redirect to an output file
 */
int isFileRedirect(char * string);

/*
 * Checks whether a single string is a pipe to another command
 */
int isPipe(char * string);

/*
 * Checks whether a single string is a redirect or pipe
 */
int isRedirect(char * string);

/*
 * Deallocates the array of Parameter structs, when finished executing the current pipeline
 */
void freeParameterArray(struct Parameter * parameters[]);

/*
 * Deallocates the array of single parameters, when finished executing the current pipeline
 */
void freeArgArray(char ** args);
/*
 * Counts the amount of parameters, by counting the number of redirect and pipe characters.
 * Every single command, with the exception of the last command, is followed by a pipe character.
 * The amount of parameters can be calculated by the number of redirects and pipes, plus one.
 * The case where there are zero parameters, when the user enters a blank command, is handled by prior error checking.
 */
int countParameters(char ** args);
/*
 * Parses an array of individual arguments into an array of Parameter objects, along with a list of redirects and pipes
 */
void argsToParameterList(char ** args, int numParameters, struct Parameter ** parameters[], char * redirects[]);

/*
 * Replaces every instance of a file append >> with a single character APPEND, followed by a space.
 * This way, every single redirect and pipe is a single character, which simplified parsing.
 * Returns an error code if there is a parsing error.
 */
int parseAppends(char cmd[]);

/*
 * Count the number of arguments in a command input by the user.
 * This number is dependent on the number of whitespace separating the words, in addition to the number of redirect characters.
 * Redirect characters may or may not have whitespace between them and other arguments.
 * This function additionally accounts for quotation marks, which treat all arguments within the range of quotations as a single argument.
 */
int countArgs(char cmd[]);

/*
 * Error checking for the array of arguments. This function is run before the arguments are split into Parameter structs.
 */
int checkValidArgs(char ** argArray, int numArgs);


/*
 * Allocates and returns a pointer to a single substring from the command, given left and right bounds
 */
char * getSubstring(char cmd[], int leftInc, int rightEx);

/*
 * Splits the command into individual argument substrings, separating on whitespace or redirects/pipes
 */
char ** parseArgs(char cmd[], int numArgs);

/*
 * Builtin sls command
 */
int sls();

/*
 * Builtin cd command
 */
int cd(struct Parameter * parameter);

/*
 * Builtin pwd command
 */
int pwd();

/*
 * Parse the user's command into an array of Parameters and an array of redirect characters, along with error checking
 */
int getParametersAndRedirects(char cmd[], struct Parameter ** parameters[], char * redirects[], char ** argArray[], int * parameterCount);

/*
 * Executes the entire pipeline
 */
void executePipeline(struct Parameter ** commands, int numCommands, char * cmd);

/*
 * Redirect stdout, with the given parameters
 */
int stdoutRedirect(int outputMode, char * outputFile);