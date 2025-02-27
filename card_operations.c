#include "card_operations.h"
#include "api_handler.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Common keys for Mifare Classic
static const BYTE DEFAULT_KEYS[][6] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Most common default
    {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}, // Common alternative
    {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7}, // Common in some systems
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // All zeros
    {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5}, // Another common key
    {0x4D, 0x3A, 0x99, 0xC3, 0x51, 0xDD}  // Common in access control
};
#define NUM_KEYS (sizeof(DEFAULT_KEYS) / sizeof(DEFAULT_KEYS[0]))

// Helper function to print UID
static void print_uid(BYTE *uid, size_t length) {
    for (size_t i = 0; i < length; i++) {
        printf("%02X", uid[i]);
    }
}

// Helper function to handle card response
static void handle_card_response(BYTE *response, DWORD length, BYTE **uid, size_t *uid_length) {
    if (length < 2) {
        *uid = NULL;
        *uid_length = 0;
        return;
    }

    if (response[length - 2] != 0x90 || response[length - 1] != 0x00) {
        *uid = NULL;
        *uid_length = 0;
        return;
    }

    *uid = response;
    *uid_length = length - 2;
}

int authenticate_block(SCARDHANDLE hCard, BYTE block_number, BYTE key_type, const BYTE *key) {
    // Try direct authentication first
    BYTE direct_auth[12] = {
        0xFF, 0x88, 0x00, block_number,
        0x60, // Authentication with key A
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF // Default key
    };
    
    BYTE response[2];
    DWORD response_length = sizeof(response);
    
    printf("[*] Trying direct authentication for block %d...\n", block_number);
    
    // Try each key
    for(int i = 0; i < NUM_KEYS; i++) {
        // Update key in authentication command
        memcpy(direct_auth + 6, DEFAULT_KEYS[i], 6);
        
        LONG rv = SCardTransmit(hCard, SCARD_PCI_T1, direct_auth,
                               sizeof(direct_auth), NULL,
                               response, &response_length);
        
        if (rv == SCARD_S_SUCCESS && 
            response_length >= 2 && 
            response[0] == 0x90 && 
            response[1] == 0x00) {
            printf("[+] Authentication successful with key set %d\n", i);
            return 1;
        }
        
        // Try key B if key A failed
        direct_auth[4] = 0x61; // Switch to key B
        rv = SCardTransmit(hCard, SCARD_PCI_T1, direct_auth,
                          sizeof(direct_auth), NULL,
                          response, &response_length);
        
        if (rv == SCARD_S_SUCCESS && 
            response_length >= 2 && 
            response[0] == 0x90 && 
            response[1] == 0x00) {
            printf("[+] Authentication successful with key B set %d\n", i);
            return 1;
        }
    }

    printf("[*] All authentication methods failed, attempting to proceed...\n");
    return 1; // Return success anyway to try the operation
}

int read_from_card(SCARDHANDLE hCard, BYTE block_number, BYTE *data, DWORD *data_length) {
    // First try to authenticate with key A
    if (!authenticate_block(hCard, block_number, KEY_A, DEFAULT_KEYS[0])) {
        printf("[*] Key A authentication failed, trying Key B...\n");
        if (!authenticate_block(hCard, block_number, KEY_B, DEFAULT_KEYS[0])) {
            printf("[-] Could not authenticate with either key\n");
            return 0;
        }
    }

    // Mifare Classic read command
    BYTE read_command[5] = { 
        0xFF,  // CLA
        0xB0,  // INS: READ BINARY
        0x00,  // P1
        block_number,  // P2: Block number
        0x10   // Le: Expected length (16 bytes for Mifare Classic)
    };
    
    BYTE response[32];
    DWORD response_length = sizeof(response);

    LONG rv = SCardTransmit(hCard, SCARD_PCI_T1, read_command, sizeof(read_command),
                           NULL, response, &response_length);

    if (rv != SCARD_S_SUCCESS) {
        printf("[-] Read failed: %s\n", pcsc_stringify_error(rv));
        return 0;
    }

    if (response_length >= 2) {
        if (response[response_length-2] == 0x90 && response[response_length-1] == 0x00) {
            *data_length = response_length - 2;  // Subtract status bytes
            memcpy(data, response, *data_length);
            return 1;
        }
        printf("[-] Read failed with error code: %02X %02X\n", 
               response[response_length-2], response[response_length-1]);
    }

    return 0;
}

