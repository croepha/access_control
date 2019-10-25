#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SPIFFS.h> 
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <ESPmDNS.h>
#include <WebServer.h>


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
void db_init();


u64 rtc_offset = 0;
char tmp_buf[4096];
u8 rfid[10];
u8 rfid_len;


inline static void debug_print_rfid() {
    for (int i=0;i<rfid_len;i++) {
        Serial.printf("%02x ", rfid[i]);
    }
    Serial.printf("\n");
}

const int RST_PIN = 22; // Reset pin
const int SS_PIN = 21; // Slave select pin

const int PIN_BLUEal  = 15; 
const int PIN_GREENal = 2; 
const int PIN_REDal   = 16; 
const int PIN_UNLOCK  = 4;
 
const int PIN_TOUCH0  = 13; 
const int PIN_TOUCH1  = 12; 
const int PIN_TOUCH2  = 14; 


MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

void rfid_read() {
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    rfid_len = 0;
    return;
  }  
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    rfid_len = 0;
    return;
  }
  rfid_len = mfrc522.uid.size;
  memcpy(rfid, mfrc522.uid.uidByte, rfid_len);
}


int unlock_ticks = 0;
int error_ticks = 0;

bool down0 = 0;
bool down1 = 0;
bool down2 = 0;
int idle_sustain = 0;
char pin_entry_buffer[10];

int touch0_sustain = 0;
int touch1_sustain = 0;
int touch2_sustain = 0;

#define IS_TOUCH(id) __is_touch(id, PIN_TOUCH ## id, touch ## id ##_sustain, down ## id)
bool __is_touch(int id, int pin, int&sustain, bool&was_down) {
  bool down = 0; if (touchRead(pin) < 25 /*threshold*/) { 
    if (sustain < 2) sustain++;
    else down = 1;   
  } else sustain = 0;
  bool pressed = 0; if (down) {
    if (!was_down) pressed = 1;
    was_down = 1;
  } else was_down = 0;
  return pressed;
}


void setup() {
  Serial.begin(115200); // Initialize serial communications with the PC

  while (!Serial); // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  Serial.println("BOOT\n");


  auto r1 = SPIFFS.begin(true);
  assert(r1);

  db_init();

  SPI.begin(); // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522
  mfrc522.PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader details
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));

  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
  //pinMode(5, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(5), rfid_isr, CHANGE);

  pinMode(PIN_BLUEal, OUTPUT);
  pinMode(PIN_GREENal, OUTPUT);
  pinMode(PIN_UNLOCK, OUTPUT);
  pinMode(PIN_REDal, OUTPUT);
  digitalWrite(PIN_BLUEal, LOW);
  digitalWrite(PIN_GREENal, HIGH);


  pinMode(PIN_TOUCH0, OUTPUT);
  pinMode(PIN_TOUCH1, OUTPUT);
  pinMode(PIN_TOUCH2, OUTPUT);

}


void pin_entry_buffer_add_char(char c) {
  memmove(pin_entry_buffer, pin_entry_buffer+1, sizeof pin_entry_buffer-1);
  pin_entry_buffer[sizeof pin_entry_buffer-1] = c;
}

enum {
  STATE_IDLE,
  STATE_UNLOCK,
  STATE_PROGRAM,
} state;

void set_led_red() {
  digitalWrite(PIN_REDal  , LOW );
  digitalWrite(PIN_GREENal, HIGH);
  digitalWrite(PIN_BLUEal , HIGH);
}
void set_led_green() {
  digitalWrite(PIN_REDal  , HIGH);
  digitalWrite(PIN_GREENal, LOW );
  digitalWrite(PIN_BLUEal , HIGH);
}
void set_led_blue() {
  digitalWrite(PIN_REDal  , HIGH);
  digitalWrite(PIN_GREENal, HIGH);
  digitalWrite(PIN_BLUEal , LOW );
}
void set_led_white() {
  digitalWrite(PIN_REDal  , LOW );
  digitalWrite(PIN_GREENal, LOW );
  digitalWrite(PIN_BLUEal , LOW );
}


#define redirect(m_a) server.sendHeader("Location", m_a,true); server.send(302, "text/plain",""); 
#define set_no_cache() \
server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate", true); \
server.sendHeader("Pragma", "no-cache", true); \
server.sendHeader("Expires", "0", true); 


