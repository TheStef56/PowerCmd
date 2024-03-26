#ifndef __POWERCMD_H__
#define __POWERCMD_H__
#include <stdio.h>

#define remove_dquotes(string)      do {size_t len = strlen(string);\
                                        if (string[len - NULL_TERM] == '"') string[len - NULL_TERM] = '\0';\
                                        if (string[0] == '"') memmove(string, string + 1, len);} while (0)
#define move_cursor_left(count)    do {for (int x = 0; x < count; x++) {\
                                        if (BUFFER_CURSOR > 0) {\
                                        CONSOLE_SCREEN_BUFFER_INFO csbi;\
                                        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);\
                                        GetConsoleScreenBufferInfo(console, &csbi);\
                                        if (csbi.dwCursorPosition.X == 0){\
                                            printf("\033[1A");\
                                            BUFFER_CURSOR--;\
                                            printf("\033[%dC", csbi.dwSize.X);\
                                            continue;\
                                        }\
                                        printf("\033[1D");\
                                        BUFFER_CURSOR--;}}} while (0)

#define move_cursor_right(count) do {size_t cnt = count;\
                                        for (int x = 0; x < cnt; x++) {\
                                        if (BUFFER_CURSOR < strlen(cmd_buffer)) {\
                                        CONSOLE_SCREEN_BUFFER_INFO csbi;\
                                        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);\
                                        GetConsoleScreenBufferInfo(console, &csbi);\
                                        if (csbi.dwCursorPosition.X == csbi.dwSize.X - 1){\
                                            printf("\033[1B");\
                                            printf("\033[%dD", csbi.dwSize.X);\
                                            BUFFER_CURSOR++;\
                                            continue;\
                                        }\
                                        printf("\033[1C");\
                                        BUFFER_CURSOR++;}}} while (0)
#define move_cursor_to_end(void)    move_cursor_right(strlen(cmd_buffer) - BUFFER_CURSOR)
#define move_cursor_to_start(void)  do {while (BUFFER_CURSOR > 0) {move_cursor_left(1);}} while (0)
#define reset_terminal_cursor(void) do {if (strlen(cmd_buffer) == 0) break;\
                                        CONSOLE_SCREEN_BUFFER_INFO csbi;\
                                        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);\
                                        move_cursor_to_end();\
                                        printf("\033[1C");\
                                        size_t bfsz = strlen(cmd_buffer);\
                                        int last_x = -1;\
                                        for (int x = 0; x <= bfsz; x++) {\
                                        GetConsoleScreenBufferInfo(console, &csbi);\
                                        if (last_x == 0) {\
                                            printf(" \033[1D");\
                                            x++;}\
                                        if (csbi.dwMaximumWindowSize.X - csbi.dwCursorPosition.X == 1) {\
                                        char charAtCursor;\
                                        DWORD bytesRead;\
                                        COORD cursorPos = csbi.dwCursorPosition;\
                                        ReadConsoleOutputCharacter(console, &charAtCursor, 1, cursorPos, &bytesRead);\
                                        COORD nextPos = {cursorPos.X + 1, cursorPos.Y};\
                                        char charAtNext;\
                                        ReadConsoleOutputCharacter(console, &charAtNext, 1, nextPos, &bytesRead);\
                                        if (charAtCursor != ' ' && charAtNext == '\0') {\
                                            printf(" ");\
                                            x++;}}\
                                        last_x = csbi.dwCursorPosition.X;\
                                        move_cursor_left(1);\
                                        BUFFER_CURSOR++;\
                                        printf(" ");\
                                        move_cursor_left(1);\
                                        BUFFER_CURSOR++;}\
                                        BUFFER_CURSOR = 0;} while (0)

#define buff_append(buffer, c)       do {size_t len = strlen(buffer);\
                                        buffer[len]             = c;\
                                        buffer[len + NULL_TERM] = '\0';} while (0)
#define buff_insert(c)              do {memmove(cmd_buffer + BUFFER_CURSOR + NULL_TERM, cmd_buffer + BUFFER_CURSOR, MAX_BUFFER_SIZE - BUFFER_CURSOR - 2);\
                                        cmd_buffer[BUFFER_CURSOR] = c;} while (0)
#define buff_clear(void)            do {empty_buffer(cmd_buffer);\
                                        BUFFER_CURSOR = 0;} while (0)
#define buff_remove(void)           memmove(cmd_buffer + BUFFER_CURSOR - 1, cmd_buffer + BUFFER_CURSOR, MAX_BUFFER_SIZE - BUFFER_CURSOR)
#define empty_buffer(buffer)        memset(buffer, 0, MAX_BUFFER_SIZE + NULL_TERM)
#define buff_pop(void)              if (strlen(cmd_buffer) > 0) cmd_buffer[strlen(cmd_buffer) - NULL_TERM] = '\0'