int write_to_card(SCARDHANDLE hCard, BYTE block_number, BYTE *data, DWORD data_length) {
    // First try to authenticate with key A
    if (!authenticate_block(hCard, block_number, KEY_A, DEFAULT_KEYS[0])) {
        printf("[*] Key A authentication failed, trying Key B...\n");
        if (!authenticate_block(hCard, block_number, KEY_B, DEFAULT_KEYS[0])) {
            printf("[-] Could not authenticate with either key\n");
            return 0;
        }
    }

    // Mifare Classic write command
    BYTE write_command[21] = { 
        0xFF,  // CLA
        0xD6,  // INS: UPDATE BINARY
        0x00,  // P1
        block_number,  // P2: Block number
        0x10   // Lc: Length to write (always 16 for Mifare Classic)
    };
    
    // Pad data to 16 bytes
    BYTE padded_data[16] = {0};
    memcpy(padded_data, data, data_length > 16 ? 16 : data_length);
    
    // Copy the padded data into the command buffer
    memcpy(write_command + 5, padded_data, 16);
    
    BYTE response[2];
    DWORD response_length = sizeof(response);
    
    LONG rv = SCardTransmit(hCard, SCARD_PCI_T1, write_command, sizeof(write_command),
                           NULL, response, &response_length);
    
    if (rv != SCARD_S_SUCCESS) {
        printf("[-] Write failed: %s\n", pcsc_stringify_error(rv));
        return 0;
    }
    
    if (response_length >= 2) {
        if (response[0] == 0x90 && response[1] == 0x00) {
            printf("[+] Write successful\n");
            return 1;
        }
        printf("[-] Write failed with error code: %02X %02X\n", response[0], response[1]);
    }
    
    return 0;
}

void read_user_profile(SCARDHANDLE hCard) {
    BYTE data[WRITE_BLOCK_SIZE];
    DWORD data_length;
    
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║          User Profile Data           ║\n");
    printf("╠══════════════════════════════════════╣\n");

    // Read name from block 4
    if (read_from_card(hCard, 4, data, &data_length)) {
        data[data_length] = '\0';  // Null terminate the string
        printf("║ Name: %-30s ║\n", data);
    } else {
        printf("║ Name: <not set>                    ║\n");
    }

    // Read email from block 5
    if (read_from_card(hCard, 5, data, &data_length)) {
        data[data_length] = '\0';  // Null terminate the string
        printf("║ Email: %-29s ║\n", data);
    } else {
        printf("║ Email: <not set>                   ║\n");
    }

    // Read phone from block 6
    if (read_from_card(hCard, 6, data, &data_length)) {
        data[data_length] = '\0';  // Null terminate the string
        printf("║ Phone: %-29s ║\n", data);
    } else {
        printf("║ Phone: <not set>                   ║\n");
    }
    
    printf("╚══════════════════════════════════════╝\n");
}

void get_user_info(char *name, char *email, char *phone) {
    printf("\n[*] User Registration\n");
    printf("╔══════════════════════════════════════╗\n");
    
    printf("Enter name: ");
    fgets(name, MAX_NAME_LENGTH, stdin);
    name[strcspn(name, "\n")] = 0;
    
    printf("Enter email: ");
    fgets(email, MAX_EMAIL_LENGTH, stdin);
    email[strcspn(email, "\n")] = 0;
    
    printf("Enter phone: ");
    fgets(phone, MAX_PHONE_LENGTH, stdin);
    phone[strcspn(phone, "\n")] = 0;
    
    printf("╚══════════════════════════════════════╝\n");
}

void process_card(SCARDHANDLE hCard, BYTE *response, DWORD response_length) {
    BYTE *uid;
    size_t uid_length;
    
    handle_card_response(response, response_length, &uid, &uid_length);
    if (uid == NULL || uid_length == 0) {
        printf("[-] Invalid card response\n");
        return;
    }
    
    printf("[+] Card UID: ");
    print_uid(uid, uid_length);
    printf("\n");
    
    send_card_data_to_api(uid, uid_length);
}

void check_status(LONG rv, const char *message) {
    if (rv != SCARD_S_SUCCESS) {
        printf("%s: %s\n", message, pcsc_stringify_error(rv));
        exit(1);
    }
} 