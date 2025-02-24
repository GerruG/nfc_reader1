#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#include <PCSC/reader.h>

// Add missing constants
#ifndef MAX_ATR_SIZE
#define MAX_ATR_SIZE 33
#endif

#ifndef MAX_READERNAME
#define MAX_READERNAME 128
#endif

#ifndef SCARD_STATE_EMPTY
#define SCARD_STATE_EMPTY 0x00000010
#endif

#ifndef SCARD_STATE_MUTE
#define SCARD_STATE_MUTE 0x00000200
#endif

#ifndef SCARD_STATE_UNAVAILABLE
#define SCARD_STATE_UNAVAILABLE 0x00000008
#endif

#ifndef SCARD_S_SUCCESS
#define SCARD_S_SUCCESS 0
#endif

#ifndef INFINITE
#define INFINITE 0xFFFFFFFF
#endif

// PC/SC Constants if not already defined
#ifndef SCARD_STATE_UNAWARE
#define SCARD_STATE_UNAWARE 0x00000000
#define SCARD_STATE_PRESENT 0x00000020
#define SCARD_SHARE_SHARED 2
#define SCARD_PROTOCOL_T0 1
#define SCARD_PROTOCOL_T1 2
#define SCARD_SCOPE_SYSTEM 2
#define SCARD_UNPOWER_CARD 2
#endif

// Declare external SCARD_PCI_T1
extern const SCARD_IO_REQUEST g_rgSCardT1Pci;
#define SCARD_PCI_T1 (&g_rgSCardT1Pci)

// Add these structures and functions before main()
#define MAX_CARDS 100
#define UID_MAX_LENGTH 10
#define MAX_CARD_NAME 50  // Maximum length for card names

typedef struct {
    BYTE uid[UID_MAX_LENGTH];
    size_t uid_length;
    char name[MAX_CARD_NAME];
} CardAccess;

static CardAccess authorized_cards[MAX_CARDS];
static int num_authorized_cards = 0;

void print_uid(BYTE *uid, size_t length) {
    for (size_t i = 0; i < length; i++) {
        printf("%02X", uid[i]);
    }
}

int compare_uid(BYTE *uid1, size_t len1, BYTE *uid2, size_t len2) {
    if (len1 != len2) return 0;
    return memcmp(uid1, uid2, len1) == 0;
}

void add_card_access(BYTE *uid, size_t length) {
    if (num_authorized_cards >= MAX_CARDS) {
        printf("Maximum number of authorized cards reached\n");
        return;
    }
    
    // Check if card already exists
    for (int i = 0; i < num_authorized_cards; i++) {
        if (compare_uid(authorized_cards[i].uid, authorized_cards[i].uid_length, uid, length)) {
            printf("Card already authorized as: %s\n", authorized_cards[i].name);
            return;
        }
    }
    
    // Get name for the card
    printf("Enter a name for this card (max %d characters): ", MAX_CARD_NAME - 1);
    char name[MAX_CARD_NAME];
    scanf(" %[^\n]", name);  // Read until newline
    
    // Copy data to new card entry
    memcpy(authorized_cards[num_authorized_cards].uid, uid, length);
    authorized_cards[num_authorized_cards].uid_length = length;
    strncpy(authorized_cards[num_authorized_cards].name, name, MAX_CARD_NAME - 1);
    authorized_cards[num_authorized_cards].name[MAX_CARD_NAME - 1] = '\0';  // Ensure null termination
    
    num_authorized_cards++;
    printf("Card authorized successfully as: %s\n", name);
}

void remove_card_access(BYTE *uid, size_t length) {
    for (int i = 0; i < num_authorized_cards; i++) {
        if (compare_uid(authorized_cards[i].uid, authorized_cards[i].uid_length, uid, length)) {
            printf("Removing access for card: %s\n", authorized_cards[i].name);
            // Remove card by shifting remaining cards
            for (int j = i; j < num_authorized_cards - 1; j++) {
                memcpy(authorized_cards[j].uid, authorized_cards[j + 1].uid, authorized_cards[j + 1].uid_length);
                authorized_cards[j].uid_length = authorized_cards[j + 1].uid_length;
                strcpy(authorized_cards[j].name, authorized_cards[j + 1].name);
            }
            num_authorized_cards--;
            printf("Card access removed\n");
            return;
        }
    }
    printf("Card not found in authorized list\n");
}

