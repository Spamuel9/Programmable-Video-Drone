#include <windows.h>
#include <stdio.h>
#include <string.h>

#define PACKET_SIZE 9       // Adjust if your packet size is different
#define HEADER_BYTE 0x59    // Expected header byte (0x59 prints as 'Y')

int main(void) {
    HANDLE hSerial = CreateFile("\\\\.\\COM12",  // Use extended syntax for COM12
                                GENERIC_READ | GENERIC_WRITE,
                                0,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);
    if (hSerial == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        fprintf(stderr, "Error opening COM port (Error code %lu)\n", err);
        return 1;
    }

    // Configure the serial port
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        fprintf(stderr, "Error getting current serial parameters\n");
        CloseHandle(hSerial);
        return 1;
    }
    dcbSerialParams.BaudRate = CBR_115200;  // Update as needed per your datasheet
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity   = NOPARITY;
    if (!SetCommState(hSerial, &dcbSerialParams)) {
        fprintf(stderr, "Error setting serial port parameters\n");
        CloseHandle(hSerial);
        return 1;
    }

    // Set timeouts
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    
    if (!SetCommTimeouts(hSerial, &timeouts)) {
        fprintf(stderr, "Error setting timeouts\n");
        CloseHandle(hSerial);
        return 1;
    }

    unsigned char buffer[256];
    int bufferIndex = 0;
    DWORD bytesRead;

    while (1) {
        // Read one byte at a time (for simplicity)
        if (ReadFile(hSerial, &buffer[bufferIndex], 1, &bytesRead, NULL) && bytesRead == 1) {
            bufferIndex++;
            // When we have at least one full packet, check for header and parse
            if (bufferIndex >= PACKET_SIZE) {
                for (int i = 0; i <= bufferIndex - PACKET_SIZE; i++) {
                    // Look for packet header (two consecutive 0x59 bytes)
                    if (buffer[i] == HEADER_BYTE && buffer[i+1] == HEADER_BYTE) {
                        // Extract the distance (assuming byte 2 is low and byte 3 is high)
                        int distance = buffer[i+2] + (buffer[i+3] << 8);
                        printf("Measured Distance: %d\n", distance);

                        // Optionally, you can verify the checksum here using the rest of the bytes.
                        
                        // Remove the processed packet from the buffer
                        int remaining = bufferIndex - (i + PACKET_SIZE);
                        memmove(buffer, buffer + i + PACKET_SIZE, remaining);
                        bufferIndex = remaining;
                        break;  // Process one packet per loop iteration
                    }
                }
            }
        } else {
            // No byte read or error; pause briefly
            Sleep(10);
        }
    }

    CloseHandle(hSerial);
    return 0;
}
