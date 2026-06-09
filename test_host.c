/**
 * @file test_host.c
 * @brief USB HID loopback test host application using HIDAPI.
 *
 * This program tests a loopback HID device (such as the EZ-USB FX2LP running the
 * fx2hid firmware) by sending a configurable pattern of data and verifying that
 * the exact same pattern is read back.
 *
 * Compilation Instructions:
 * -------------------------
 * 1. Install HIDAPI library.
 *    - On Debian/Ubuntu: sudo apt-get install libhidapi-dev
 *    - On macOS: brew install hidapi
 *    - On Windows (MinGW):
 *      You can compile by linking against the setupapi library:
 *      gcc test_host.c -o test_host -lhidapi -lsetupapi
 *      Or if compiling directly with the hidapi source file:
 *      gcc test_host.c path/to/hidapi/windows/hid.c -Ipath/to/hidapi/hidapi -o test_host -lsetupapi
 *    - On Windows (MSVC):
 *      cl test_host.c /I <path_to_hidapi_headers> /link <path_to_hidapi_lib>\hidapi.lib setupapi.lib
 *
 * Usage:
 * ------
 *   test_host <vid>:<pid> <num_bytes> <pattern> [start_value]
 *
 * Arguments:
 *   - <vid>:<pid>   Vendor ID and Product ID in hex (e.g., 0925:1234 or 0x0925:0x1234)
 *   - <num_bytes>   Number of data bytes to send (must match the report size of the firmware)
 *   - <pattern>     Data pattern to send:
 *                     "inc"  - Increasing numbers: a, a+1, a+2, ...
 *                     "dec"  - Decreasing numbers: a+n-1, a+n-2, ...
 *                     "rand" - Random bytes
 *   - [start_value] Optional starting value 'a' for "inc" and "dec" patterns (hex or decimal, default is 0)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Try to include the standard hidapi headers */
#if defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
#include <hidapi.h>
#else
#include "hidapi.h"
#endif

#define MAX_DUMP_BYTES 64

/* Helper to print a buffer in hex format */
void print_hex_dump(const char *label, const unsigned char *buf, int len) {
    printf("%s (total %d bytes):\n  ", label, len);
    int dump_len = len > MAX_DUMP_BYTES ? MAX_DUMP_BYTES : len;
    for (int i = 0; i < dump_len; i++) {
        printf("%02X ", buf[i]);
        if ((i + 1) % 16 == 0 && i + 1 < dump_len) {
            printf("\n  ");
        }
    }
    if (len > MAX_DUMP_BYTES) {
        printf("... [truncated, first %d bytes shown]", MAX_DUMP_BYTES);
    }
    printf("\n");
}

