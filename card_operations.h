#ifndef CARD_OPERATIONS_H
#define CARD_OPERATIONS_H

// Only include winscard.h as it includes the other necessary headers
#include <PCSC/winscard.h>

// Constants
#define MAX_ATR_SIZE 33
#define MAX_READERNAME 128
#define MAX_WRITE_ATTEMPTS 3
#define WRITE_BLOCK_SIZE 16
#define MAX_NAME_LENGTH 50
#define MAX_EMAIL_LENGTH 100
#define MAX_PHONE_LENGTH 20
#define KEY_A 0x60
#define KEY_B 0x61

// Function declarations
int authenticate_block(SCARDHANDLE hCard, BYTE block_number, BYTE key_type, const BYTE *key);
int read_from_card(SCARDHANDLE hCard, BYTE block_number, BYTE *data, DWORD *data_length);
int write_to_card(SCARDHANDLE hCard, BYTE block_number, BYTE *data, DWORD data_length);
void read_user_profile(SCARDHANDLE hCard);
void get_user_info(char *name, char *email, char *phone);
void process_card(SCARDHANDLE hCard, BYTE *response, DWORD response_length);
void check_status(LONG rv, const char *message);

#endif // CARD_OPERATIONS_H 