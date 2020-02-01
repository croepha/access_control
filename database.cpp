

#include <sqlite3.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
//#include <SPI.h>
//#include <FS.h>
//#include <SPIFFS.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef int64_t  s64;
typedef int32_t  s32;
typedef int16_t  s16;
typedef int8_t   s8;

extern u64 rtc_offset;
extern u8 rfid[10];
extern u8 rfid_len;

#ifndef debugf
#define debugf Serial.printf
#endif

sqlite3 *db = 0;

void db_add_user(const char* user_name);
void db_get_delete_list(char* tmp_buf, size_t tmp_buf_len);
bool db_check_and_log_access();
void db_list_log(char* tmp_buf, size_t tmp_buf_len);
void db_prune();
void db_del_user(u64 uid);
void db_init(char*file_name);

u64 get_adjusted_time();


#define db_assert_ok(m_r) __db_assert_ok(m_r, __FILE__, __LINE__)
inline static void __db_assert_ok(int r, const char*file, int line) {
    if (r != SQLITE_OK) {
        debugf("ERROR: %s:%d: SQL ERROR:%d: %s\n", file, line, r, sqlite3_errmsg(db));
        abort();
    }
}

#define db_assert_eq(m_r,m_e) __db_assert_eq(m_r, m_e, __FILE__, __LINE__)
inline static void __db_assert_eq(int r, int eq, const char*file, int line) {
    if (r != eq) {
        debugf("ERROR: %s:%d: SQL ERROR:%d: %s\n", file, line, r, sqlite3_errmsg(db));
        abort();
    }
}


inline static void debug_print_rfid() {
    for (int i=0;i<rfid_len;i++) {
        debugf("%02x ", rfid[i]);
    }
    debugf("\n");
}


void db_set_rtc_offset() {
    static const char*q = "SELECT \"log\".\"when\" FROM \"log\" ORDER BY \"log\".\"id\" DESC LIMIT 1";
    sqlite3_stmt *stmt;
    auto r1 = sqlite3_prepare_v2(db, q, -1, &stmt, 0);
    db_assert_ok(r1);
    assert(stmt);
    
    auto r2 = sqlite3_step(stmt);
    if (r2 == SQLITE_DONE) {
        rtc_offset = 0;
    } else {
        db_assert_eq(r2, SQLITE_ROW);
        rtc_offset = sqlite3_column_int64(stmt, 0) - time(0);
        auto r3 = sqlite3_step(stmt);
        db_assert_eq(r3, SQLITE_DONE);
    }
    
    auto r4 = sqlite3_finalize(stmt);
    db_assert_ok(r4);
}


void db_add_user(const char* user_name) {
    debugf("DEBUG: db_add_user %d `%s'\n", rfid_len, user_name);
    debug_print_rfid();
    
    u64 now = get_adjusted_time();
    {
        static const char*q = "UPDATE \"user\" SET \"active\" = FALSE WHERE \"rfid\" = ?";
        sqlite3_stmt *stmt;
        auto r1 = sqlite3_prepare_v2(db, q, -1, &stmt, 0);
        db_assert_ok(r1);
        assert(stmt);
        auto r3 = sqlite3_bind_blob(stmt, 1, rfid, rfid_len, SQLITE_TRANSIENT);
        db_assert_ok(r3);
        auto r2 = sqlite3_step(stmt);
        db_assert_eq(r2, SQLITE_DONE);
        auto r4 = sqlite3_finalize(stmt);
        db_assert_ok(r4);
    }
    {
        static const char*q = "INSERT INTO \"user\" (\"rfid\", \"name\", \"last_seen\", \"active\") VALUES (?, ?, ?, TRUE)";
        sqlite3_stmt *stmt;
        auto r1 = sqlite3_prepare_v2(db, q, -1, &stmt, 0);
        db_assert_ok(r1);
        assert(stmt);
        auto r3 = sqlite3_bind_blob(stmt, 1, rfid, rfid_len, SQLITE_TRANSIENT);
        db_assert_ok(r3);
        auto r4 = sqlite3_bind_text(stmt, 2, user_name, -1, SQLITE_TRANSIENT);
        db_assert_ok(r4);
        auto r5 = sqlite3_bind_int64(stmt, 3, now);
        db_assert_ok(r5);
        auto r2 = sqlite3_step(stmt);
        db_assert_eq(r2, SQLITE_DONE);
        auto r6 = sqlite3_finalize(stmt);
        db_assert_ok(r6);
    }
    
    {
        auto id = sqlite3_last_insert_rowid(db);
        static const char*q = "INSERT INTO \"log\" (\"user_id\", \"when\", \"type\") VALUES (?, ?, 'N')";
        sqlite3_stmt *stmt;
        auto r1 = sqlite3_prepare_v2(db, q, -1, &stmt, 0);
        db_assert_ok(r1);
        assert(stmt);
        auto r3 = sqlite3_bind_int64(stmt, 1, id);
        db_assert_ok(r3);
        auto r4 = sqlite3_bind_int64(stmt, 2, now);
        db_assert_ok(r4);
        auto r2 = sqlite3_step(stmt);
        db_assert_eq(r2, SQLITE_DONE);
        auto r6 = sqlite3_finalize(stmt);
        db_assert_ok(r6);    
    }
    auto r90 = sqlite3_db_cacheflush(db);
    db_assert_ok(r90);
}