typedef enum {                                                                        //                    ____________________
    EXIT, HISTORY, CLEAR_HISTORY, COLOR, TEXT_COLOR, BACKGROUND_COLOR, HELP,          // <------------------| custom commands  |
                                                                                      //                    |------------------|
    ASSOC, BREAK, CALL, CD, CHCP, CHDIR, COPY, DATE_, DEL, DIR_, ECHO, ENDLOCAL,      // <------------------|------------------|
    ERASE, FOR, FORMAT, FTYPE, GOTO, GRAFTABL, HELP_, IF, MD, MKDIR, MKLINK,          //                    |     windows      |
    MODE, MORE, MOVE, PATH, PAUSE, POPD, PROMPT, PUSHD, RD, REM, REN, RENAME,         //                    |     commands     |
    RMDIR, SET, SETLOCAL, SHIFT, START, TIME, TITLE, TREE, TYPE, VER, VERIFY, VOL,    // <------------------|__________________|
    NUMBER_OF_COMMANDS
} CustomCommands;

typedef enum {
    BLACK,        DARK_BLUE,      DARK_GREEN,    DARK_CYAN,
    DARK_RED,     DARK_MAGENTA,   DARK_YELLOW,   DARK_WHITE,
    BRIGHT_BLACK, BRIGHT_BLUE,    BRIGHT_GREEN,  BRIGHT_CYAN,
    BRIGHT_RED,   BRIGHT_MAGENTA, BRIGHT_YELLOW, WHITE,
    NUMBER_OF_COLORS
} CustomColors;

typedef struct {
    size_t value;
    char * name;
} CmdColor;

typedef struct Node Node;

struct Node{
    char * value;
    Node * next;
} ;

typedef struct {
    char kind;
    char *ptr;
} Token;

typedef struct {
    Node * first;
    Node * last;
    size_t count;
    size_t index;
} AutoCompl_CircularSearch;

char *CUSTOM_COMMANDS[] = {
    [EXIT]       = "exit",       [HISTORY] = "history", [CLEAR_HISTORY]    = "clear-history",    [COLOR] = "color",
    [TEXT_COLOR] = "text-color", [HELP]    = "help?",   [BACKGROUND_COLOR] = "background-color", [ASSOC] = "assoc",
    [BREAK]      = "break",      [CALL]    = "call",    [CD]               = "cd",               [CHCP]  = "chcp",
    [CHDIR]      = "chdir",      [COPY]    = "copy",    [DATE_]            = "date",             [DEL]   = "del",
    [DIR_]       = "dir",        [ECHO]    = "echo",    [ENDLOCAL]         = "endlocal",         [ERASE] = "erase",
    [FOR]        = "for",        [FORMAT]  = "format",  [FTYPE]            = "ftype",            [GOTO]  = "goto",
    [GRAFTABL]   = "graftabl",   [HELP_]   = "help",    [IF]               = "if",               [MD]    = "md",
    [MKDIR]      = "mkdir",      [MKLINK]  = "mklink",  [MODE]             = "mode",             [MORE]  = "more",
    [MOVE]       = "move",       [PATH]    = "path",    [PAUSE]            = "pause",            [POPD]  = "popd",
    [PROMPT]     = "prompt",     [PUSHD]   = "pushd",   [RD]               = "rd",               [REM]   = "rem",
    [REN]        = "ren",        [RENAME]  = "rename",  [RMDIR]            = "rmdir",            [SET]   = "set",
    [SETLOCAL]   = "setlocal",   [SHIFT]   = "shift",   [START]            = "start",            [TIME]  = "time",
    [TITLE]      = "title",      [TREE]    = "tree",    [TYPE]             = "type",             [VER]   = "ver",
    [VERIFY]     = "verify",     [VOL]     = "vol"

};

CmdColor CUSTOM_COLORS[] = {
    [BLACK]         = {.value = 30, .name = "Black"},        [DARK_BLUE]      = {.value = 34, .name = "Blue"},
    [DARK_GREEN]    = {.value = 32, .name = "Green" },       [DARK_CYAN]      = {.value = 36, .name = "Cyan"},
    [DARK_RED]      = {.value = 31, .name = "Red" },         [DARK_MAGENTA]   = {.value = 35, .name = "Magenta"},
    [DARK_YELLOW]   = {.value = 33, .name = "Yellow"},       [DARK_WHITE]     = {.value = 37, .name = "Light gray"},
    [BRIGHT_BLACK]  = {.value = 90, .name = "Dark gray" },   [BRIGHT_BLUE]    = {.value = 94, .name = "Light blue"},
    [BRIGHT_GREEN]  = {.value = 92, .name = "Light green"},  [BRIGHT_CYAN]    = {.value = 96, .name = "Light cyan"},
    [BRIGHT_RED]    = {.value = 91, .name = "Light red" },   [BRIGHT_MAGENTA] = {.value = 95, .name = "Light Magenta"},
    [BRIGHT_YELLOW] = {.value = 93, .name = "Light yellow"}, [WHITE]          = {.value = 97, .name = "White"}
};

#endif // __POWERCMD_H__