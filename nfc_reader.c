#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#include <PCSC/reader.h>

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
                    printf("Card UID: ");
                    for (DWORD i = 0; i < dwRecvLength; i++) {
                        printf("%02X", pbRecvBuffer[i]);
                    }
                    printf("\n");
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
