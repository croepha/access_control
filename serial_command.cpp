


#if 1

extern sqlite3*db

static const int MAX_LEN = 1024;
static char buffer[MAX_LEN];
static int len = 0;

static char* line;

static char* get_word() {
    while (*line && isspace(*line)) line++;
    char* word_start = line;
    while (*line && !isspace(*line)) line++;
    if (!*line) return 0;
    *line++ = 0;
}


bool sqlexec_first_time = false;
static int sqlexec_callback(void *data, int argc, char **argv, char **azColName) {
    int i;
    if (sqlexec_first_time) {
        for (i = 0; i<argc; i++) {
            if (i)
                Serial.print((char) '\t');
            Serial.printf("%s", azColName[i]);
        }
        Serial.printf("\n");
        sqlexec_first_time = false;
    }
    for (i = 0; i<argc; i++) {
        if (i)
            Serial.print((char) '\t');
        Serial.printf("%s", argv[i] ? argv[i] : "NULL");
    }
    Serial.printf("\n");
    return 0;
}


char *zErrMsg = 0;
int sqlexec(const char *sql) {
    if (db == NULL) {
        Serial.println("No database open");
        return 0;
    }
    sqlexec_first_time = true;
    long start = micros();
    int rc = sqlite3_exec(db, sql, sqlexec_callback, (void*)0, &zErrMsg);
    if (rc != SQLITE_OK) {
        Serial.print(F("SQL error: "));
        Serial.print(sqlite3_extended_errcode(db));
        Serial.print(" ");
        Serial.println(zErrMsg);
        sqlite3_free(zErrMsg);
    } else
        Serial.println(F("Operation done successfully"));
    Serial.print(F("Time taken:"));
    Serial.print(micros()-start);
    Serial.println(F(" us"));
    return rc;
}

void serial_process() {
    bool processing_commands = 1;
    while (processing_commands) {
        // process a line
        len = 0;
        bool line_is_junk = 0;
        for (;;) {
            while (Serial.available() == 0); // wait for input
            char new_char = line_buffer[len++] = Serial.read(); // read a single char
            if (len >= MAX_LEN) { line_is_junk=1; len=0; Serial.printf("Line buffer overflow\n"); }
            if (new_char == '\n') { 
                line_buffer[len-1] = 0;
                line = line_buffer;
                break;
            }
            
        }
        if (line_is_junk) {
            Serial.printf("Ignoring this line\n");
        } else {
            Serial.printf("Got a line: '%s'\n", line_buffer);
            
            auto command_word = get_word();
            Serial.printf("First word: '%s'\n", first_word);
            
            if (strcmp(command_word_word, "ls") == 0) {
                File root = SPIFFS.open("/");
                while(File file = root.openNextFile()) {
                    if (file.isDirectory) {
                        Serial.printf("LS: '%s' *DIR*\n", file.name());
                    } else {
                        unsigned long long sz = file.size();
                        Serial.printf("LS: '%s' %ull\n", file.name(), sz);
                    }
                }
                Serial.printf("LS END\n");
            } else if (strcmp(command_word_word, "boot") == 0) {
                Serial.printf("ok, booting now\n");
                processing_commands = 0;
            } else if (strcmp(command_word_word, "rm") == 0) {
                auto file_name = get_word();
                long r1 = SPIFFS.remove(file_name);
                Serial.printf("RM: '%s' -> %l\n", r1);
            } else if (strcmp(command_word_word, "sql") == 0) {
                sqlexec(line);
            }
        }
    }
    
}
#endif


