


#if 1

#include <sqlite3.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#ifndef debugf
#include <HardwareSerial.h>
#define debugf Serial.printf
#endif


static const int MAX_LEN = 1024;
static char line_buffer[MAX_LEN];
static int len = 0;

static char* line;

extern sqlite3*db;
static char* get_word() {
    while (*line && isspace(*line)) line++;
    char* word_start = line++;
    while (*line && !isspace(*line)) line++;
    *line++ = 0;
    return word_start;
}


bool sqlexec_first_time = false;
static int sqlexec_callback(void *data, int argc, char **argv, char **azColName) {
    int i;
    if (sqlexec_first_time) {
        for (i = 0; i<argc; i++) {
            if (i)
                debugf("\t");
            debugf("%s", azColName[i]);
        }
        debugf("\n");
        sqlexec_first_time = false;
    }
    for (i = 0; i<argc; i++) {
        if (i)
            debugf("\t");
        debugf("%s", argv[i] ? argv[i] : "NULL");
    }
    debugf("\n");
    return 0;
}


char *zErrMsg = 0;
int sqlexec(const char *sql) {
    if (db == NULL) {
        debugf("No database open\n");
        return 0;
    }
    sqlexec_first_time = true;
    int rc = sqlite3_exec(db, sql, sqlexec_callback, (void*)0, &zErrMsg);
    if (rc != SQLITE_OK) {
        debugf("SQL error: %d %s\n", 
               sqlite3_extended_errcode(db),
               zErrMsg);
        sqlite3_free(zErrMsg);
    } else
        debugf("Operation done successfully\n");
    return rc;
}

char serial_process_get_char();
int sqlexec(const char *sql);
void serial_command_rm(char* file_name);
void serial_command_ls();

void serial_process() {
    bool processing_commands = 1;
    while (processing_commands) {
        // process a line
        len = 0;
        bool line_is_junk = 0;
        for (;;) {
            char new_char = line_buffer[len++] = serial_process_get_char();
            if (len >= MAX_LEN) { line_is_junk=1; len=0; debugf("Line buffer overflow\n"); }
            if (new_char == '\n') { 
                line_buffer[len-1] = 0;
                line = line_buffer;
                break;
            }
        }
        if (line_is_junk) {
            debugf("Ignoring this line\n");
        } else {
            debugf("Got a line: '%s'\n", line_buffer);
            
            auto command_word = get_word();
            debugf("Command word: '%s'\n", command_word);
            
            if (strcmp(command_word, "ls") == 0) {
                serial_command_ls();
            } else if (strcmp(command_word, "boot") == 0) {
                debugf("ok, booting now\n");
                processing_commands = 0;
            } else if (strcmp(command_word, "rm") == 0) {
                serial_command_rm(get_word());
            } else if (strcmp(command_word, "sql") == 0) {
                sqlexec(line);
            }
        }
    }
    
}
#endif


