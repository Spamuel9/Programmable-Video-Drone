#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <stdint.h>
#include <C:\Users\samue\Documents\Spring 2025\462\RF Code\c_library_v2-master/common/mavlink.h>

#define SERIAL_PORT "\\\\.\\COM13"  // Adjust for sender radio
#define BAUD_RATE CBR_57600
#define MAX_MSG_LENGTH 50  // MAVLink STATUSTEXT max length

// Open Serial Port
HANDLE open_serial(const char *device) {
    HANDLE hSerial = CreateFile(device, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) {
        printf("❌ Error opening serial port: %ld\n", GetLastError());
        return NULL;
    }

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        printf("❌ Error getting serial port state\n");
        CloseHandle(hSerial);
        return NULL;
    }

    dcbSerialParams.BaudRate = BAUD_RATE;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        printf("❌ Error setting serial port parameters\n");
        CloseHandle(hSerial);
        return NULL;
    }

    return hSerial;
}

// Send MAVLink message (supports multi-chunk messages)
void send_text(HANDLE serialHandle, const char* message) {
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len;

    int msg_length = strlen(message);
    int chunks = (msg_length / MAX_MSG_LENGTH) + 1;  // Calculate number of chunks

    for (int i = 0; i < chunks; i++) {
        char text_buffer[MAX_MSG_LENGTH];
        memset(text_buffer, 0, sizeof(text_buffer));
        strncpy(text_buffer, message + (i * MAX_MSG_LENGTH), MAX_MSG_LENGTH - 1);  // Copy chunk

        mavlink_msg_statustext_pack(1, 200, &msg, MAV_SEVERITY_INFO, text_buffer, 0, 0);
        len = mavlink_msg_to_send_buffer(buf, &msg);

        DWORD bytesWritten;
        if (!WriteFile(serialHandle, buf, len, &bytesWritten, NULL)) {
            printf("❌ Error sending message chunk.\n");
        } else {
            printf("📡 Sent chunk: %s\n", text_buffer);
        }
        FlushFileBuffers(serialHandle);  // Ensure data is sent
        Sleep(100);
    }
}

// Read responses (Non-blocking)
void read_mavlink(HANDLE serialHandle) {
    mavlink_message_t msg;
    mavlink_status_t status;
    uint8_t c;
    DWORD bytesRead;
    
    // Set up non-blocking read
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    SetCommTimeouts(serialHandle, &timeouts);

    while (ReadFile(serialHandle, &c, 1, &bytesRead, NULL) && bytesRead > 0) {
        if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
            if (msg.msgid == MAVLINK_MSG_ID_STATUSTEXT) {
                mavlink_statustext_t response;
                mavlink_msg_statustext_decode(&msg, &response);

                char received_text[MAX_MSG_LENGTH + 1];
                memset(received_text, 0, sizeof(received_text));
                strncpy(received_text, response.text, MAX_MSG_LENGTH);
                received_text[MAX_MSG_LENGTH] = '\0';

                printf("✅ Response: %s\n", received_text);
            }
        }
    }

    PurgeComm(serialHandle, PURGE_RXCLEAR | PURGE_TXCLEAR);  // Clear buffer after reading
}

int main() {
    HANDLE serialHandle = open_serial(SERIAL_PORT);
    if (serialHandle == NULL) return 1;

    char message[MAX_MSG_LENGTH * 5];

    while (1) {
        printf("\n> ");
        fflush(stdout);  // Flush output before input
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = 0;

        if (strcmp(message, "EXIT") == 0) {
            printf("🔴 Exiting...\n");
            break;
        }

        if (strlen(message) > 0) {
            send_text(serialHandle, message);
            read_mavlink(serialHandle);
        }
        fflush(stdin);  // Ensure input buffer is cleared
    }

    CloseHandle(serialHandle);
    return 0;
}
