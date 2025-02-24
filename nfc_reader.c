#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>

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

    printf("Using reader: %s\n", mszReaders);

    // Connect to the first available reader
    rv = SCardConnect(hContext, mszReaders, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);
    check_status(rv, "Failed to connect to card");

    // Send command to get NFC UID (ISO14443 cards)
    BYTE cmd_get_uid[] = { 0xFF, 0xCA, 0x00, 0x00, 0x00 };
    dwRecvLength = sizeof(pbRecvBuffer);
    rv = SCardTransmit(hCard, SCARD_PCI_T1, cmd_get_uid, sizeof(cmd_get_uid), NULL, pbRecvBuffer, &dwRecvLength);
    
    if (rv == SCARD_S_SUCCESS) {
        printf("Card UID: ");
        for (DWORD i = 0; i < dwRecvLength; i++) {
            printf("%02X", pbRecvBuffer[i]);
        }
        printf("\n");
    } else {
        printf("Failed to read card UID\n");
    }

    // Cleanup
    SCardDisconnect(hCard, SCARD_UNPOWER_CARD);
    SCardReleaseContext(hContext);
    free(mszReaders);
    
    return 0;
}