int check_card_access(BYTE *uid, size_t length) {
    for (int i = 0; i < num_authorized_cards; i++) {
        if (compare_uid(authorized_cards[i].uid, authorized_cards[i].uid_length, uid, length)) {
            return 1;
        }
    }
    return 0;
}

// Add this helper function before process_card
void handle_card_response(BYTE *response, DWORD length, BYTE **uid, size_t *uid_length) {
    // Check if we have at least 2 bytes for status
    if (length < 2) {
        *uid = NULL;
        *uid_length = 0;
        return;
    }

    // Check if command was successful (status bytes should be 0x90 0x00)
    if (response[length - 2] != 0x90 || response[length - 1] != 0x00) {
        *uid = NULL;
        *uid_length = 0;
        return;
    }

    // Set the UID pointer and length (excluding status bytes)
    *uid = response;
    *uid_length = length - 2;
}

// Add a function to display all authorized cards
void list_authorized_cards() {
    if (num_authorized_cards == 0) {
        printf("No cards are currently authorized\n");
        return;
    }
    
    printf("\nAuthorized Cards:\n");
    printf("----------------\n");
    for (int i = 0; i < num_authorized_cards; i++) {
        printf("%d. %s (UID: ", i + 1, authorized_cards[i].name);
        print_uid(authorized_cards[i].uid, authorized_cards[i].uid_length);
        printf(")\n");
    }
    printf("\n");
}

// Modify the process_card function
void process_card(SCARDHANDLE hCard, BYTE *response, DWORD response_length) {
    BYTE *uid;
    size_t uid_length;
    
    handle_card_response(response, response_length, &uid, &uid_length);
    
    if (uid == NULL || uid_length == 0) {
        printf("Invalid card response\n");
        return;
    }

    printf("\nCard UID: ");
    print_uid(uid, uid_length);
    printf("\n");
    
    // Check access and display name if authorized
    int authorized = 0;
    for (int i = 0; i < num_authorized_cards; i++) {
        if (compare_uid(authorized_cards[i].uid, authorized_cards[i].uid_length, uid, uid_length)) {
            printf("Access granted - Card is authorized as: %s\n", authorized_cards[i].name);
            authorized = 1;
            break;
        }
    }
    if (!authorized) {
        printf("Access denied - Card is not authorized\n");
    }
    
    printf("\nCommands:\n");
    printf("1. Add card access\n");
    printf("2. Remove card access\n");
    printf("3. List all authorized cards\n");
    printf("4. Continue without changes\n");
    
    printf("Enter command (1-4): ");
    char cmd;
    scanf(" %c", &cmd);
    
    switch (cmd) {
        case '1':
            add_card_access(uid, uid_length);
            break;
        case '2':
            remove_card_access(uid, uid_length);
            break;
        case '3':
            list_authorized_cards();
            break;
        case '4':
            break;
        default:
            printf("Invalid command\n");
    }
}

void check_status(LONG rv, const char *message) {
    if (rv != SCARD_S_SUCCESS) {
        printf("%s: %s\n", message, pcsc_stringify_error(rv));
        exit(1);
    }
}