void loop() {
  // TODO: if no users, should go to program mode...
  // TODO: if time is not set, then goto maintenance...
  idle: {
    Serial.println("IDLE\n");

    // if day_start + 24 hours > now : goto maintenance;
    
    for(;;) { 
      bool t0 = IS_TOUCH(0);
      bool t1 = IS_TOUCH(1);
      bool t2 = IS_TOUCH(2);
      
      if (t0) { error_ticks=5; pin_entry_buffer_add_char('1'); } 
      if (t1) { error_ticks=5; pin_entry_buffer_add_char('2'); } 
      if (t2) { error_ticks=5; pin_entry_buffer_add_char('3'); } 
      
      if (!t0 && !t1 && !t2 ) {
        if (idle_sustain < 100) {
          idle_sustain++;
        } else {
          pin_entry_buffer_add_char('\0');
        }
      } else {
        idle_sustain = 0;
      } 
  
  
      char pin[] = {'1', '2', '3', '3'};
    
      if(memcmp(pin_entry_buffer + sizeof(pin_entry_buffer) - sizeof(pin), pin, sizeof(pin)) == 0) {
          pin_entry_buffer_add_char('\0');
          goto unlock;
      }
          
      if(error_ticks>0) {
        error_ticks--;
        set_led_red(); 
      } else {
        set_led_blue(); 
      }
  
      rfid_read();
      if (!rfid_len) continue;      
      Serial.print("UID: ");
      debug_print_rfid();
  
      if (db_check_and_log_access()) {
        goto unlock;
      } else {
        error_ticks = 100;
      }
    }
    delay(1);
  }

  unlock: {
    Serial.println("UNLOCK\n");
    unlock_ticks = 1000;
    error_ticks = 0;
      
    for(;;) { 
      if (unlock_ticks<=0) {
        goto idle;
      }
      unlock_ticks--;
      bool t0 = IS_TOUCH(0);
      if (t0) goto program;
      digitalWrite(PIN_UNLOCK, unlock_ticks%16!=0);
          
      set_led_green();
      delay(1);  
    } 
  }

  program: {
    Serial.println("PROGRAM\n");
    WiFi.softAP("access_control", "access_control");
    MDNS.begin("access_control");
    WebServer server(80);
    set_led_white();

    bool should_stop = 0;

    server.onNotFound([&](){
      redirect("/");
    });
    server.on("/view_log", [&](){
      set_no_cache();
      db_list_log(tmp_buf, sizeof tmp_buf);
      server.send(200, "text/html", tmp_buf);
    });
    server.on("/del", [&](){
      db_del_user(atoi(server.arg("id").c_str())); // what happens if arg isn't present? oh well, we just trust the client in program mode...
      redirect("/delete_list");
    });
    server.on("/delete_list", [&](){
      set_no_cache();
      db_get_delete_list(tmp_buf, sizeof tmp_buf);
      server.send(200, "text/html", tmp_buf);
    });
    server.on("/", [&](){
      if (server.method() == HTTP_POST) {
        int username_arg = -1;
        for (int i = 0; i< server.args(); i++) {
          if (server.argName(i) == "username") {
             username_arg = i; break;
          }
        }
        if (username_arg == -1) {
          server.send(500, "text/plain", "11111");
        } else {
          server.send(200, "text/html", "<h1>Scan new card now</h1>");          
          for (;;) {
            rfid_read();
            if (rfid_len) break;

          }
          Serial.print("username: ");
          Serial.println(server.arg(username_arg));
          Serial.print("UID: ");
          debug_print_rfid();
          db_add_user(server.arg(username_arg).c_str());
          ESP.restart();


        }
      } else {
        server.send(200, "text/html", 
        "<p><a href=/view_log>View recent accesses</a></p>"
        "<p><a href=/delete_list>Delete Users</a></p>"
        "<p><form method=post><h1>Enter name for new person</h1>"
        "<input type=text name=username /><input type=submit /></p>"
        );
      }
    });

    server.begin();

    for(;;) {
      server.handleClient();
      bool t0 = IS_TOUCH(0);
      if (t0) goto idle;
    }

  }

  maintenence: {
    // TODO: Swap log files, 
    // TODO: Log onto wifi
    // TODO: get time with NTP, set it
    // TOOD: Delete old users
    // TODO: Save time...
    ESP.restart();
    
  }
  
}
