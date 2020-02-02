#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef int64_t  s64;
typedef int32_t  s32;
typedef int16_t  s16;
typedef int8_t   s8;

void db_add_user(const char* user_name);
void db_get_delete_list(char* tmp_buf, size_t tmp_buf_len);
bool db_check_and_log_access();
void db_list_log(char* tmp_buf, size_t tmp_buf_len);
void db_prune();
void db_del_user(u64 uid);
void db_init(char*file_name);
bool db_has_users();

u64  rtc_offset = 0;
u8   rfid[10];
u8   rfid_len;
char tmp_buf[4096];

/*
    japan town
    lower pacific heights
    */


static const char*check_str1 =
R"STRINGLITERAL()STRINGLITERAL";


#define set_rfid(m_s) rfid_len=(sizeof m_s)-1; memcpy(rfid, m_s, rfid_len); 

u64 debug_time;
u64 get_adjusted_time() { return debug_time++; }


#define check(m_log, m_delete) \
db_list_log(tmp_buf, sizeof tmp_buf); \
assert(strcmp(tmp_buf, m_log) == 0); \
db_get_delete_list(tmp_buf, sizeof tmp_buf); \
assert(strcmp(tmp_buf, m_delete) == 0); 


void dump() {
    db_list_log(tmp_buf, sizeof tmp_buf);
    printf("log: \"%s\" end_log\n", tmp_buf);
    db_get_delete_list(tmp_buf, sizeof tmp_buf);
    printf("del: \"%s\" end_del\n", tmp_buf);
}

#define check_access(m_r, m_v) \
set_rfid(m_r); \
assert(db_check_and_log_access() == m_v);



int main () {
    setbuf(stderr, 0);
    setbuf(stdout, 0);
    
    debug_time = 0;
    
    
    unlink("build/test_db0");
    db_init("build/test_db0");
    
    check("", "");
    
    
    check_access("abcdef", 0);
    check_access("abcdef-", 0);
    assert(db_has_users() == 0);
    
    check("", "");
    
    db_list_log(tmp_buf, sizeof tmp_buf);
    assert(strcmp(tmp_buf, "") == 0);
    
    set_rfid("abcdef");
    db_add_user("new_user0");
    
    assert(db_has_users() == 1);
    
    
    check(
        "<p> 2 : new_user0 : ADDED </p>",
        "<p><a href=/del?id=1>2:new_user0</a></p>");
    
    
    check_access("abcdef", 1);
    check_access("abcdef-", 0);
    
    check(
        "<p> 3 : new_user0 : ACCESS </p><p> 2 : new_user0 : ADDED </p>",
        "<p><a href=/del?id=1>3:new_user0</a></p>");
    
    assert(db_has_users() == 1);
    
    db_del_user(1);
    assert(db_has_users() == 0);
    
    
    check(
        "<p> 5 : new_user0 : REMOVED </p>"
        "<p> 3 : new_user0 : ACCESS </p>"
        "<p> 2 : new_user0 : ADDED </p>",
        "");
    
    check_access("abcdef", 0);
    check_access("abcdef-", 0);
    
    
    check(
        "<p> 5 : new_user0 : REMOVED </p>"
        "<p> 3 : new_user0 : ACCESS </p>"
        "<p> 2 : new_user0 : ADDED </p>",
        "");
    
    
    assert(db_has_users() == 0);
    
    set_rfid("abcdef");
    db_add_user("new_user1");
    
    assert(db_has_users() == 1);
    
    
    check(
        "<p> 8 : new_user1 : ADDED </p>"
        "<p> 5 : new_user0 : REMOVED </p>"
        "<p> 3 : new_user0 : ACCESS </p>"
        "<p> 2 : new_user0 : ADDED </p>",
        "<p><a href=/del?id=2>8:new_user1</a></p>");
    
    
    check_access("abcdef", 1);
    check_access("abcdef-", 0);
    
    check(
        "<p> 9 : new_user1 : ACCESS </p>"
        "<p> 8 : new_user1 : ADDED </p>"
        "<p> 5 : new_user0 : REMOVED </p>"
        "<p> 3 : new_user0 : ACCESS </p>"
        "<p> 2 : new_user0 : ADDED </p>",
        "<p><a href=/del?id=2>9:new_user1</a></p>");
    
    
    
}