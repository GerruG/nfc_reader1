#include "api_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <json-c/json.h>

static const char *api_url = "http://192.168.20.152:3000/card-access";

void send_card_data_to_api(BYTE *uid, size_t uid_length) {
    // ... existing send_card_data_to_api implementation ...
}

int check_card_authorization(const char *uid_hex) {
    // ... existing check_card_authorization implementation ...
}

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    // ... existing write_callback implementation ...
}

// Move write_callback, check_card_authorization, 
// and send_card_data_to_api functions here 