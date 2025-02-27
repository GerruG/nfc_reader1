#ifndef API_HANDLER_H
#define API_HANDLER_H

#include <PCSC/winscard.h>
#include <stddef.h>

// Function declarations
void send_card_data_to_api(BYTE *uid, size_t uid_length);
int check_card_authorization(const char *uid_hex);
size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);

#endif // API_HANDLER_H 