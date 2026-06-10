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
void print_hex_dump(const char *label, const unsigned char *buf, int len, int truncate_output) {
    printf("%s (total %d bytes):\n  ", label, len);
    int dump_len = len;
    if (truncate_output && len > MAX_DUMP_BYTES) {
        dump_len = MAX_DUMP_BYTES;
    }
    for (int i = 0; i < dump_len; i++) {
        printf("%02X ", buf[i]);
        if ((i + 1) % 16 == 0 && i + 1 < dump_len) {
            printf("\n  ");
        }
    }
    if (truncate_output && len > MAX_DUMP_BYTES) {
        printf("\n  ... [truncated, first %d bytes shown]", MAX_DUMP_BYTES);
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
    int truncate_output = 0;
    unsigned char in_rep_id = 0;
    unsigned char out_rep_id = 0;
    int pos_argc = 0;
    char *pos_argv[6];

    pos_argv[pos_argc++] = argv[0];
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--truncate") == 0 || strcmp(argv[i], "-t") == 0) {
            truncate_output = 1;
        } else if (strcmp(argv[i], "--in-id") == 0 || strcmp(argv[i], "-i") == 0) {
            if (i + 1 < argc) {
                in_rep_id = (unsigned char)strtol(argv[++i], NULL, 0);
            } else {
                fprintf(stderr, "Error: Missing value for %s option.\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "--out-id") == 0 || strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                out_rep_id = (unsigned char)strtol(argv[++i], NULL, 0);
            } else {
                fprintf(stderr, "Error: Missing value for %s option.\n", argv[i]);
                return 1;
            }
        } else {
            if (pos_argc < 5) {
                pos_argv[pos_argc++] = argv[i];
            } else {
                fprintf(stderr, "Error: Too many positional arguments.\n");
                return 1;
            }
        }
    }

    if (pos_argc < 4) {
        fprintf(stderr, "Usage: %s <vid>:<pid> <num_bytes> <pattern> [start_value] [options]\n", argv[0]);
        fprintf(stderr, "Patterns:\n");
        fprintf(stderr, "  inc   - Increasing bytes starting from [start_value] (default 0)\n");
        fprintf(stderr, "  dec   - Decreasing bytes starting from [start_value] + payload_len - 1\n");
        fprintf(stderr, "  rand  - Random bytes\n");
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  -i, --in-id <id>   Expected IN Report ID (default: 0 for none)\n");
        fprintf(stderr, "  -o, --out-id <id>  OUT Report ID to prepend (default: 0 for none)\n");
        fprintf(stderr, "  -t, --truncate     Truncate output hex dumps to first %d bytes (default: print full)\n", MAX_DUMP_BYTES);
        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "  %s 0925:1234 1024 inc 0x00\n", argv[0]);
        fprintf(stderr, "  %s 0925:1234 1024 rand -o 0x02 -i 0x01\n", argv[0]);
        return 1;
    }

    /* 1. Parse VID and PID */
    unsigned short vid = 0;
    unsigned short pid = 0;
    char *colon = strchr(pos_argv[1], ':');
    if (!colon) {
        fprintf(stderr, "Error: Invalid VID:PID format. Expected format is 'vid:pid' in hex (e.g. 0925:1234).\n");
        return 1;
    }
    *colon = '\0';
    vid = (unsigned short)strtol(pos_argv[1], NULL, 16);
    pid = (unsigned short)strtol(colon + 1, NULL, 16);
    *colon = ':'; /* Restore original string */

    /* 2. Parse Number of Bytes */
    int n = atoi(pos_argv[2]);
    if (n <= 0) {
        fprintf(stderr, "Error: Number of bytes to send must be a positive integer.\n");
        return 1;
    }

    /* 3. Parse Pattern type */
    char *pattern = pos_argv[3];
    if (strcmp(pattern, "inc") != 0 && strcmp(pattern, "dec") != 0 && strcmp(pattern, "rand") != 0) {
        fprintf(stderr, "Error: Unknown pattern '%s'. Choose from: inc, dec, rand.\n", pattern);
        return 1;
    }

    /* 4. Parse Optional Start Value */
    unsigned char start_val = 0;
    if (pos_argc >= 5) {
        start_val = (unsigned char)strtol(pos_argv[4], NULL, 0);
    }

    printf("Loopback Test Configuration:\n");
    printf("  Target Device: VID 0x%04X, PID 0x%04X\n", vid, pid);
    printf("  Total Bytes:   %d\n", n);
    printf("  Pattern:       %s\n", pattern);
    if (strcmp(pattern, "rand") != 0) {
        printf("  Start Value:   0x%02X (%d)\n", start_val, start_val);
    }
    printf("  OUT Report ID: 0x%02X (%s)\n", out_rep_id, out_rep_id == 0 ? "None" : "Prepend");
    printf("  IN Report ID:  0x%02X (%s)\n", in_rep_id, in_rep_id == 0 ? "None" : "Verify");
    printf("  Truncate Out:  %s\n", truncate_output ? "Yes" : "No");
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
    int write_buf_len = (out_rep_id == 0) ? n + 1 : n;
    unsigned char *write_buf = (unsigned char *)malloc(write_buf_len);
    unsigned char *read_buf = (unsigned char *)malloc(n);
    if (!write_buf || !read_buf) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        free(write_buf);
        free(read_buf);
        hid_close(handle);
        hid_exit();
        return 1;
    }

    write_buf[0] = out_rep_id;

    /* Generate pattern data starting at write_buf[1] */
    int pattern_len = (out_rep_id == 0) ? n : n - 1;
    if (strcmp(pattern, "inc") == 0) {
        for (int i = 0; i < pattern_len; i++) {
            write_buf[i + 1] = (unsigned char)(start_val + i);
        }
    } else if (strcmp(pattern, "dec") == 0) {
        for (int i = 0; i < pattern_len; i++) {
            write_buf[i + 1] = (unsigned char)(start_val + pattern_len - 1 - i);
        }
    } else if (strcmp(pattern, "rand") == 0) {
        srand((unsigned int)time(NULL));
        for (int i = 0; i < pattern_len; i++) {
            write_buf[i + 1] = (unsigned char)(rand() % 256);
        }
    }

    /* Print out sent data sample */
    if (out_rep_id == 0) {
        print_hex_dump("Sent Data Pattern", write_buf + 1, n, truncate_output);
    } else {
        print_hex_dump("Sent Report (including OUT ID)", write_buf, n, truncate_output);
    }

    /* Send the OUT Report */
    printf("\nSending OUT report...\n");
    int written = hid_write(handle, write_buf, write_buf_len);
    if (written < 0) {
        fprintf(stderr, "Error: hid_write failed: %ls\n", hid_error(handle));
        free(write_buf);
        free(read_buf);
        hid_close(handle);
        hid_exit();
        return 1;
    }
    printf("hid_write wrote %d bytes (including Report ID byte).\n", written);

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
        if (in_rep_id == 0) {
            print_hex_dump("Received Data Pattern", read_buf, bytes_read, truncate_output);
        } else {
            print_hex_dump("Received Report (including IN ID)", read_buf, bytes_read, truncate_output);
        }

        /* Compare buffers */
        int mismatches = 0;
        
        /* 1. Verify Report ID if non-zero and we received at least 1 byte */
        if (in_rep_id != 0 && bytes_read > 0) {
            if (read_buf[0] != in_rep_id) {
                printf("  Report ID Mismatch: Expected 0x%02X (IN ID), Got 0x%02X\n", in_rep_id, read_buf[0]);
                mismatches++;
            }
        }

        /* 2. Verify payload data */
        unsigned char *send_payload = write_buf + 1;
        int send_payload_len = (out_rep_id == 0) ? n : n - 1;
        unsigned char *recv_payload = (in_rep_id == 0) ? read_buf : read_buf + 1;
        int recv_payload_len = (in_rep_id == 0) ? bytes_read : bytes_read - 1;

        if (recv_payload_len < 0) {
            recv_payload_len = 0;
        }

        int compare_len = send_payload_len < recv_payload_len ? send_payload_len : recv_payload_len;
        for (int i = 0; i < compare_len; i++) {
            if (recv_payload[i] != send_payload[i]) {
                if (mismatches < 10) {
                    printf("  Mismatch at payload index %d: Expected 0x%02X, Got 0x%02X\n", i, send_payload[i], recv_payload[i]);
                }
                mismatches++;
            }
        }

        /* 3. Verify sizes */
        if (bytes_read != n) {
            printf("  Total Size Mismatch: Expected %d bytes, Received %d bytes\n", n, bytes_read);
            mismatches += abs(n - bytes_read);
        } else if (send_payload_len != recv_payload_len) {
            printf("  Payload Size Mismatch: Expected %d data bytes, Received %d data bytes\n", send_payload_len, recv_payload_len);
            mismatches += abs(send_payload_len - recv_payload_len);
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
