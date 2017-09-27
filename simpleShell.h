// Julia Connelly 9/26/2017

typedef enum {FD_TYPE, STRING_TYPE, NULL_TYPE} ValueType;

struct IO {
    ValueType type;
    union {
        int fd;
        char* name;
    };
};

typedef struct IO IO;

void interpret();
void execute(char** words, int instructionStart, IO* output, IO* input, int fileToClose, bool runInBackground, bool interpretNow);