/* Helper to list all connected HID devices for troubleshooting */
void list_hid_devices(void) {
    printf("\nAvailable HID devices:\n");
    struct hid_device_info *devs, *cur_dev;
    devs = hid_enumerate(0x0, 0x0);
    cur_dev = devs;
    int count = 0;
    while (cur_dev) {
        printf("  Device %d: VID 0x%04hX, PID 0x%04hX\n", count++, cur_dev->vendor_id, cur_dev->product_id);
        printf("    Path: %s\n", cur_dev->path);
        if (cur_dev->manufacturer_string) {
            printf("    Manufacturer: %ls\n", cur_dev->manufacturer_string);
        }
        if (cur_dev->product_string) {
            printf("    Product:      %ls\n", cur_dev->product_string);
        }
        printf("    Release:      %hu\n", cur_dev->release_number);
        printf("    Interface:    %d\n", cur_dev->interface_number);
        printf("\n");
        cur_dev = cur_dev->next;
    }
    if (count == 0) {
        printf("  No HID devices detected.\n");
    }
    hid_free_enumeration(devs);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <vid>:<pid> <num_bytes> <pattern> [start_value]\n", argv[0]);
        fprintf(stderr, "Patterns:\n");
        fprintf(stderr, "  inc   - Increasing bytes starting from [start_value] (default 0)\n");
        fprintf(stderr, "  dec   - Decreasing bytes starting from [start_value] + n - 1\n");
        fprintf(stderr, "  rand  - Random bytes\n");
        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "  %s 0925:1234 1024 inc 0x00\n", argv[0]);
        fprintf(stderr, "  %s 0925:1234 64 rand\n", argv[0]);
        return 1;
    }

    /* 1. Parse VID and PID */
    unsigned short vid = 0;
    unsigned short pid = 0;
    char *colon = strchr(argv[1], ':');
    if (!colon) {
        fprintf(stderr, "Error: Invalid VID:PID format. Expected format is 'vid:pid' in hex (e.g. 0925:1234).\n");
        return 1;
    }
    *colon = '\0';
    vid = (unsigned short)strtol(argv[1], NULL, 16);
    pid = (unsigned short)strtol(colon + 1, NULL, 16);
    *colon = ':'; /* Restore original string */

    /* 2. Parse Number of Bytes */
    int n = atoi(argv[2]);
    if (n <= 0) {
        fprintf(stderr, "Error: Number of bytes to send must be a positive integer.\n");
        return 1;
    }

    /* 3. Parse Pattern type */
    char *pattern = argv[3];
    if (strcmp(pattern, "inc") != 0 && strcmp(pattern, "dec") != 0 && strcmp(pattern, "rand") != 0) {
        fprintf(stderr, "Error: Unknown pattern '%s'. Choose from: inc, dec, rand.\n", pattern);
        return 1;
    }

    /* 4. Parse Optional Start Value */
    unsigned char start_val = 0;
    if (argc >= 5) {
        start_val = (unsigned char)strtol(argv[4], NULL, 0);
    }

    printf("Loopback Test Configuration:\n");
    printf("  Target Device: VID 0x%04X, PID 0x%04X\n", vid, pid);
    printf("  Data Bytes:    %d\n", n);
    printf("  Pattern:       %s\n", pattern);
    if (strcmp(pattern, "rand") != 0) {
        printf("  Start Value:   0x%02X (%d)\n", start_val, start_val);
    }
    printf("\n");

    /* Initialize HIDAPI */
    if (hid_init() < 0) {
        fprintf(stderr, "Error: Failed to initialize HIDAPI.\n");
        return 1;
    }

    /* Open the device */
    hid_device *handle = hid_open(vid, pid, NULL);
    if (!handle) {
        fprintf(stderr, "Error: Failed to open device VID 0x%04X, PID 0x%04X.\n", vid, pid);
        fprintf(stderr, "Please verify that the device is connected and you have permission to access it.\n");
        list_hid_devices();
        hid_exit();
        return 1;
    }

    printf("Successfully opened HID device!\n");

    /* Allocate buffers */
    /* write_buf[0] must contain the Report ID (0x00 for no Report ID) */
    unsigned char *write_buf = (unsigned char *)malloc(n + 1);
    unsigned char *read_buf = (unsigned char *)malloc(n);
    if (!write_buf || !read_buf) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        free(write_buf);
        free(read_buf);
        hid_close(handle);
        hid_exit();
        return 1;
    }

    write_buf[0] = 0x00; /* Report ID */

    /* Generate pattern data starting at write_buf[1] */
    if (strcmp(pattern, "inc") == 0) {
        for (int i = 0; i < n; i++) {
            write_buf[i + 1] = (unsigned char)(start_val + i);
        }
    } else if (strcmp(pattern, "dec") == 0) {
        for (int i = 0; i < n; i++) {
            write_buf[i + 1] = (unsigned char)(start_val + n - 1 - i);
        }
    } else if (strcmp(pattern, "rand") == 0) {
        srand((unsigned int)time(NULL));
        for (int i = 0; i < n; i++) {
            write_buf[i + 1] = (unsigned char)(rand() % 256);
        }
    }

    /* Print out sent data sample */
    print_hex_dump("Sent Data", write_buf + 1, n);

    /* Send the OUT Report */
    printf("\nSending OUT report...\n");
    int written = hid_write(handle, write_buf, n + 1);
    if (written < 0) {
        fprintf(stderr, "Error: hid_write failed: %ls\n", hid_error(handle));
        free(write_buf);
        free(read_buf);
        hid_close(handle);
        hid_exit();
        return 1;
    }
    printf("hid_write wrote %d bytes (including 1-byte Report ID).\n", written);

    /* Read the IN Report back */
    printf("Waiting for loopback IN report...\n");
    memset(read_buf, 0, n);
    
    /* Read with a timeout of 3000ms (3 seconds) */
    int bytes_read = hid_read_timeout(handle, read_buf, n, 3000);
    if (bytes_read < 0) {
        fprintf(stderr, "Error: hid_read failed: %ls\n", hid_error(handle));
    } else if (bytes_read == 0) {
        fprintf(stderr, "Error: Read timed out! No report received.\n");
        fprintf(stderr, "Note: If the firmware expects a different report size than %d bytes, it will wait\n", n);
        fprintf(stderr, "      for more bytes before triggering a transmit. Ensure the size matches the firmware.\n");
    } else {
        printf("Received IN report of %d bytes.\n", bytes_read);
        print_hex_dump("Received Data", read_buf, bytes_read);

        /* Compare buffers */
        int mismatches = 0;
        int compare_limit = bytes_read < n ? bytes_read : n;
        for (int i = 0; i < compare_limit; i++) {
            if (read_buf[i] != write_buf[i + 1]) {
                if (mismatches < 10) {
                    printf("  Mismatch at index %d: Expected 0x%02X, Got 0x%02X\n", i, write_buf[i + 1], read_buf[i]);
                }
                mismatches++;
            }
        }
        if (bytes_read != n) {
            printf("  Size Mismatch: Expected %d bytes, Received %d bytes\n", n, bytes_read);
            mismatches += abs(n - bytes_read);
        }

        if (mismatches == 0) {
            printf("\n>>> SUCCESS: Loopback matches perfectly! <<<\n");
        } else {
            printf("\n>>> FAILURE: %d mismatch(es) detected. <<<\n", mismatches);
        }
    }

    /* Clean up */
    free(write_buf);
    free(read_buf);
    hid_close(handle);
    hid_exit();
    return 0;
}