void db_get_delete_list(char* tmp_buf, size_t tmp_buf_len) {
    
    size_t tmp_buf_used = 0;
    *tmp_buf = 0;
    
    debugf("DEBUG: user list:\n");
    static const char*q = "SELECT \"id\", \"name\", \"last_seen\" FROM \"user\" WHERE \"active\" = TRUE";
    sqlite3_stmt *stmt;
    auto r1 = sqlite3_prepare_v2(db, q, -1, &stmt, 0);
    db_assert_ok(r1);
    assert(stmt);
    
    for(;;) {
        auto r3 = sqlite3_step(stmt);
        if (r3 == SQLITE_DONE) break;
        db_assert_eq(r3, SQLITE_ROW);
        
        auto t_id        = sqlite3_column_text(stmt, 0);
        auto t_last_seen = sqlite3_column_text(stmt, 2);
        auto t_name      = sqlite3_column_text(stmt, 1);
        
        debugf("debug1: %p, %p %p\n", t_id, t_last_seen, t_name);
        
        auto new_used = snprintf(
            tmp_buf + tmp_buf_used, 
            tmp_buf_len - tmp_buf_used,
            "<p><a href=/del?id=%s>%s:%s</a></p>",
            t_id,t_last_seen,t_name
            );
        if (new_used < 0) {
            debugf("delete list overflow\n");
            break;
        }
        tmp_buf_used += new_used;
    }
    
    auto r6 = sqlite3_finalize(stmt);
    db_assert_ok(r6);
    
}

bool db_check_and_log_access() {
    debugf("DEBUG: check and log access %d\n", rfid_len);
    debug_print_rfid();
    
    u64 now = get_adjusted_time();
    int id = -1;
    {
        static const char*q = "SELECT \"user\".\"id\" FROM \"user\" WHERE \"user\".\"active\" = TRUE AND \"user\".\"rfid\" = ?";
        sqlite3_stmt *stmt;
        auto r1 = sqlite3_prepare_v2(db, q, -1, &stmt, 0);
        db_assert_ok(r1);
        assert(stmt);
        auto r5 = sqlite3_bind_blob(stmt, 1, rfid, rfid_len, SQLITE_TRANSIENT);
        db_assert_ok(r5);
        
        
        
        auto r2 = sqlite3_step(stmt);
        if (r2 == SQLITE_DONE) {
            debugf("DEBUG: denied\n");
            auto r7 = sqlite3_finalize(stmt);
            db_assert_ok(r7);
            return 0;
        } 
        db_assert_eq(r2, SQLITE_ROW);
        id = sqlite3_column_int64(stmt, 0);
        auto r3 = sqlite3_step(stmt);
        db_assert_eq(r3, SQLITE_DONE);
        auto r6 = sqlite3_finalize(stmt);
        db_assert_ok(r6);
    }
    
    {
        static const char*q = "INSERT INTO \"log\" (\"user_id\", \"when\", \"type\") VALUES (?, ?, 'A')";
        sqlite3_stmt *stmt;
        auto r1 = sqlite3_prepare_v2(db, q, -1, &stmt, 0);
        db_assert_ok(r1);
        assert(stmt);
        auto r3 = sqlite3_bind_int64(stmt, 1, id);
        db_assert_ok(r3);
        auto r4 = sqlite3_bind_int64(stmt, 2, now);
        db_assert_ok(r4);
        auto r2 = sqlite3_step(stmt);
        db_assert_eq(r2, SQLITE_DONE);
        auto r6 = sqlite3_finalize(stmt);
        db_assert_ok(r6);    
    }
    {
        static const char*q = "UPDATE \"user\" SET \"last_seen\" = ? WHERE id = ?";
        sqlite3_stmt *stmt;
        auto r1 = sqlite3_prepare_v2(db, q, -1, &stmt, 0);
        db_assert_ok(r1);
        assert(stmt);
        auto r3 = sqlite3_bind_int64(stmt, 1, now);
        db_assert_ok(r3);
        auto r4 = sqlite3_bind_int64(stmt, 2, id);
        db_assert_ok(r4);
        auto r2 = sqlite3_step(stmt);
        db_assert_eq(r2, SQLITE_DONE);
        auto r6 = sqlite3_finalize(stmt);
        db_assert_ok(r6);    
    }
    auto r90 = sqlite3_db_cacheflush(db);
    db_assert_ok(r90);
    
    debugf("DEBUG: granted\n");
    return 1;
    
}