int main() {
    SCARDCONTEXT hContext;
    SCARDHANDLE hCard;
    DWORD dwActiveProtocol;
    BYTE pbRecvBuffer[32];
    DWORD dwRecvLength;
    LONG rv;
    SCARD_READERSTATE rgReaderStates[1];
    char *reader_name;

    // Initialize PCSC context
    rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
    check_status(rv, "Failed to establish context");

    // List available NFC readers
    LPTSTR mszReaders;
    DWORD dwReaders;
    rv = SCardListReaders(hContext, NULL, NULL, &dwReaders);
    check_status(rv, "Failed to list readers");

    mszReaders = malloc(dwReaders);
    rv = SCardListReaders(hContext, NULL, mszReaders, &dwReaders);
    check_status(rv, "Failed to get reader list");

    reader_name = strdup(mszReaders);
    printf("=== NFC Reader Initialized ===\n");
    printf("Reader: %s\n", reader_name);
    printf("Waiting for cards. Press Ctrl+C to exit.\n");
    printf("==============================\n\n");

    // Set up the reader state
    rgReaderStates[0].szReader = reader_name;
    rgReaderStates[0].dwCurrentState = SCARD_STATE_UNAWARE;

    while (1) {
        // Wait for card status change
        rv = SCardGetStatusChange(hContext, INFINITE, rgReaderStates, 1);
        if (rv != SCARD_S_SUCCESS) {
            printf("Status change error: %s\n", pcsc_stringify_error(rv));
            continue;
        }

        // Print reader state changes
        DWORD state = rgReaderStates[0].dwEventState;
        printf("Reader State: ");
        if (state & SCARD_STATE_EMPTY)        printf("Empty ");
        if (state & SCARD_STATE_PRESENT)      printf("Card Present ");
        if (state & SCARD_STATE_MUTE)         printf("Mute ");
        if (state & SCARD_STATE_UNAVAILABLE)  printf("Unavailable ");
        printf("\n");

        // Check if a card was inserted
        if (rgReaderStates[0].dwEventState & SCARD_STATE_PRESENT &&
            !(rgReaderStates[0].dwCurrentState & SCARD_STATE_PRESENT)) {
            
            printf("\n=== Card Detected ===\n");
            
            // Connect to the card
            rv = SCardConnect(hContext, reader_name, SCARD_SHARE_SHARED,
                            SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
                            &hCard, &dwActiveProtocol);
            
            if (rv == SCARD_S_SUCCESS) {
                printf("Protocol: %s\n", 
                    dwActiveProtocol == SCARD_PROTOCOL_T0 ? "T=0" :
                    dwActiveProtocol == SCARD_PROTOCOL_T1 ? "T=1" : "Unknown");

                // Get card status
                BYTE pbAtr[MAX_ATR_SIZE];
                DWORD dwAtrLen = sizeof(pbAtr);
                DWORD dwState, dwProtocol;
                char szReader[MAX_READERNAME];
                DWORD dwReaderLen = sizeof(szReader);

                rv = SCardStatus(hCard, szReader, &dwReaderLen, &dwState, 
                               &dwProtocol, pbAtr, &dwAtrLen);
                
                if (rv == SCARD_S_SUCCESS && dwAtrLen > 0) {
                    printf("ATR: ");
                    for (DWORD i = 0; i < dwAtrLen; i++) {
                        printf("%02X", pbAtr[i]);
                    }
                    printf("\n");
                }

                // Send command to get NFC UID
                BYTE cmd_get_uid[] = { 0xFF, 0xCA, 0x00, 0x00, 0x00 };
                dwRecvLength = sizeof(pbRecvBuffer);
                printf("Sending Get UID command: FF CA 00 00 00\n");
                
                rv = SCardTransmit(hCard, SCARD_PCI_T1, cmd_get_uid,
                                 sizeof(cmd_get_uid), NULL,
                                 pbRecvBuffer, &dwRecvLength);
                
                if (rv == SCARD_S_SUCCESS) {
                    process_card(hCard, pbRecvBuffer, dwRecvLength);
                } else {
                    printf("Failed to read card UID: %s\n", pcsc_stringify_error(rv));
                }

                // Disconnect the card
                SCardDisconnect(hCard, SCARD_UNPOWER_CARD);
                printf("Card disconnected\n");
            } else {
                printf("Failed to connect to card: %s\n", pcsc_stringify_error(rv));
            }
            printf("===================\n\n");
        }
        
        // Check if card was removed
        if (!(rgReaderStates[0].dwEventState & SCARD_STATE_PRESENT) &&
            rgReaderStates[0].dwCurrentState & SCARD_STATE_PRESENT) {
            printf("Card removed - Waiting for next card\n\n");
        }

        // Update the current state for the next iteration
        rgReaderStates[0].dwCurrentState = rgReaderStates[0].dwEventState;
    }

    // Cleanup
    SCardReleaseContext(hContext);
    free(mszReaders);
    free(reader_name);
    
    return 0;
}
