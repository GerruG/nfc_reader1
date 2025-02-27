# NFC Card Reader for Raspberry Pi

This project implements an NFC card reader using a Raspberry Pi and an NFC/RFID reader module. The program reads card UIDs and communicates with a REST API for access control.

## Hardware Requirements

- Raspberry Pi (any model with USB ports)
- ACR122U NFC Reader or compatible PCSC-compatible reader

## Software Prerequisites

1. Update your Raspberry Pi:
   ```bash
   sudo apt update
   sudo apt upgrade -y
   ```

2. Install required libraries:
    ```bash
    sudo apt install -y \
    build-essential \
    libcurl4-openssl-dev \
    libjson-c-dev \
    pcscd \
    pcsc-tools
    ```

3. Enable and start the PCSC service:
   ```bash
   sudo systemctl enable pcscd
   sudo systemctl start pcscd
   ```

## installation

1. Clone the repository:


2. Build the project:

```bash
make
```

