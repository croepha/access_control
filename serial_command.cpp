


#if 0
static const int MAX_LEN;
static char buffer[MAX_LEN];
static int len = 0;

void serial_process() {
    for(;;) {
        auto new_count = Serial.available();
        if (new_count <= 0) break;
        if (new_count + len >= MAX_LEN) {
            Serial.println("Serial in overflow resetting...");
            len = 0;
        }
    }
    for(;;) {
        auto nl = strnchr(buffer, len, "\n");
        if (!nl) break;
        *nl = 0;
        serial_handle_command(buffer);
        memmove(nl+1, buffer, (len-(nl-buffer+1)));
    }
    
    
    
}
#endif


