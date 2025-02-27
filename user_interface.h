#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

#include "nfc_reader.h"

// Function declarations
void get_user_info(char *name, char *email, char *phone);
void read_user_profile(SCARDHANDLE hCard);
void handle_interactive_mode(SCARDHANDLE hCard);

#endif // USER_INTERFACE_H 