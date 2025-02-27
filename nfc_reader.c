#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// PCSC includes - make sure these are in the right order
#include <PCSC/wintypes.h>
#include <PCSC/pcsclite.h>
#include <PCSC/winscard.h>
#include <PCSC/reader.h>

#include <curl/curl.h>
#include <json-c/json.h>

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

// Add these structures and functions before main()
#define MAX_CARDS 100
#define UID_MAX_LENGTH 10
#define MAX_CARD_NAME 50  // Maximum length for card names

// Update the API URL to match your setup
static const char *api_url = "http://192.168.20.152:3000/card-access";

// Forward declarations of all functions
void print_uid(BYTE *uid, size_t length);
void handle_card_response(BYTE *response, DWORD length, BYTE **uid, size_t *uid_length);
size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
int check_card_authorization(const char *uid_hex);
void send_card_data_to_api(BYTE *uid, size_t uid_length);
void process_card(SCARDHANDLE hCard, BYTE *response, DWORD response_length);
void check_status(LONG rv, const char *message);
LONG print_card_status(SCARDHANDLE hCard);
LONG read_card_uid(SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *dwRecvLength);
LONG handle_card_connection(SCARDCONTEXT hContext, const char *reader_name);
void print_reader_state(DWORD state);
LONG initialize_pcsc(SCARDCONTEXT *hContext, char **reader_name);

// Add these helper functions that were removed earlier
void print_uid(BYTE *uid, size_t length) {
    for (size_t i = 0; i < length; i++) {
        printf("%02X", uid[i]);
    }
}

void handle_card_response(BYTE *response, DWORD length, BYTE **uid, size_t *uid_length) {
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

// Add callback declaration
size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);

int check_card_authorization(const char *uid_hex) {
    CURL *curl;
    CURLcode res;
    int authorized = 0;
    char url[256];
    
    // Construct the URL with the UID
    snprintf(url, sizeof(url), "http://192.168.20.152:3000/check-access/%s", uid_hex);
    
    struct json_object *json_response = NULL;
    
    curl = curl_easy_init();
    if (curl) {
        char *response_data = NULL;
        size_t response_size = 0;
        
        // Setup curl to write to our buffer
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        
        res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            json_response = json_tokener_parse(response_data);
            if (json_response) {
                struct json_object *auth_obj;
                if (json_object_object_get_ex(json_response, "authorized", &auth_obj)) {
                    authorized = json_object_get_boolean(auth_obj);
                }
                json_object_put(json_response);
            }
        }
        
        free(response_data);
        curl_easy_cleanup(curl);
    }
    
    return authorized;
}

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    char **response_ptr = (char**)userdata;
    size_t realsize = size * nmemb;
    
    *response_ptr = realloc(*response_ptr, realsize + 1);
    if (*response_ptr) {
        memcpy(*response_ptr, ptr, realsize);
        (*response_ptr)[realsize] = 0;
    }
    
    return realsize;
}

// Modify send_card_data_to_api to include the authorized field
void send_card_data_to_api(BYTE *uid, size_t uid_length) {
    CURL *curl;
    CURLcode res;
    struct json_object *json_obj = json_object_new_object();
    
    // Convert UID to hex string
    char uid_hex[UID_MAX_LENGTH * 2 + 1] = {0};
    for (size_t i = 0; i < uid_length; i++) {
        sprintf(uid_hex + (i * 2), "%02X", uid[i]);
    }
    
    // Check if card is authorized
    int is_authorized = check_card_authorization(uid_hex);
    printf("Card authorization status: %s\n", is_authorized ? "Authorized" : "Not authorized");
    
    // Create JSON payload matching server expectations
    json_object_object_add(json_obj, "uid", json_object_new_string(uid_hex));
    json_object_object_add(json_obj, "authorized", json_object_new_boolean(is_authorized));
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

// Simplify process_card to only handle UID
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
    
    // Send only UID data to API
    send_card_data_to_api(uid, uid_length);
}

void check_status(LONG rv, const char *message) {
    if (rv != SCARD_S_SUCCESS) {
        printf("%s: %s\n", message, pcsc_stringify_error(rv));
        exit(1);
    }
}

// New helper functions to reduce nesting
LONG initialize_pcsc(SCARDCONTEXT *hContext, char **reader_name) {
    LONG rv;
    LPTSTR mszReaders;
    DWORD dwReaders;

    rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, hContext);
    if (rv != SCARD_S_SUCCESS) return rv;

    rv = SCardListReaders(*hContext, NULL, NULL, &dwReaders);
    if (rv != SCARD_S_SUCCESS) return rv;

    mszReaders = malloc(dwReaders);
    rv = SCardListReaders(*hContext, NULL, mszReaders, &dwReaders);
    if (rv != SCARD_S_SUCCESS) {
        free(mszReaders);
        return rv;
    }

    *reader_name = strdup(mszReaders);
    free(mszReaders);
    return SCARD_S_SUCCESS;
}