void db_list_log(char* tmp_buf, size_t tmp_buf_len) {
    
    size_t tmp_buf_used = 0;
    *tmp_buf = 0;
    
    static const char*q = "SELECT \"user\".\"name\", \"log\".\"when\", \"log\".\"type\" FROM \"user\", \"log\" WHERE \"log\".\"user_id\" = \"user\".\"id\" ORDER BY \"log\".\"when\" DESC";
    sqlite3_stmt *stmt = 0;
    int r1 = sqlite3_prepare_v2(db, q, -1, &stmt, 0);
    db_assert_ok(r1);
    assert(stmt);
    
    
    for(;;) {
        auto r3 = sqlite3_step(stmt);
        if (r3 == SQLITE_DONE) break;
        db_assert_eq(r3, SQLITE_ROW);
        
        const char*type = (const char*)sqlite3_column_text(stmt, 2);
        char*type_string = "????";
        if (strcmp(type, "N")==0) {
            type_string = "ADDED";
        } else if (strcmp(type, "A")==0) {
            type_string = "ACCESS";
        } else if (strcmp(type, "D")==0) {
            type_string = "REMOVED";
        }
        
        
        auto new_used = snprintf(
            tmp_buf + tmp_buf_used, 
            tmp_buf_len - tmp_buf_used,
            "<p> %s : %s : %s </p>",
            sqlite3_column_text(stmt, 1),
            sqlite3_column_text(stmt, 0),
            type_string
            );
        if (new_used < 0) {
            debugf("access list overflow\n");
            break;
        }
        tmp_buf_used += new_used;
    }
    
    auto r6 = sqlite3_finalize(stmt);
    db_assert_ok(r6);
    
}

void db_prune() {
    debugf("DEBUG: prune \n");
    u64 now = get_adjusted_time();
    {
        static const char*q = "DELETE FROM \"log\" WHERE \"when\" < ?;";
        sqlite3_stmt *stmt;
        auto r1 = sqlite3_prepare_v2(db, q, -1, &stmt, 0);
        db_assert_ok(r1);
        assert(stmt);
        auto r3 = sqlite3_bind_int64(stmt, 1, (s64)now - 60*60*24*5);
        db_assert_ok(r3);
        auto r2 = sqlite3_step(stmt);
        db_assert_eq(r2, SQLITE_DONE);
        auto r4 = sqlite3_finalize(stmt);
        db_assert_ok(r4);    
    }
    {
        static const char*q = "DELETE FROM \"user\" WHERE \"last_seen\" < ?;";
        sqlite3_stmt *stmt;
        auto r1 = sqlite3_prepare_v2(db, q, -1, &stmt, 0);
        db_assert_ok(r1);
        assert(stmt);
        auto r3 = sqlite3_bind_int64(stmt, 1, (s64)now - 60*60*24*30*6);
        db_assert_ok(r3);
        auto r2 = sqlite3_step(stmt);
        db_assert_eq(r2, SQLITE_DONE);
        auto r4 = sqlite3_finalize(stmt);
        db_assert_ok(r4);    
    }
    auto r90 = sqlite3_db_cacheflush(db);
    db_assert_ok(r90);
}

