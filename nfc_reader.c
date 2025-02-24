#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#include <PCSC/reader.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <time.h>

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

// Update the API URL to match your setup
static const char *api_url = "http://192.168.20.152:3000/card-access";

// Add these type definitions after the includes and before the constants
#ifndef BYTE
typedef unsigned char BYTE;
#endif

#ifndef DWORD
typedef unsigned long DWORD;
#endif

#ifndef LPTSTR
typedef char* LPTSTR;
#endif

typedef unsigned long SCARDCONTEXT;
typedef unsigned long SCARDHANDLE;

typedef struct {
    const char *szReader;
    void *pvUserData;
    DWORD dwCurrentState;
    DWORD dwEventState;
    DWORD cbAtr;
    unsigned char rgbAtr[33];
} SCARD_READERSTATE;

typedef struct {
    DWORD dwProtocol;
    DWORD cbPciLength;
} SCARD_IO_REQUEST;

// Add these new functions before main()

// Simplify the send_card_data_to_api function
void send_card_data_to_api(BYTE *uid, size_t uid_length, int authorized, const char *cardName) {
    CURL *curl;
    CURLcode res;
    struct json_object *json_obj = json_object_new_object();
    
    // Convert UID to hex string
    char uid_hex[UID_MAX_LENGTH * 2 + 1] = {0};
    for (size_t i = 0; i < uid_length; i++) {
        sprintf(uid_hex + (i * 2), "%02X", uid[i]);
    }
    
    // Create JSON payload
    json_object_object_add(json_obj, "uid", json_object_new_string(uid_hex));
    json_object_object_add(json_obj, "authorized", json_object_new_boolean(authorized));
    json_object_object_add(json_obj, "name", json_object_new_string(cardName ? cardName : ""));
    json_object_object_add(json_obj, "timestamp", json_object_new_int64(time(NULL)));
    
    const char *json_str = json_object_to_json_string(json_obj);
    
    curl = curl_easy_init();
    if (curl) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        curl_easy_setopt(curl, CURLOPT_URL, api_url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // Enable verbose output for debugging
        
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "Failed to send data to API: %s\n", curl_easy_strerror(res));
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    
    json_object_put(json_obj);
}

// Modify the process_card function to include API call
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
    
    // Read card name from block 4
    BYTE cmd_get_name[] = { 0xFF, 0xB0, 0x00, 0x04, 0x10 };  // Read 16 bytes from block 4
    BYTE nameBuffer[32] = {0};
    DWORD dwNameLength = sizeof(nameBuffer);
    
    LONG rv = SCardTransmit(hCard, SCARD_PCI_T1, cmd_get_name, sizeof(cmd_get_name), 
                           NULL, nameBuffer, &dwNameLength);
    
    char cardName[MAX_CARD_NAME] = {0};
    int authorized = 0;
    
    if (rv == SCARD_S_SUCCESS) {
        // Check if response is valid (status bytes 0x90 0x00)
        if (dwNameLength >= 2 && nameBuffer[dwNameLength - 2] == 0x90 && 
            nameBuffer[dwNameLength - 1] == 0x00) {
            
            size_t nameDataLength = dwNameLength - 2;
            if (nameDataLength > 16) nameDataLength = 16;
            memcpy(cardName, nameBuffer, nameDataLength);
            cardName[nameDataLength] = '\0';
            
            // Consider card authorized if it has a non-empty name
            authorized = (strlen(cardName) > 0);
            printf("Card Name: %s\n", cardName);
            printf("Access %s\n", authorized ? "granted" : "denied");
        } else {
            printf("Invalid name response from card\n");
        }
    } else {
        printf("Failed to read card name: %s\n", pcsc_stringify_error(rv));
    }
    
    // Send data to API
    send_card_data_to_api(uid, uid_length, authorized, cardName);
}

void check_status(LONG rv, const char *message) {
    if (rv != SCARD_S_SUCCESS) {
        printf("%s: %s\n", message, pcsc_stringify_error(rv));
        exit(1);
    }
}

int main() {
    // Initialize curl before the main loop
    curl_global_init(CURL_GLOBAL_ALL);
    
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
    
    // Add cleanup before return
    curl_global_cleanup();
    return 0;
}