void print_reader_state(DWORD state) {
    printf("Reader State: ");
    if (state & SCARD_STATE_EMPTY)        printf("Empty ");
    if (state & SCARD_STATE_PRESENT)      printf("Card Present ");
    if (state & SCARD_STATE_MUTE)         printf("Mute ");
    if (state & SCARD_STATE_UNAVAILABLE)  printf("Unavailable ");
    printf("\n");
}

LONG handle_card_connection(SCARDCONTEXT hContext, const char *reader_name) {
    LONG rv;
    SCARDHANDLE hCard;
    DWORD dwActiveProtocol;
    BYTE pbRecvBuffer[32];
    DWORD dwRecvLength;

    rv = SCardConnect(hContext, reader_name, SCARD_SHARE_SHARED,
                     SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
                     &hCard, &dwActiveProtocol);
    if (rv != SCARD_S_SUCCESS) {
        printf("Failed to connect to card: %s\n", pcsc_stringify_error(rv));
        return rv;
    }

    printf("Protocol: %s\n", 
        dwActiveProtocol == SCARD_PROTOCOL_T0 ? "T=0" :
        dwActiveProtocol == SCARD_PROTOCOL_T1 ? "T=1" : "Unknown");

    rv = print_card_status(hCard);
    if (rv != SCARD_S_SUCCESS) {
        SCardDisconnect(hCard, SCARD_UNPOWER_CARD);
        return rv;
    }

    rv = read_card_uid(hCard, pbRecvBuffer, &dwRecvLength);
    if (rv == SCARD_S_SUCCESS) {
        process_card(hCard, pbRecvBuffer, dwRecvLength);
    }

    SCardDisconnect(hCard, SCARD_UNPOWER_CARD);
    printf("Card disconnected\n");
    return rv;
}

LONG print_card_status(SCARDHANDLE hCard) {
    BYTE pbAtr[MAX_ATR_SIZE];
    DWORD dwAtrLen = sizeof(pbAtr);
    DWORD dwState, dwProtocol;
    char szReader[MAX_READERNAME];
    DWORD dwReaderLen = sizeof(szReader);

    LONG rv = SCardStatus(hCard, szReader, &dwReaderLen, &dwState, 
                         &dwProtocol, pbAtr, &dwAtrLen);
    if (rv != SCARD_S_SUCCESS || dwAtrLen == 0) return rv;

    printf("ATR: ");
    for (DWORD i = 0; i < dwAtrLen; i++) {
        printf("%02X", pbAtr[i]);
    }
    printf("\n");
    return SCARD_S_SUCCESS;
}

LONG read_card_uid(SCARDHANDLE hCard, BYTE *pbRecvBuffer, DWORD *dwRecvLength) {
    BYTE cmd_get_uid[] = { 0xFF, 0xCA, 0x00, 0x00, 0x00 };
    *dwRecvLength = 32; // Fix: Use actual buffer size instead of sizeof pointer
    printf("Sending Get UID command: FF CA 00 00 00\n");
    
    return SCardTransmit(hCard, SCARD_PCI_T1, cmd_get_uid,
                        sizeof(cmd_get_uid), NULL,
                        pbRecvBuffer, dwRecvLength);
}

// Simplified main function
int main() {
    curl_global_init(CURL_GLOBAL_ALL);
    
    SCARDCONTEXT hContext;
    LONG rv;
    char *reader_name;
    SCARD_READERSTATE rgReaderStates[1];

    rv = initialize_pcsc(&hContext, &reader_name);
    if (rv != SCARD_S_SUCCESS) {
        printf("Failed to initialize PCSC: %s\n", pcsc_stringify_error(rv));
        return 1;
    }

    printf("=== NFC Reader Initialized ===\n");
    printf("Reader: %s\n", reader_name);
    printf("Waiting for cards. Press Ctrl+C to exit.\n");
    printf("==============================\n\n");

    rgReaderStates[0].szReader = reader_name;
    rgReaderStates[0].dwCurrentState = SCARD_STATE_UNAWARE;

    while (1) {
        rv = SCardGetStatusChange(hContext, INFINITE, rgReaderStates, 1);
        if (rv != SCARD_S_SUCCESS) {
            printf("Status change error: %s\n", pcsc_stringify_error(rv));
            continue;
        }

        print_reader_state(rgReaderStates[0].dwEventState);

        // Card inserted
        if ((rgReaderStates[0].dwEventState & SCARD_STATE_PRESENT) &&
            !(rgReaderStates[0].dwCurrentState & SCARD_STATE_PRESENT)) {
            printf("\n=== Card Detected ===\n");
            handle_card_connection(hContext, reader_name);
            printf("===================\n\n");
        }

        // Card removed
        if (!(rgReaderStates[0].dwEventState & SCARD_STATE_PRESENT) &&
            rgReaderStates[0].dwCurrentState & SCARD_STATE_PRESENT) {
            printf("Card removed - Waiting for next card\n\n");
        }

        rgReaderStates[0].dwCurrentState = rgReaderStates[0].dwEventState;
    }

    SCardReleaseContext(hContext);
    free(reader_name);
    curl_global_cleanup();
    return 0;
}
