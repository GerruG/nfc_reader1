#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "card_operations.h"
#include "api_handler.h"
#include <curl/curl.h>

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
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║        NFC Reader Initialized        ║\n");
    printf("╠══════════════════════════════════════╣\n");
    printf("║ Reader:                              ║\n");
    printf("║ %-36s ║\n", reader_name);
    printf("║                                      ║\n");
    printf("║ Status: Waiting for cards            ║\n");
    printf("║                                      ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    // Set up the reader state
    rgReaderStates[0].szReader = reader_name;
    rgReaderStates[0].dwCurrentState = SCARD_STATE_UNAWARE;

    while (1) {
        // Wait for card status change
        rv = SCardGetStatusChange(hContext, INFINITE, rgReaderStates, 1);
        if (rv != SCARD_S_SUCCESS) {
            printf("[-] Status change error: %s\n", pcsc_stringify_error(rv));
            continue;
        }

        // Print reader state changes (only if there's a change)
        if (rgReaderStates[0].dwEventState != rgReaderStates[0].dwCurrentState) {
            DWORD state = rgReaderStates[0].dwEventState;
            printf("[*] Reader State: ");
            if (state & SCARD_STATE_EMPTY)        printf("Empty");
            if (state & SCARD_STATE_PRESENT)      printf("Card Present");
            if (state & SCARD_STATE_MUTE)         printf("Mute");
            if (state & SCARD_STATE_UNAVAILABLE)  printf("Unavailable");
            printf("\n");
        }

        // Check if a card was inserted
        if (rgReaderStates[0].dwEventState & SCARD_STATE_PRESENT &&
            !(rgReaderStates[0].dwCurrentState & SCARD_STATE_PRESENT)) {
            
            printf("\n╔════════════════════════════════════╗\n");
            printf("║          Card Detected             ║\n");
            printf("╚════════════════════════════════════╝\n");
            
            // Connect to the card
            rv = SCardConnect(hContext, reader_name, SCARD_SHARE_SHARED,
                            SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
                            &hCard, &dwActiveProtocol);
            
            if (rv == SCARD_S_SUCCESS) {
                printf("[+] Connected using protocol: %s\n", 
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
                    printf("[+] ATR: ");
                    for (DWORD i = 0; i < dwAtrLen; i++) {
                        printf("%02X", pbAtr[i]);
                    }
                    printf("\n");
                }

                // Send command to get NFC UID
                BYTE cmd_get_uid[] = { 0xFF, 0xCA, 0x00, 0x00, 0x00 };
                dwRecvLength = sizeof(pbRecvBuffer);
                printf("[*] Reading card UID...\n");
                
                rv = SCardTransmit(hCard, SCARD_PCI_T1, cmd_get_uid,
                                 sizeof(cmd_get_uid), NULL,
                                 pbRecvBuffer, &dwRecvLength);
                
                if (rv == SCARD_S_SUCCESS) {
                    process_card(hCard, pbRecvBuffer, dwRecvLength);
                    
                    // After processing, enter interactive mode
                    printf("\n[*] Card Interactive Mode\n");
                    printf("[*] Commands: \n");
                    printf("    w <block> <data> - Write data to block\n");
                    printf("    r <block>        - Read data from block\n");
                    printf("    p               - Read user profile\n");
                    printf("    n               - Register new user\n");
                    printf("    q               - Quit and disconnect\n");
                    
                    char cmd[256];
                    char name[MAX_NAME_LENGTH] = {0};
                    char email[MAX_EMAIL_LENGTH] = {0};
                    char phone[MAX_PHONE_LENGTH] = {0};
                    
                    while (1) {
                        printf("\nEnter command (or q to quit): ");
                        if (fgets(cmd, sizeof(cmd), stdin) == NULL) break;
                        
                        // Remove newline
                        cmd[strcspn(cmd, "\n")] = 0;
                        
                        if (cmd[0] == 'q') break;
                        
                        if (cmd[0] == 'p') {
                            read_user_profile(hCard);
                            continue;
                        }
                        
                        if (cmd[0] == 'r') {
                            int block;
                            if (sscanf(cmd, "r %d", &block) == 1) {
                                BYTE data[WRITE_BLOCK_SIZE];
                                DWORD data_length;
                                
                                printf("[*] Reading from block %d...\n", block);
                                if (read_from_card(hCard, (BYTE)block, data, &data_length)) {
                                    printf("[+] Data (ASCII): ");
                                    for (DWORD i = 0; i < data_length; i++) {
                                        if (isprint(data[i])) {
                                            printf("%c", data[i]);
                                        } else {
                                            printf(".");
                                        }
                                    }
                                    printf("\n[+] Data (HEX): ");
                                    for (DWORD i = 0; i < data_length; i++) {
                                        printf("%02X ", data[i]);
                                    }
                                    printf("\n");
                                }
                            } else {
                                printf("[-] Invalid read command format\n");
                                printf("[*] Usage: r <block>\n");
                            }
                            continue;
                        }
                        
                        if (cmd[0] == 'n') {
                            get_user_info(name, email, phone);
                            
                            // Write name to block 4
                            printf("[*] Writing name to block 4...\n");
                            write_to_card(hCard, 4, (BYTE*)name, strlen(name));
                            
                            // Write email to block 5
                            printf("[*] Writing email to block 5...\n");
                            write_to_card(hCard, 5, (BYTE*)email, strlen(email));
                            
                            // Write phone to block 6
                            printf("[*] Writing phone to block 6...\n");
                            write_to_card(hCard, 6, (BYTE*)phone, strlen(phone));
                            
                            printf("[*] Registration complete!\n");
                            continue;
                        }
                        
                        if (cmd[0] == 'w') {
                            int block;
                            char data[WRITE_BLOCK_SIZE + 1];
                            if (sscanf(cmd, "w %d %16s", &block, data) == 2) {
                                DWORD data_len = strlen(data);
                                if (data_len > WRITE_BLOCK_SIZE) {
                                    printf("[-] Data too long (max %d bytes)\n", WRITE_BLOCK_SIZE);
                                    continue;
                                }
                                
                                printf("[*] Writing to block %d: %s\n", block, data);
                                write_to_card(hCard, (BYTE)block, (BYTE*)data, data_len);
                            } else {
                                printf("[-] Invalid write command format\n");
                                printf("[*] Usage: w <block> <data>\n");
                            }
                        }
                    }
                } else {
                    printf("[-] Failed to read card UID: %s\n", pcsc_stringify_error(rv));
                }

                // Disconnect the card
                SCardDisconnect(hCard, SCARD_UNPOWER_CARD);
                printf("[*] Card disconnected\n");
            } else {
                printf("[-] Failed to connect to card: %s\n", pcsc_stringify_error(rv));
            }
            printf("\n");
        }
        
        // Check if card was removed
        if (!(rgReaderStates[0].dwEventState & SCARD_STATE_PRESENT) &&
            rgReaderStates[0].dwCurrentState & SCARD_STATE_PRESENT) {
            printf("[*] Card removed - Waiting for next card\n\n");
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