void db_del_user(u64 uid) {
    debugf("DEBUG: del user %lld\n", uid);
    u64 now = get_adjusted_time();
    {
        static const char*q = "UPDATE \"user\" SET \"active\" = FALSE WHERE \"id\" = ?;";
        sqlite3_stmt *stmt;
        auto r1 = sqlite3_prepare_v2(db, q, -1, &stmt, 0);
        db_assert_ok(r1);
        assert(stmt);
        auto r3 = sqlite3_bind_int64(stmt, 1, uid);
        db_assert_ok(r3);
        auto r2 = sqlite3_step(stmt);
        db_assert_eq(r2, SQLITE_DONE);
        auto r4 = sqlite3_finalize(stmt);
        db_assert_ok(r4);
    }
    {
        static const char*q = "INSERT INTO \"log\" (\"user_id\", \"when\", \"type\") VALUES (?, ?, 'D')";
        sqlite3_stmt *stmt;
        auto r1 = sqlite3_prepare_v2(db, q, -1, &stmt, 0);
        db_assert_ok(r1);
        assert(stmt);
        auto r3 = sqlite3_bind_int64(stmt, 1, uid);
        db_assert_ok(r3);
        auto r4 = sqlite3_bind_int64(stmt, 2, now);
        db_assert_ok(r4);
        auto r2 = sqlite3_step(stmt);
        db_assert_eq(r2, SQLITE_DONE);
        auto r6 = sqlite3_finalize(stmt);
        db_assert_ok(r6);    
    }
    auto r90 = sqlite3_db_cacheflush(db);
    db_assert_ok(r90);
}


static int callback(void *data, int argc, char **argv, char **azColName) {
    int i;
    for (i = 0; i<argc; i++){
        debugf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    debugf("\n");
    return 0;
}

void debug_exec(const char*sql) {
    debugf("Execing debug SQL: %s\n", sql);  
    char *zErrMsg = 0;
    int rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        debugf("SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    } else {
        debugf("Operation done successfully\n");
    }
    auto r90 = sqlite3_db_cacheflush(db);
    db_assert_ok(r90);
    
}


void db_init(char*file_name) {
    sqlite3_initialize();
    
    {  // "/spiffs/test3"
        auto r1 = sqlite3_open(file_name, &db);
        db_assert_ok(r1);
    }
    
    {
        static const char*q = R"SQLSQLSQL(
    CREATE TABLE IF NOT EXISTS "user" (
        'id'        INTEGER PRIMARY KEY,
        'rfid'      BLOB    NOT NULL,
        'name'      TEXT    NOT NULL,
        'last_seen' DATE    NOT NULL,
        'active'    BOOLEAN NOT NULL
    );
    )SQLSQLSQL";
        sqlite3_stmt *stmt;
        auto r1 = sqlite3_prepare_v2(db, q, -1, &stmt, 0);
        debugf("DEBUG db schema'%s'\n", q);
        db_assert_ok(r1);
        assert(stmt);
        auto r2 = sqlite3_step(stmt);
        assert(r2 == SQLITE_DONE);
        auto r4 = sqlite3_finalize(stmt);
        db_assert_ok(r4);
    }
    {
        static const char*q = R"SQLSQLSQL(
    CREATE TABLE IF NOT EXISTS log (
        "id"        INTEGER PRIMARY KEY,
        "user_id"   INTEGER NOT NULL,
        "when"      INTEGER NOT NULL,
        "type"      CHAR(1) NOT NULL
    );
    )SQLSQLSQL";
        //    FOREIGN KEY(user_id) REFERENCES user(id)  
        // constraint is nice in theory, but...
        
        sqlite3_stmt *stmt;
        auto r1 = sqlite3_prepare_v2(db, q, -1, &stmt, 0);
        db_assert_ok(r1);
        assert(stmt);
        auto r2 = sqlite3_step(stmt);
        db_assert_eq(r2, SQLITE_DONE);
        auto r4 = sqlite3_finalize(stmt);
        db_assert_ok(r4);    
    }
    auto r90 = sqlite3_db_cacheflush(db);
    db_assert_ok(r90);
    
    db_set_rtc_offset();
}



