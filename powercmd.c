/* Copyright (c) 2024 Stefano Sanasi <stefanosanasi2017@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions: 

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <unistd.h>
#include <windows.h>
#include <malloc.h>
#include <dirent.h>
#include "powercmd.h"

#define MAX_BUFFER_SIZE 8191                                            // 8KB as Windows terminal standard
#define NULL_TERM 1
#define MAX_HISTORY_SIZE 1024*1024                                      // 1MB

char *HISTORY_FILE;
char *SESSION_FILE;
char cmd_buffer[MAX_BUFFER_SIZE + NULL_TERM]        = {0};
char cmd_temp_buffer[MAX_BUFFER_SIZE + NULL_TERM]   = {0};
AutoCompl_CircularSearch ACCS                       = {0};              // circular linked list for commands
CustomColors CURRENT_CORE_COLOR                     =  DARK_GREEN;
CustomColors CURRENT_TYPING_COLOR                   =  DARK_BLUE;
CustomColors CURRENT_BACK_COLOR                     =  BRIGHT_CYAN;
size_t BUFFER_CURSOR                                = 0;
size_t INDEX                                        = 0;
size_t FILE_CURSOR_OFFSET;
size_t CORE_COLOR;
size_t TYPING_COLOR;

char *get_last_char(char *buffer, char ch) {                            // get the pointer to the last character matched in the string, else get NULL
    size_t len = strlen(buffer);
    for (int x = len; x >= 0; x--) {
        if (buffer[x] == ch) {
            return buffer + x;
        }
    }
    return NULL;
}

size_t get_char_count(char *buffer, char ch) {                          // get how many characters are there in a string
    size_t count = 0;
    size_t len = strlen(buffer);
    for (size_t x = 0; x < len; x++) {
        if (buffer[x] == ch) count++;
    }
    return count;
}

int is_custom_command(char *command) {                                  // get 0 if the string corresponds to a custom command
    for (size_t x = 0; x < NUMBER_OF_COMMANDS; x++){
        if (strcmp(CUSTOM_COMMANDS[x], command) == 0) return 0;
    } return 1;
}

Token get_last_valid_token(char *buffer) {                              // get last "valid" (not enclosed in double quotes) token
    Token res;
    size_t len = strlen(buffer);
    size_t number_of_dquotes = get_char_count(buffer, '"');
    size_t number_of_spaces = get_char_count(buffer, ' ');
    if (number_of_dquotes > 0) {                                                             // if "" are > 0
        char * last_dquote;                  
        if (number_of_dquotes%2 == 0) {                                                      // if number of dquotes is even
            if (buffer[len - NULL_TERM] == '"') {                                            // if last char is a dquote, remove it, find the previous, then put it back;
                buffer[len - NULL_TERM] = '\0';
                last_dquote = get_last_char(buffer, '"');
                buffer[len - NULL_TERM] = '"';
                res.kind = '"';
                res.ptr = last_dquote;
                return res;
            } else if (get_last_char(buffer, ' ') != NULL && get_last_char(buffer, ' ') - get_last_char(buffer, '"') < 0){         // if the dquotes are still even, but the last char isn't a dquote
                res.kind = '\0';                                                                                                   // and the last space is before the las quote: return NULL (no action)
                res.ptr = NULL;
                return res;
            }
        } else {                                                                            // the number of dquotes is odd, return the char next to the last one
            last_dquote = get_last_char(buffer, '"');
            res.kind = '"';
            res.ptr = last_dquote;
            return res;
        }
    } else if (number_of_spaces > 0){
        char * last_space = get_last_char(buffer, ' ');
        res.kind = ' ';
        res.ptr = last_space;    
        return res;                                                            // return the the last space
    } else if (len > 0) {
        res.kind = 'b',
        res.ptr = buffer;
        return res;
    }

}

void ACCS_append(char *value) {                                         // append entrie to the circular linked list
    if (ACCS.count == 0) {
        ACCS.first        = malloc(sizeof(Node));
        ACCS.first->value = malloc(sizeof(char)*(strlen(value)+1));
        strncpy(ACCS.first->value, value, strlen(value) + NULL_TERM);       
        ACCS.first->next = ACCS.last;
        ACCS.last        = ACCS.first;
        ACCS.last->next  = ACCS.first;
        ACCS.count++;
    } else {
        Node *n              = malloc(sizeof(Node));
        n->value             = malloc(sizeof(char)*(strlen(value)+1));
        n->next              = ACCS.first;
        ACCS.last->next      = n;
        ACCS.last            = n;
        strncpy(n->value, value, strlen(value) + NULL_TERM); 
        ACCS.count++;
    }
}

void jump_to_space_left(void) {                                         // moves the cursor to the nearest space outside of dquotes to the right
    size_t dquotes_encountered = 0;
    size_t len                 = strlen(cmd_buffer);
    size_t n_of_dquotes        = 0;
    for (int x = 0; x < len; x++) if (cmd_buffer[x] == '"') n_of_dquotes++;
    char ch;
    for (int x = len; x >= 0; x--) {
        ch = cmd_buffer[x];
        if (ch == '"') {
            dquotes_encountered++;
            continue;
        }
        if (ch == ' ') {
            if (strchr(cmd_buffer, '"') != NULL && dquotes_encountered%2 == 0 && n_of_dquotes%2 != 0) continue;
            if (strchr(cmd_buffer, '"') != NULL && dquotes_encountered%2 == 1 && n_of_dquotes%2 == 0) continue;
            if (x >= BUFFER_CURSOR) {
                continue;
            } else {
                size_t limit = BUFFER_CURSOR - x;
                move_cursor_left(limit);
            }
            break;
        } else {
            char * first_space = strchr(cmd_buffer, ' ');
            if (first_space == NULL || first_space - cmd_buffer >= x) {
                move_cursor_left(BUFFER_CURSOR);
            }
        }   
    }
}

void jump_to_space_right(void) {                                        // moves the cursor to the nearest space outside of dquotes to the right
    size_t dquotes_encountered = 0;
    size_t len                 = strlen(cmd_buffer);
    char ch;
    if (BUFFER_CURSOR == len) return;
    for (size_t x = 0; x < len; x++) {
        ch = cmd_buffer[x];
        if (ch == '"') {
            dquotes_encountered++;
            continue;
        }
        if (ch == ' ') {
            if (strchr(cmd_buffer, '"') != NULL && dquotes_encountered%2 == 1) continue;
            if (x <= BUFFER_CURSOR) {
                continue;
            } else {
                size_t limit = x - BUFFER_CURSOR;
                move_cursor_right(limit);
                break;
            }
        } else {
            char * next_space = strchr(cmd_buffer + x, ' ');
            if (next_space == NULL) {
                move_cursor_to_end();
                return;
            }
        }   
    }
}

void back_space(size_t count) {                                         // cancels a character from terminal and cmd_buffer based on cursos position
    for (size_t x = 0; x < count; x++) {
        if (BUFFER_CURSOR > 0) {
            size_t bfsz = strlen(cmd_buffer);
            if (BUFFER_CURSOR == bfsz) {
                buff_pop();
                printf("\033[1D \033[1D");
            } else {
                size_t last_bfc = BUFFER_CURSOR;
                size_t offset   = bfsz -  BUFFER_CURSOR;
                buff_remove();
                move_cursor_to_end();
                reset_terminal_cursor();
                BUFFER_CURSOR = last_bfc;
                printf("%s", cmd_buffer);
                printf(" \033[1D");
                for (int x = 0; x < offset; x++) {
                    printf("\033[1D");
                };
            }
            BUFFER_CURSOR--;
        }
    }
    INDEX = 0;
}

void cancel_until_token(void) {                                         // cancel until a space or a " ora \ is encountered
    char tmp[MAX_BUFFER_SIZE + NULL_TERM];
    char *last_back_slash = get_last_char(cmd_buffer, '\\');
    memset(tmp, 0, MAX_BUFFER_SIZE + NULL_TERM);
    memcpy(tmp, cmd_buffer, BUFFER_CURSOR);
    Token t = get_last_valid_token(tmp);
    size_t offset = t.ptr - tmp;
    if (last_back_slash != NULL && last_back_slash -  cmd_buffer > offset && last_back_slash - cmd_buffer < BUFFER_CURSOR) {
        offset = last_back_slash - cmd_buffer;
        back_space((cmd_buffer + BUFFER_CURSOR) - (cmd_buffer + offset));
        return;
    }
    if (t.ptr != NULL && t.ptr > tmp) {
        back_space((cmd_buffer + BUFFER_CURSOR) - (cmd_buffer + offset));
    } else {
        back_space(BUFFER_CURSOR);
    }
}

void swap_buffers(void) {                                               // puts the cmd_temp_buffer content into cmd_buffer and memset the frist one to 0
        if (strlen(cmd_temp_buffer) > 0) {                              // if there are commands in the cmd_temp_buffer buffer, put it in the main buffer and memset to 0 the first one
            memcpy(cmd_buffer, cmd_temp_buffer, MAX_BUFFER_SIZE + NULL_TERM);
            empty_buffer(cmd_temp_buffer);
            BUFFER_CURSOR = strlen(cmd_buffer);
        }
}

void free_nodes(Node *n, int idx) {                                     // free all the nodes recursively
    if (idx == 0) {
        ACCS.count = 0;
        ACCS.index = 0;
        return;
    }
    Node *next = n->next;
    free(n);
    free_nodes(next, idx - 1);
}

void print_nodes(Node *n, int idx) {                                    // prints all the nodes recursively
    if (ACCS.count == 0) return;
    if (idx == 0) {
        size_t limit;
        if (strlen(cmd_temp_buffer) > 0) {
            limit = strlen(cmd_temp_buffer);
        } else {
            limit = strlen(cmd_buffer);
        }
        for (size_t x = 0; x < limit; x++) {
            printf("\033[1D \033[1D");
        }
        empty_buffer(cmd_temp_buffer);
        ACCS.index    = (ACCS.index + NULL_TERM)%(ACCS.count + NULL_TERM);
        strncpy(cmd_temp_buffer, cmd_buffer, strlen(cmd_buffer) + NULL_TERM);

        // ---------------------------------------------------------------------------------------------------------------

        // removing and putting dquotes for to change C:\path\"completion with spaces" into "C:\path\competion with spaces"

        size_t bfsz = strlen(cmd_buffer);
        size_t n_len = strlen(n->value);
        char temp_n_value_buff[n_len + NULL_TERM];                                                                // putting entries in a temp variable to avoid next cicle contamination
        char *temp_n_value = temp_n_value_buff;
        strncpy(temp_n_value, n->value, n_len + NULL_TERM);
        Token t = get_last_valid_token(cmd_buffer);                                                               // getting kind of last token to avoid putting extra duote at the beginning
        if (strchr(cmd_buffer, ' ') == NULL) {
            strncpy(cmd_temp_buffer + strlen(cmd_temp_buffer), temp_n_value, strlen(temp_n_value) + NULL_TERM);
            printf("%s",cmd_temp_buffer);
            return;
        }
        if (temp_n_value[0] == '"' && cmd_buffer[bfsz - NULL_TERM] != ' ' && cmd_buffer[bfsz - NULL_TERM] != '"') {   // if the entry is in dquotes and the last char of cmd_buffer isn't space or dquote
            remove_dquotes(temp_n_value);                                                                             // if there are dquotes on the entry, remove those
            temp_n_value[n_len + 1 - NULL_TERM] = '"';
            if (t.kind != '"') {                                                                                  // if token is not of kind " then last space is before the path comletion
                char * last_space = get_last_char(cmd_temp_buffer, ' ');                                          // and we need to put a dquote after there
                memmove(last_space + 1, last_space, strlen(last_space));
                last_space[1] = '"';
            }
        } else if (t.kind == '"') {                                                                               // else, the dquote is already before last command, we only need to put one at the end
            remove_dquotes(temp_n_value);
            char tmp[n_len + 1 + NULL_TERM];
            memset(tmp, 0 , n_len + 1 + NULL_TERM);
            strncpy(tmp, temp_n_value, n_len + NULL_TERM);
            tmp[strlen(tmp)] = '"';
            tmp[strlen(tmp) + 1] = '\0';
            temp_n_value = tmp;
        } 

        strncpy(cmd_temp_buffer + strlen(cmd_temp_buffer), temp_n_value, strlen(temp_n_value) + NULL_TERM);
        if (get_char_count(cmd_temp_buffer, '"') > 0 && get_char_count(cmd_temp_buffer, '"')%2 != 0) {
            cmd_temp_buffer[strlen(cmd_temp_buffer)] = '"';
            cmd_temp_buffer[strlen(cmd_temp_buffer) + 1] = '\0';
        }

        // ---------------------------------------------------------------------------------------------------------------

        printf("%s",cmd_temp_buffer);
        return;
    }
    print_nodes(n->next, idx - 1);
}

int count_lines(void) {                                                 // counts the lines of the history file
    FILE *fl = fopen(HISTORY_FILE, "r");
    fseek(fl, 0, SEEK_END);
    int size = ftell(fl);
    fseek(fl, 0, SEEK_SET);
    int  count   = 0;
    char *buffer = malloc(size);
    while (fgets(buffer, sizeof(buffer), fl) != NULL) {
        count++;
    }
    fclose(fl);
    free(buffer);
    return count;
}

size_t auto_complete_dir_dump(char *match, char *path) {                // looks for files/dierctories in the given path
    if (ACCS.count > 0) return ACCS.count;
    size_t found = 0;
    DIR * dir;
    struct dirent *entry;
    dir = opendir(path);
    if (dir == NULL) return 0;
    entry = readdir(dir);
    while (entry != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            size_t entry_len = strlen(entry->d_name);
            if (match != NULL && strcmp(match, "")) {
                remove_dquotes(match);
                size_t match_len = strlen(match);
                if (strncmp(entry->d_name, match, match_len)) {
                    entry = readdir(dir);
                    continue;
                }if (strchr(entry->d_name, ' ') != NULL) {
                    char edit[entry_len + 3];
                    edit[0] = '"';
                    strncpy(edit + NULL_TERM, entry->d_name, entry_len);
                    edit[entry_len + NULL_TERM] = '"'; 
                    edit[entry_len + 2] = '\0';
                    ACCS_append(edit);
                    found++;
                } else {
                    ACCS_append(entry->d_name);
                    found++;
                }
            } else {
                if (strchr(entry->d_name, ' ') != NULL) {
                    char edit[entry_len + 3];
                    edit[0] = '"';
                    strncpy(edit + NULL_TERM, entry->d_name, entry_len);
                    edit[entry_len + NULL_TERM] = '"'; 
                    edit[entry_len + 2] = '\0';
                    ACCS_append(edit);
                    found++;
                } else {
                    ACCS_append(entry->d_name);
                    found++;
                }
            }
        }
        entry = readdir(dir);
    }
    closedir(dir);
    return found;
}

size_t auto_complete_dump(char *match) {                                // looks for commands or directory in the current path to auto-complete the command
    if (ACCS.count > 0) return ACCS.count;
    size_t found = 0;
    char* envVar = getenv("PATH");
    
    char* envVarCopy = _strdup(envVar);
    char* token      = strtok(envVarCopy, ";");

    remove_dquotes(match);

    WIN32_FIND_DATA FindData;
    HANDLE hFind;
    char searchPath[MAX_PATH];

    for (size_t x = 0; x < NUMBER_OF_COMMANDS; x++) {
        if (strncmp(match, CUSTOM_COMMANDS[x], strlen(match)) == 0) {
            ACCS_append(CUSTOM_COMMANDS[x]);
            found++;
        }
    }

    snprintf(searchPath, MAX_PATH, "%s\\%s*", ".\\", match);
    hFind = FindFirstFile(searchPath, &FindData);
    if (hFind != INVALID_HANDLE_VALUE) {
        size_t len = strlen(FindData.cFileName);
        if (strchr(FindData.cFileName, ' ') != NULL) {
            char edit[len + 3];
            strncpy(edit, FindData.cFileName, len);
            edit[len] = '"';
            edit[len + 1]         = '\0';
            ACCS_append(edit);
            found++;
        } else {
            ACCS_append(FindData.cFileName);
            found++;
        }
        while (FindNextFile(hFind, &FindData) != 0) {
            size_t len = strlen(FindData.cFileName);
            if (strchr(FindData.cFileName, ' ') != NULL) {
                char edit[len + 3];
                strncpy(edit, FindData.cFileName, len);
                edit[len] = '"';
                edit[len + 1]         = '\0';
                ACCS_append(edit);
                found++;
            } else {
                ACCS_append(FindData.cFileName);
                found++;
            }
        }
        FindClose(hFind);
    }
    while (token != NULL) {
        WIN32_FIND_DATA FindData;
        HANDLE hFind;
        size_t len = strlen(match);
        snprintf(searchPath, MAX_PATH, "%s\\%s*", token, match);
        hFind = FindFirstFile(searchPath, &FindData);
        if (hFind != INVALID_HANDLE_VALUE) {
            size_t fl_len = strlen(FindData.cFileName);
            if (fl_len > 5) {
                if (strcmp(FindData.cFileName + fl_len - 4, ".exe") == 0 || strcmp(FindData.cFileName + fl_len - 4, ".lnk") == 0) {         // finding only executables and links
                    ACCS_append(FindData.cFileName);
                    found++;
                }
            }
            while (FindNextFile(hFind, &FindData) != 0) {
                fl_len = strlen(FindData.cFileName);
                if (fl_len > 5) {
                    if (strcmp(FindData.cFileName + fl_len - 4, ".exe") == 0 || strcmp(FindData.cFileName + fl_len - 4, ".lnk") == 0) {     // finding only executables and links 
                        ACCS_append(FindData.cFileName);
                        found++;
                    }
                }
            }
            FindClose(hFind);
        }
        token = strtok(NULL, ";");
    }
    free(envVarCopy);
    return found;
}

void check_for_resize_history_file(size_t offset) {                     // checks if the history file is bigger than MAX_HISTORY_SIZE, if so, it shrinks it deletese some content at the beginning
    FILE * fl = fopen(HISTORY_FILE, "rb");
    fseek(fl, 0, SEEK_END);
    size_t size = ftell(fl);
    fseek(fl, 0, SEEK_SET);
    if (size > MAX_HISTORY_SIZE) {
        char * buffer = malloc(size*sizeof(char));
        fread(buffer, 1, size, fl);
        buffer[size] = '\0';
        fclose(fl);
        memmove(buffer, buffer + offset, size - offset);
        memset(buffer + size - offset, 0, offset);
        FILE * fl = fopen(HISTORY_FILE, "wb");
        fprintf(fl, "%s", buffer);
        free(buffer);
    }
    fclose(fl);
}

void load_cmd(void) {                                                   // gets the last command from history based on the INDEX (which changes with arrow keys)
    if (access(HISTORY_FILE, F_OK)) return;
    check_for_resize_history_file(1024);
    int ch;
    char buffer[MAX_BUFFER_SIZE] = {0};
    int size;
    FILE * fl = fopen(HISTORY_FILE, "r");
    fseek(fl, 0, SEEK_END);
    size = ftell(fl);

    ch = fgetc(fl);

    for (int x = 0; x <= INDEX; x++) {
        while (ch != '\n') {
            if (FILE_CURSOR_OFFSET == size) break;
            FILE_CURSOR_OFFSET++;
            fseek(fl, size - FILE_CURSOR_OFFSET, SEEK_SET);
            ch = fgetc(fl);
        }
        if (FILE_CURSOR_OFFSET == size) break;
        ch = 0;
        FILE_CURSOR_OFFSET++;

    }

    if (FILE_CURSOR_OFFSET >= size) {
        FILE_CURSOR_OFFSET = size;
        INDEX--;
    } else {
        FILE_CURSOR_OFFSET -= 2;
    }

    fseek(fl, size - FILE_CURSOR_OFFSET, SEEK_SET);
    fgets(buffer, MAX_BUFFER_SIZE, fl);

    char * new_line_char = strchr(buffer, '\n');
    if (new_line_char) {
        *new_line_char = '\0';
    }

    memcpy(cmd_buffer, buffer, MAX_BUFFER_SIZE);
    printf("%s", buffer);
    BUFFER_CURSOR = strlen(buffer);
    fclose(fl);
    FILE_CURSOR_OFFSET = 0;
}

void save_buffer(char *buffer) {                                        // save last command to the .history if the last one was different
    FILE * fl = fopen(HISTORY_FILE, "a+");
    fseek(fl, 0, SEEK_END);
    int ch;
    size_t offset = 0;
    size_t size   = ftell(fl);
    if (size > 0) {
        char *tmp = malloc(size);
        while (ch != '\n') {
            if (offset == size) break;
            offset++;
            fseek(fl, size - offset, SEEK_SET);
            ch = fgetc(fl);
        }
        fread(tmp, offset, 1, fl);
        tmp[offset - 1] = '\0';
        if (strcmp(tmp, buffer) == 0){
             return;
        }  // checking if the last command is the same of the one being saved
    free(tmp);
    }
    fseek(fl, 0, SEEK_END);
    if (ftell(fl) > 0) fwrite("\n", strlen("\n"), 1, fl);
    fwrite(buffer, strlen(buffer), 1, fl);
    fclose(fl);
}

void handle_tab(void) {                                                 // Handles the tab-completion behavior
    Token  t    = get_last_valid_token(cmd_buffer);
    size_t bfsz = strlen(cmd_buffer);

    if (strchr(cmd_buffer, ' ') == NULL) {
        if (bfsz == 0) {
            auto_complete_dir_dump(NULL, ".");
            print_nodes(ACCS.first, ACCS.index);
        } else {
            if (auto_complete_dump(cmd_buffer)) back_space(bfsz);
            print_nodes(ACCS.first, ACCS.index);
        }
        return;
    }
    switch (t.kind){
        case ' ':
                if (get_last_char(cmd_buffer, '\\') - t.ptr > 0){  // if there is a slash after the last space
                char tmp[bfsz + NULL_TERM];
                strncpy(tmp, t.ptr + 1, bfsz + NULL_TERM);
                char * slash = get_last_char(tmp, '\\');
                *slash = '\0';
                if (slash[1] == '\0') {
                    auto_complete_dir_dump(NULL, t.ptr + 1);
                    print_nodes(ACCS.first, ACCS.index);
                } else {
                    char tmp2[strlen(slash + 1) + NULL_TERM];
                    strncpy(tmp2, slash + 1, strlen(slash + 1) + NULL_TERM);
                    slash[0] = '\\';
                    slash[1] = '\0';
                    if (auto_complete_dir_dump(tmp2, tmp)) back_space(strlen(tmp2));
                    print_nodes(ACCS.first, ACCS.index);
                }
            } else if (t.ptr[1] != '\0') {                   // if there are no slashes and there is a match
                if (auto_complete_dir_dump(t.ptr + 1, ".")) back_space(strlen(t.ptr + 1));
                print_nodes(ACCS.first, ACCS.index);
                   
            } else {                                         // if we are just going to complete after a space
                auto_complete_dir_dump(NULL, ".");
                print_nodes(ACCS.first, ACCS.index);
            }
            break;
        case '"': 
            if (t.ptr[1] == '\0') return;
            if (get_last_char(cmd_buffer, '\\') - t.ptr > 0) { // if there is a slash after last dquote
                if (cmd_buffer[bfsz - NULL_TERM] == '"') cmd_buffer[bfsz - NULL_TERM] = '\0';
                bfsz = strlen(cmd_buffer);
                char tmp[bfsz + NULL_TERM];
                strncpy(tmp, t.ptr + 1, bfsz + NULL_TERM);
                char * slash = get_last_char(tmp, '\\');
                *slash = '\0';
                if (slash[1] == '\0') {
                    auto_complete_dir_dump(NULL, tmp);
                    print_nodes(ACCS.first, ACCS.index);
                } else {
                    auto_complete_dir_dump(slash + 1, tmp);
                    print_nodes(ACCS.first, ACCS.index);
                }
            } else {                                           // if we are just completing an entry after a dquote
                if (auto_complete_dir_dump(t.ptr + 1, ".")) back_space(strlen(t.ptr + 1));
                print_nodes(ACCS.first, ACCS.index);
            }
            break;
        default: 
            break;
    }
}

int get_input(void) {                                                   // gets the keyboard input and acts accordingly based on key-code
    buff_clear();
    int ch;
    unsigned long long count = 0;
    while (1) {
        BOOL do_default      = FALSE;
        BOOL is_ctrl_pressed = GetAsyncKeyState(VK_LCONTROL) & 0x8000;
        BOOL is_V_pressed    = GetAsyncKeyState('V') & 0x8000;
        if (!(is_ctrl_pressed && is_V_pressed)) Sleep(1);
        if (kbhit()) {
            BOOL is_shift_pressed = GetAsyncKeyState(VK_SHIFT) & 0x8000;
            BOOL is_ctrl_pressed  = GetAsyncKeyState(VK_LCONTROL) & 0x8000;
            BOOL is_V_pressed     = GetAsyncKeyState('V') & 0x8000;
                 ch               = getch();
            switch (ch) {
                case 0: 
                    swap_buffers();
                    free_nodes(ACCS.first, ACCS.count);
                    break;
                case 224:   // cmd detects gets 224 when arrow keys are pressed
                    is_shift_pressed = FALSE;
                    swap_buffers();
                    free_nodes(ACCS.first, ACCS.count);
                    break;
                case 8: 
                    if (!is_V_pressed) {            // DELETE
                        swap_buffers();
                        free_nodes(ACCS.first, ACCS.count);
                        back_space(1);
                    }
                    break;
                case 9:   // TAB
                        handle_tab();
                    break;
                case 13:   // ENTER
                    free_nodes(ACCS.first, ACCS.count);
                    swap_buffers();
                    INDEX = 0;
                    return 0;
                case 72:   // ARROW UP
                    if (!is_shift_pressed && !is_V_pressed){
                        if (INDEX <= count_lines()) {
                            reset_terminal_cursor();
                            load_cmd();
                            INDEX++;
                        }
                        break;
                    }
                    do_default = TRUE;
                    break;
                case 75:   // ARROW-LEFT
                    if (!is_shift_pressed && !is_V_pressed){
                        move_cursor_left(1);
                        break;
                    }
                    do_default = TRUE;
                    break;
                case 77:   // ARROW-RIGHT
                    if (!is_shift_pressed && !is_V_pressed) {
                        move_cursor_right(1);
                        break;
                    }
                    do_default = TRUE;
                    break;
                case 80:   // ARROW-DOWN
                    if (!is_shift_pressed && !is_V_pressed){
                        if (INDEX > 0) {
                            INDEX--;
                            reset_terminal_cursor();
                            load_cmd();
                        } else {
                            reset_terminal_cursor();
                            buff_clear();
                        }
                        break;
                    }
                    do_default = TRUE;
                    break;
                case 115:   // CTRL + ARROW-LEFT  
                    swap_buffers();
                    if (is_ctrl_pressed && !is_V_pressed){ 
                        jump_to_space_left();
                        do_default = FALSE;
                        break;
                    }
                case 116:   // CTRL + ARROW-RIGHT
                    swap_buffers();
                    if (is_ctrl_pressed && !is_V_pressed){ 
                        jump_to_space_right();
                        do_default = FALSE;
                        break;
                    }
                case 127:   // CTRL + DEL
                    if (is_ctrl_pressed && !is_V_pressed) {
                        swap_buffers();
                        cancel_until_token();
                        break;
                    }
                default: 
                    swap_buffers();
                    do_default = TRUE;
                    break;
            }
            if (do_default) {
                INDEX = 0;
                free_nodes(ACCS.first, ACCS.count);
                if (strlen(cmd_buffer) >= MAX_BUFFER_SIZE - 1) continue;
                if (strlen(cmd_buffer) == BUFFER_CURSOR) {
                    buff_append(cmd_buffer, (char)ch);
                    printf("%c", ch); 
                    BUFFER_CURSOR ++;
                } else {
                    buff_insert((char)ch);
                    printf("%s", cmd_buffer + BUFFER_CURSOR);
                    for (int x = 0; x < strlen(cmd_buffer) - BUFFER_CURSOR - 1; x ++) {
                        printf("\033[1D");
                    }
                    BUFFER_CURSOR ++;
                }
            }
        }
    }
}

void sanitize_spaces(char *buffer) {                                    // removes multiple spaces outside of dquotes
    size_t len           = strlen(buffer);
    size_t edits         = 0;
    size_t last_is_space = 0;
    size_t dquotes       = 0;
    size_t index         = 0;
    if (len <= 0) return;
    for (size_t x = 0; x < len; x++) {
        if (buffer[index] == ' ') {
            if (dquotes > 0 && dquotes%2 != 0) {
                index++;
                continue;
            }
            if (index >= len - edits - NULL_TERM) {
                buffer[len - edits - NULL_TERM] = '\0';
            };
            if (last_is_space > 0) {
                memmove(buffer + index - NULL_TERM, buffer + index, strlen(buffer + index) + 1);
                edits++;
                continue;
            }
            last_is_space++;
        } else {
            if (buffer[index] == '"') {
                dquotes++;
            }
            last_is_space = 0;
        }
        index++;
    }
}

void sanitize_slashes(char *buffer) {                                   // removes front/slashes and puts back slashes when completing a path
    size_t len = strlen(buffer);
    for (size_t x = 0; x < len; x++) {
        if (buffer[x] == '/') buffer[x] = '\\';
    }
}

void print_color_list(void) {                                           // prints color list
    printf("\nColor list:\n"); 
    for (size_t x = 0; x < NUMBER_OF_COLORS; x++) {
        if (x <= 9) {
            printf("%d: %s\n", x, CUSTOM_COLORS[x].name);
        } else {
            printf("%c: %s\n", x + 55, CUSTOM_COLORS[x].name);
        }
    }
}

void init_colors(void) {                                                // inits color list
    CORE_COLOR = CUSTOM_COLORS[CURRENT_CORE_COLOR].value;
    TYPING_COLOR = CUSTOM_COLORS[CURRENT_TYPING_COLOR].value;
    char tmp[64];
    int c, b;
    if (CURRENT_TYPING_COLOR <= 9) c  = CURRENT_TYPING_COLOR + 48;
    if (CURRENT_TYPING_COLOR >= 10) c = CURRENT_TYPING_COLOR + 55;
    if (CURRENT_BACK_COLOR <= 9) b  = CURRENT_BACK_COLOR + 48;
    if (CURRENT_BACK_COLOR >= 10) b = CURRENT_BACK_COLOR + 55;
    sprintf(tmp, "color %c%c", b, c);
    system(tmp);
    printf("\n");
}

void save_session(void) {                                               // save colors and path
    FILE *fl = fopen(SESSION_FILE, "wb");
    fwrite(&CURRENT_CORE_COLOR, sizeof(&CURRENT_CORE_COLOR), 1, fl);
    fwrite(&CURRENT_TYPING_COLOR, sizeof(&CURRENT_TYPING_COLOR), 1, fl);
    fwrite(&CURRENT_BACK_COLOR, sizeof(&CURRENT_BACK_COLOR), 1, fl);
    char buffer[MAX_PATH];
    getcwd(buffer, MAX_PATH);
    size_t size = strlen(buffer) + NULL_TERM;
    fwrite(&size, sizeof(size_t), 1, fl);
    fprintf(fl, "%s", buffer);
    fclose(fl);
}

void load_sesion(void) {                                                // load colors and path
    if (access(SESSION_FILE, F_OK) == 0) {
        FILE *fl = fopen(SESSION_FILE, "rb");
        fread(&CURRENT_CORE_COLOR, sizeof(&CURRENT_CORE_COLOR), 1, fl);
        fread(&CURRENT_TYPING_COLOR, sizeof(&CURRENT_TYPING_COLOR), 1, fl);
        fread(&CURRENT_BACK_COLOR, sizeof(&CURRENT_BACK_COLOR), 1, fl);
        size_t size;
        fread(&size, sizeof(size_t), 1, fl);
        char buffer[size];
        fread(&buffer, sizeof(char), size, fl);
        chdir(buffer);
        fclose(fl);
    }
    init_colors();
}

int check_cd(void) {                                                    // checks if we are trying to change directory
    size_t bfsz = strlen(cmd_buffer);
    char tmp[MAX_BUFFER_SIZE + NULL_TERM];
    strncpy(tmp, cmd_buffer, MAX_BUFFER_SIZE + 1);                             
    sanitize_slashes(tmp);
    if (strncmp(tmp, "cd", 2) == 0) {
        if (strlen(tmp) == 2) {
            chdir(getenv("USERPROFILE"));
            printf("\n");
            save_buffer(tmp);
            return 1;
        }
        if (tmp[2] != ' ') {
            return 0;
        }
        char* directory = tmp + 3;
        save_buffer(tmp);
        remove_dquotes(directory);
        if (chdir(directory) != 0) {
            perror("\nError");
        }
        printf("\n");
        return 1;
    } else {
        return 0;
    }
    return 0;
}

void print_commands(void) {                                             // prints the list of commands
    printf("\nCustom commands list:\n\n\
color            [color index]  : sets the color of the path you are crruently in\n\
text-color       [color index]  : sets the color of the input and output\n\
background-color [color index]  : sets the background color of the terminal\n\
history                         : prints the history of the commands you have previously typed\n\
clear-history                   : clears the history\n\n");
}

int check_commands(void) {                                              // checks for custom commands and behaves accordingly
    if (!strcmp(cmd_buffer, CUSTOM_COMMANDS[EXIT])) {
        save_buffer(cmd_buffer);
        free(HISTORY_FILE);
        free_nodes(ACCS.first, ACCS.index);
        exit(0);
    } else if (!strcmp(cmd_buffer, CUSTOM_COMMANDS[HISTORY])) {
        save_buffer(cmd_buffer);
        if (access(HISTORY_FILE, F_OK)) {
            printf("\n");
            return 1;
        }
        FILE * fl = fopen(HISTORY_FILE, "r");
        fseek(fl, 0, SEEK_END);
        size_t size = ftell(fl);
        fseek(fl, 0, SEEK_SET);
        size_t count = 0;
        char last_c;
        if (size > 0){
            printf("\n");
            printf("0: ");
            while (!feof(fl)){
                last_c = fgetc(fl);
                printf("%c", last_c);
                if (last_c == '\n') {
                    count++;
                    printf("%u: ", count);
                } 
            }
            printf("\n");
        } else {
            printf("\n");
        }
        return 1;
    } else if (!strcmp(cmd_buffer, CUSTOM_COMMANDS[CLEAR_HISTORY])) {
        save_buffer(cmd_buffer);
        FILE * fl = fopen(HISTORY_FILE, "w");
        fclose(fl);
        printf("\n");
        return 1;
    } else  if (!strncmp(cmd_buffer, CUSTOM_COMMANDS[COLOR], strlen(CUSTOM_COMMANDS[COLOR]))) {
        save_buffer(cmd_buffer);
        size_t bfsz = strlen(cmd_buffer);
        if (bfsz <= strlen("color ")) {
            print_color_list();
            return 1;
        }
        int ch = cmd_buffer[bfsz - NULL_TERM];
        if ((ch < 48 || ch > 57) && (ch < 65 || ch > 70) || bfsz > strlen("color X")) {
            printf("\nERROR: invalid color\n\nUsage: color [color code]"); 
            print_color_list();
            return 1;
        } else {
            if (ch <= 57) ch -= 48;               // colors index from 0 to 9 
            if (ch >= 65) ch -= 55;               // colors index from 10 to 15
            CORE_COLOR = CUSTOM_COLORS[ch].value;
            CURRENT_CORE_COLOR = ch;
            save_session();
            printf("\n");
            return 1;
        }
    } else if (!strncmp(cmd_buffer, CUSTOM_COMMANDS[TEXT_COLOR], strlen(CUSTOM_COMMANDS[TEXT_COLOR]))) {
        save_buffer(cmd_buffer);
        size_t bfsz = strlen(cmd_buffer);
        if (bfsz <= strlen("text-color ")) {
            print_color_list();
            return 1;
        }
        int ch = cmd_buffer[bfsz - NULL_TERM];
        if ((ch < 48 || ch > 57) && (ch < 65 || ch > 70) || bfsz > strlen("text-color X")) {
            printf("\nERROR: invalid color\n\nUsage: text-color [color code]"); 
            print_color_list();
            return 1;
        } else {
            if (ch <= 57) ch -= 48;               // colors index from 0 to 9 
            if (ch >= 65) ch -= 55;               // colors index from 10 to 15
            TYPING_COLOR = CUSTOM_COLORS[ch].value;
            CURRENT_TYPING_COLOR = ch;
            save_session();
            printf("\n");
            return 1;
        }
    } else if (!strncmp(cmd_buffer, CUSTOM_COMMANDS[BACKGROUND_COLOR], strlen(CUSTOM_COMMANDS[BACKGROUND_COLOR]))) {
        save_buffer(cmd_buffer);
        size_t bfsz = strlen(cmd_buffer);
        if (bfsz <= strlen("backround-color ")) {
            print_color_list();
            return 1;
        }
        int ch = cmd_buffer[bfsz - NULL_TERM];
        if ((ch < 48 || ch > 57) && (ch < 65 || ch > 70) || bfsz > strlen("background-color X")) {
            printf("\nERROR: invalid color\n\nUsage: background-color [color code]"); 
            print_color_list();
            return 1;
        } else {
            char tmp[64];
            int c;
            if (CURRENT_CORE_COLOR <= 9) c = CURRENT_CORE_COLOR + 48;
            if (CURRENT_CORE_COLOR >= 10) c = CURRENT_CORE_COLOR + 55;
            sprintf(tmp, "color %c%c", ch, c);
            system(tmp);
            printf("\n");
            if (ch <= 57) ch -= 48;               // colors index from 0 to 9 
            if (ch >= 65) ch -= 55;               // colors index from 10 to 15
            CURRENT_BACK_COLOR = ch;
            save_session();
            return 1;
        }
    } else if (!strncmp(cmd_buffer, CUSTOM_COMMANDS[HELP], strlen(CUSTOM_COMMANDS[HELP]))) {
        save_buffer(cmd_buffer);
        print_commands();
        return 1;
    }
    return 0;
}

void setup_folders(void) {                                           // finds the absolute path of the directory of main program and adds /.history
    char *c = getenv("APPDATA");
    HISTORY_FILE = malloc(MAX_PATH*sizeof(char));
    SESSION_FILE = malloc(MAX_PATH*sizeof(char));
    sprintf(HISTORY_FILE, "%s", c);
    sprintf(HISTORY_FILE + strlen(c), "\\PowerCmd\\.history");
    memset(SESSION_FILE, 0, MAX_PATH);
    sprintf(SESSION_FILE, "%s", c);
    sprintf(SESSION_FILE + strlen(c), "\\PowerCmd\\.session");

    char folder_path[MAX_PATH];
    DWORD attrib = GetFileAttributes(folder_path);
    if (attrib == INVALID_FILE_ATTRIBUTES || !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
        CreateDirectory(folder_path, NULL);
    }
    return;

}

void print_version_info(void) {                                         // prints windows version, copyright and whatever I want on the start
    FILE *fp = popen("ver", "r");
    char bf[1024];
    while (fgets(bf, sizeof(bf), fp) != NULL) {
        if (strlen(bf) <= 1) memset(bf, 0, sizeof(bf));
    }
    printf("\033[%um%s", TYPING_COLOR, bf);
    pclose(fp);
    memset(bf, 0, sizeof(bf));
    fp = popen("powershell (Get-Command \"$env:SystemRoot\\System32\\cmd.exe\").FileversionInfo.LegalCopyright", "r");
    while (fgets(bf, sizeof(bf), fp) != NULL) {
        if (strlen(bf) <= 1) memset(bf, 0, sizeof(bf));
    }
    printf("%s", bf);
    printf("\n\
|\\   ____|\\____\n\
| \\ ////)))))\\\\\\@     powercmd v: 2.1.1\n\
| /)))))))))))))))>\n\
|/  \\\\\\)))))/////     type help?\n\
         |/  \n\
    \033[m");
   
    pclose(fp);
}

int check_for_dquotes_to_duplicate(void) {                              // checks for dquotes (") to duplicate before sending them to system(), otherwise single presence of them won't matter 
    size_t offset = 0;
    for (size_t x = 0; x < strlen(cmd_buffer) + NULL_TERM; x++) {
        if (x + offset > MAX_BUFFER_SIZE) return 1;
        cmd_temp_buffer[x + offset] = cmd_buffer[x];
        if (cmd_buffer[x] == '"'){
            cmd_temp_buffer[x + offset + NULL_TERM] = '"';
            offset++;
        }
    }
    memcpy(cmd_buffer, cmd_temp_buffer, MAX_BUFFER_SIZE + NULL_TERM);
    empty_buffer(cmd_temp_buffer);
    return 0;
}

int main(void) {
    system("");
    BOOL session_loaded = FALSE;
    print_version_info();
    setup_folders();
    chdir(getenv("USERPROFILE"));
    while (1) {
        if (!session_loaded) load_sesion();
        session_loaded = TRUE;
        getcwd(cmd_buffer, MAX_BUFFER_SIZE);
        printf("\033[%um%s:\033[%um ", CORE_COLOR, cmd_buffer, TYPING_COLOR);
        if (get_input() == 1) {
            continue;
        }
        sanitize_spaces(cmd_buffer);
        if (check_cd()) {
            save_session();
            continue;
        } else if (check_commands()){
            continue;
        } else {
            printf("\n");
            if (strlen(cmd_buffer) > 0) save_buffer(cmd_buffer);
            check_for_dquotes_to_duplicate();
            system(cmd_buffer);
            buff_clear();
        }
    }
    return 0;
}
