// Julia Connelly 9/26/2017

#include    <stdlib.h>
#include    <stdio.h>
#include    <unistd.h>
#include    <string.h>
#include    <fcntl.h>
#include    <sys/types.h>
#include    <sys/wait.h>
#include    <stdbool.h>
#include    <errno.h>
#include    <ctype.h>

#include    "simpleShell.h"

/*
 * reads a single line from terminal and parses it into an array of tokens/words by
 * splitting the line on spaces.  Adds NULL as final token
 */
char** readLineOfWords() {

    // A line may be at most 100 characters long, which means longest word is 100 chars,
    // and max possible tokens is 51 as must be space between each
    size_t MAX_WORD_LENGTH = 100;
    size_t MAX_NUM_WORDS = 51;

    // allocate memory for array of array of characters (list of words)
    char** words = (char**) malloc( MAX_NUM_WORDS * sizeof(char*) );
    int i;
    for (i = 0; i < MAX_NUM_WORDS; i++) {
        words[i] = (char*) malloc( MAX_WORD_LENGTH );
    }

    // read actual line of input from terminal
    int bytes_read;
    char *buf;
    buf = (char*) malloc( MAX_WORD_LENGTH + 1 );
    bytes_read = getline(&buf, &MAX_WORD_LENGTH, stdin);
    if(bytes_read == -1) {
        fprintf(stderr, "Program terminated\n");
        exit(0);
    }

    // take each word from line and add it to next spot in list of words
    i = 0;
    char* word = (char*) malloc( MAX_WORD_LENGTH );
    word = strtok(buf, " \n");
    while (word != NULL && i < MAX_NUM_WORDS) {
        strcpy(words[i++], word);
        word = strtok(NULL, " \n");
    }

    // check if we quit because of going over allowed word limit
    if (i == MAX_NUM_WORDS) {
        printf( "WARNING: line contains more than %d words!\n", (int)MAX_NUM_WORDS );
    }
    else {
        words[i] = NULL;
    }

    // return the list of words
    return words;
}

bool isSymbol(char c) {
    if(c == '-' || c == '.' || c == '/' || c == '_') {
        return true;
    }
    return false;
}

/*
 * Interprets a single line of input, then waits for another line.
 * Quits when q is entered.
 */
void interpret() {
    printf(">>> ");
    char** words = readLineOfWords();
    char* curWord = words[0];
    int index = 0;

    IO* output = (IO*)malloc(sizeof(IO));
    output->type = NULL_TYPE;
    output->name = NULL;
    IO* input = (IO*)malloc(sizeof(IO));
    input->type = NULL_TYPE;
    input->name = NULL;

    bool runInBackground = false;
    int instructionEnd = -1;
    int instructionStart = 0;
    int writePipeSide = -1;

    while(curWord != NULL) {
        // quits
        if(!strcmp(curWord, "q")) {
            exit(0);
        }
        // redirect output
        if(!strcmp(curWord, ">")) {
            // cut off words at operator
            words[index] = NULL;

            // put next word into output
            index++;
            curWord = words[index];
            output->type = STRING_TYPE;
            output->name = curWord;
        }
        // redirect input
        else if (!strcmp(curWord, "<")) {
            // cut off words at operator
            words[index] = NULL;

            // put next word into input
            index++;
            curWord = words[index];
            input->type = STRING_TYPE;
            input->name = curWord;
        }
        // pipe output to input
        else if (!strcmp(curWord, "|")) {
            int fds[2];
            pipe(fds);

            // set output to be the write end of the new pipe
            output->type = FD_TYPE;
            output->fd = fds[1];
            writePipeSide = fds[1];

            // cut off words at operator
            words[index] = NULL;

            // run this side of the pipe
            execute(words, instructionStart, output, input, fds[0], false, false);

            // close the write end in the parent process
            close(fds[1]);

            // set input to read end of pipe
            input->type = FD_TYPE;
            input->fd = fds[0];

            // reset output
            output->type = NULL_TYPE;
            output->name = NULL;

            // set the start index to the other end of the pipe
            instructionStart = index + 1;
        }
        // run in the background
        else if (!strcmp(curWord, "&")) {
            // cut off words at operator
            words[index] = NULL;

            if(words[index + 1] != NULL) {
                fprintf(stderr, "Syntax error: no characters allowed after &\n");
                interpret();
                return;
            }

            runInBackground = true;
        }
        // make sure word contains allowed characters
        else {
            int numChars = strlen(curWord);
            int i;
            for(i = 0; i < numChars; i++) {
                if(!isalpha(curWord[i]) && !isdigit(curWord[i]) && !isSymbol(curWord[i])) {
                    fprintf(stderr, "%c is not an allowed character\n", curWord[i]);
                    interpret();
                    return;
                }
            }
        }

        // move on to next word
        index++;
        curWord = words[index];
    }

    // run the command
    execute(words, instructionStart, output, input, writePipeSide, runInBackground, true);
}

/*
 * Redirect STDOUT to the provided file
 */
void redirectOutput(IO* file) {
    int newfd;
    if(file->type == STRING_TYPE) {
        newfd = open(file->name, O_CREAT|O_WRONLY, 0644);
    } else {
        newfd = file->fd;
    }
    dup2(newfd, STDOUT_FILENO);
}

/*
 * Redirect STDIN to the provided file
 */
void redirectInput(IO* file) {
    int newfd;
    if(file->type == STRING_TYPE) {
        newfd = open(file->name, O_CREAT|O_RDONLY, 0644);
    } else {
        newfd = file->fd;
    }
    dup2(newfd, STDIN_FILENO);
}

/*
 * Executes the provided instructions and continues the interpreting loop when required
 */
void execute(char** words, int instructionStart, IO* output, IO* input, int fileToClose, bool runInBackground, bool interpretNow) {
    int pid = fork();
    if(pid == 0) {
        if(output->type != NULL_TYPE) {
            redirectOutput(output);
        }
        if(input->type != NULL_TYPE) {
            redirectInput(input);
        }
        if(fileToClose >= 0) {
            close(fileToClose);
        }

        int err = execvp(words[instructionStart], words + instructionStart);
        int errorCode = errno;

        if(err) {
            if(errorCode == ENOENT) {
                fprintf(stderr, "Command not found: %s\n", words[instructionStart]);
            } else {
                fprintf(stderr, "An error occured\n");
            }
            exit(0);
        }
    } else {
        if(interpretNow) {
            if(!runInBackground) {
                waitpid(pid, NULL, 0);
            }
            interpret();
        }
    }
}

int main() {
    printf("Type 'q' to exit\n");
    interpret();
    return 0;
}
