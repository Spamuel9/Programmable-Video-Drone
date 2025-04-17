#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <stdint.h>
#include <C:\Users\samue\Documents\Spring 2025\462\RF Code\c_library_v2-master/common/mavlink.h>

#define SERIAL_PORT "\\\\.\\COM13"  // Adjust for responder radio
#define BAUD_RATE CBR_57600
#define MAX_MSG_LENGTH 50

// Open Serial Port
HANDLE open_serial(const char *device) {
    HANDLE hSerial = CreateFile(device, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) {
        printf("Error opening serial port: %ld\n", GetLastError());
        return NULL;
    }

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        printf("Error getting serial port state\n");
        CloseHandle(hSerial);
        return NULL;
    }

    dcbSerialParams.BaudRate = BAUD_RATE;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        printf("Error setting serial port parameters\n");
        CloseHandle(hSerial);
        return NULL;
    }

    return hSerial;
}

// Send acknowledgment response
void send_response(HANDLE serialHandle, const char* response_text) {
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len;

    char text_buffer[MAX_MSG_LENGTH];
    memset(text_buffer, 0, sizeof(text_buffer));
    snprintf(text_buffer, sizeof(text_buffer) - 1, "%s", response_text);

    mavlink_msg_statustext_pack(2, 201, &msg, MAV_SEVERITY_INFO, text_buffer, 0, 0);
    len = mavlink_msg_to_send_buffer(buf, &msg);

    DWORD bytesWritten;
    if (!WriteFile(serialHandle, buf, len, &bytesWritten, NULL)) {
        printf("❌ Error sending response.\n");
    } else {
        printf("✅ Sent response: %s\n", text_buffer);
    }
}

// Read and process messages
void read_mavlink(HANDLE serialHandle) {
    mavlink_message_t msg;
    mavlink_status_t status;
    uint8_t c;
    DWORD bytesRead;

    while (ReadFile(serialHandle, &c, 1, &bytesRead, NULL) && bytesRead > 0) {
        if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
            if (msg.msgid == MAVLINK_MSG_ID_STATUSTEXT) {
                mavlink_statustext_t statustext;
                mavlink_msg_statustext_decode(&msg, &statustext);

                char received_text[MAX_MSG_LENGTH + 1];
                memset(received_text, 0, sizeof(received_text));
                strncpy(received_text, statustext.text, MAX_MSG_LENGTH);
                received_text[MAX_MSG_LENGTH] = '\0';

                printf("📡 Received: %s\n", received_text);
                send_response(serialHandle, received_text);
            }
        }
    }
    PurgeComm(serialHandle, PURGE_RXCLEAR | PURGE_TXCLEAR);
}

int main() {
    HANDLE serialHandle = open_serial(SERIAL_PORT);
    if (serialHandle == NULL) return 1;

    while (1) {
        read_mavlink(serialHandle);
        Sleep(500);
    }

    CloseHandle(serialHandle);
    return 0;
}