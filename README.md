
2. Available commands when a card is detected:
- `w <block> <data>` - Write data to a specific block
- `r <block>` - Read data from a specific block
- `p` - Read stored user profile
- `n` - Register new user (stores name, email, phone)
- `q` - Quit and disconnect from card

## Card Block Layout

- Block 0: Manufacturer data (read-only)
- Block 4: Name
- Block 5: Email
- Block 6: Phone
- Every 4th block (3, 7, 11, etc.): Sector trailer (contains keys)

## Notes

- The program attempts multiple authentication methods with common keys
- Some blocks may be protected or read-only
- Maximum data length per block is 16 bytes
- Be careful when writing to system blocks (0-3)

## File Structure

- `main.c` - Main program loop and card detection
- `card_operations.c` - Card reading/writing operations
- `api_handler.c` - API communication functions
- `card_operations.h` - Card operation declarations
- `api_handler.h` - API function declarations
- `Makefile` - Build configuration

## Troubleshooting

1. Reader not found:
   - Ensure your NFC reader is connected
   - Check if pcscd service is running: `sudo systemctl status pcscd`

2. Authentication failed:
   - Card might use different keys
   - Try cleaning the card surface
   - Ensure proper card placement

3. Write failed:
   - Block might be protected
   - Authentication might have failed
   - Card might be read-only

