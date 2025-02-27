#ifndef NFC_READER_H
#define NFC_READER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

// PCSC includes
#include <PCSC/wintypes.h>
#include <PCSC/pcsclite.h>
#include <PCSC/winscard.h>
#include <PCSC/reader.h>

// Constants
#define MAX_ATR_SIZE 33
#define MAX_READERNAME 128
#define MAX_CARDS 100
#define UID_MAX_LENGTH 10
#define MAX_CARD_NAME 50
#define MAX_WRITE_ATTEMPTS 3
#define WRITE_BLOCK_SIZE 16
#define MAX_NAME_LENGTH 50
#define MAX_EMAIL_LENGTH 100
#define MAX_PHONE_LENGTH 20

// Function declarations
void check_status(LONG rv, const char *message);
void print_uid(BYTE *uid, size_t length);

#endif // NFC_READER_H 