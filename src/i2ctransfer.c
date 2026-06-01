/*
 * i2ctransfer.c - Send concatenated I2C messages via CH347 on Windows
 *
 * Port of Linux i2c-tools i2ctransfer using the WCH CH347 DLL.
 *
 * Usage:
 *   i2ctransfer.exe DEVICE_INDEX DESC [DATA] [DESC [DATA]]...
 *
 * DESC format: {r|w}LENGTH[@address]
 *   r4@0x50        - read 4 bytes from 0x50
 *   w2@0x50 0x10 0x20  - write 0x10,0x20 to 0x50
 *
 * Data suffixes (write only):
 *   =  keep value constant until LENGTH
 *   +  increase value by 1 until LENGTH
 *   -  decrease value by 1 until LENGTH
 *   p  pseudo-random until LENGTH
 *
 * Examples:
 *   i2ctransfer.exe 0 w1@0x50 0x64 r8
 *     Write reg addr 0x64, then read 8 bytes from EEPROM at 0x50
 *
 *   i2ctransfer.exe 0 w17@0x50 0x42 0xff-
 *     Write 0x42 then 0xff,0xfe,...,0xf0 to EEPROM at 0x50
 */

#include <windows.h>
#include "CH347DLL.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_MSGS 64

typedef struct {
    uint8_t addr;
    int     isRead;
    size_t  len;
    uint8_t *buf;
} msg_t;

static void print_help(void)
{
    fprintf(stderr,
        "Usage: i2ctransfer DEVICE_INDEX DESC [DATA] [DESC [DATA]]...\n"
        "\n"
        "  DESC describes a transfer in the form: {r|w}LENGTH[@address]\n"
        "    r4@0x50          - read 4 bytes from device 0x50\n"
        "    w2@0x50 0x10 0x20 - write two bytes to device 0x50\n"
        "\n"
        "  DATA suffixes (write only):\n"
        "    =  keep value constant until LENGTH\n"
        "    +  increase value by 1 until LENGTH\n"
        "    -  decrease value by 1 until LENGTH\n"
        "    p  pseudo-random until LENGTH (value as seed)\n"
        "\n"
        "Examples:\n"
        "  i2ctransfer.exe 0 w1@0x50 0x64 r8\n"
        "    Write reg addr 0x64, then read 8 bytes from EEPROM at 0x50\n"
        "\n"
        "  i2ctransfer.exe 0 w17@0x50 0x42 0xff-\n"
        "    Write 0x42 then 0xff,0xfe,...,0xf0 to EEPROM at 0x50\n");
}

/* Execute a single I2C transfer (write only, read only, or write+read).
 * A write+read pair uses CH347StreamI2C in one call so the chip can
 * generate a repeated-START between the write and read phases. */
static int do_transfer(ULONG idx, uint8_t addr,
                       const uint8_t *wbuf, size_t wlen,
                       uint8_t *rbuf, size_t rlen)
{
    uint8_t out[256];

    if (wlen && rlen) {
        /* Write then read - combined in one CH347StreamI2C call.
         * Byte 0 = device address << 1, remaining bytes = write data. */
        if (wlen > sizeof(out) - 1) {
            fprintf(stderr, "Error: Write buffer too large (%zu)\n", wlen);
            return -1;
        }
        out[0] = addr << 1;
        memcpy(&out[1], wbuf, wlen);
        if (!CH347StreamI2C(idx, (ULONG)(wlen + 1), out, (ULONG)rlen, rbuf)) {
            fprintf(stderr, "Error: I2C transfer failed (write %zu + read %zu bytes to 0x%02x)\n",
                    wlen, rlen, addr);
            return -1;
        }
    } else if (wlen) {
        /* Write only */
        if (wlen > sizeof(out) - 1) {
            fprintf(stderr, "Error: Write buffer too large (%zu)\n", wlen);
            return -1;
        }
        out[0] = addr << 1;
        memcpy(&out[1], wbuf, wlen);
        if (!CH347StreamI2C(idx, (ULONG)(wlen + 1), out, 0, NULL)) {
            fprintf(stderr, "Error: I2C write failed (%zu bytes to 0x%02x)\n", wlen, addr);
            return -1;
        }
    } else if (rlen) {
        /* Read only */
        uint8_t outAddr[2] = { addr << 1, 0 };
        if (!CH347StreamI2C(idx, 2, outAddr, (ULONG)rlen, rbuf)) {
            fprintf(stderr, "Error: I2C read failed (%zu bytes from 0x%02x)\n", rlen, addr);
            return -1;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    msg_t msgs[MAX_MSGS];
    int nmsgs = 0;
    int address = -1;
    ULONG idx = 0;
    enum { ST_DESC, ST_DATA } state = ST_DESC;
    size_t buf_idx = 0;

    if (argc < 3) {
        print_help();
        return 1;
    }

    /* First non-option arg is device index */
    idx = (ULONG)atoi(argv[1]);

    printf("current using CH347 index : %ld\n", idx);

    if (!CH347OpenDevice(idx)) {
        fprintf(stderr, "Error: Can't open device.\n");
        return 1;
    }

    /* 00=low/20KHz, 01=standard/100KHz(default), 10=fast/400KHz, 11=high/750KHz */
    if (!CH347I2C_Set(idx, 1)) {
        fprintf(stderr, "Error: I2C init failed.\n");
        CH347CloseDevice(idx);
        return 1;
    }

    /* ---- Parse descriptors and data from argv[2..] ---- */
    for (int ai = 2; ai < argc; ai++) {
        char *arg = argv[ai];

        switch (state) {
        case ST_DESC: {
            if (nmsgs >= MAX_MSGS) {
                fprintf(stderr, "Error: Too many messages (max %d)\n", MAX_MSGS);
                goto err;
            }

            /* Direction */
            int isRead = 0;
            switch (*arg++) {
            case 'r': isRead = 1; break;
            case 'w': break;
            default:
                fprintf(stderr, "Error: Invalid direction '%c' (expected r or w) in '%s'\n",
                        argv[ai][0], arg);
                goto err;
            }

            /* Length */
            unsigned long len = strtoul(arg, &arg, 0);
            if (!len || len > 65535) {
                fprintf(stderr, "Error: Invalid length in '%s'\n", argv[ai]);
                goto err;
            }

            /* Optional @address */
            int msgAddr = -1;
            if (*arg == '@') {
                arg++;
                msgAddr = (int)strtoul(arg, &arg, 0);
                if (msgAddr < 0x03 || msgAddr > 0x77) {
                    fprintf(stderr, "Error: Invalid address in '%s'\n", argv[ai]);
                    goto err;
                }
            } else if (address >= 0) {
                msgAddr = address;
            }

            if (msgAddr < 0) {
                fprintf(stderr, "Error: No address given\n");
                goto err;
            }

            /* Update default address for subsequent messages */
            address = msgAddr;

            msgs[nmsgs].addr = (uint8_t)msgAddr;
            msgs[nmsgs].isRead = isRead;
            msgs[nmsgs].len = len;
            msgs[nmsgs].buf = (uint8_t *)calloc(len, 1);
            if (!msgs[nmsgs].buf) {
                fprintf(stderr, "Error: Out of memory\n");
                goto err;
            }

            if (isRead || !len) {
                /* Read message - no data follows */
                nmsgs++;
            } else {
                /* Write message - collect data tokens next */
                buf_idx = 0;
                state = ST_DATA;
            }
            break;
        }

        case ST_DATA: {
            char *end;
            unsigned long raw = strtoul(arg, &end, 0);
            if (raw > 0xff || arg == end) {
                fprintf(stderr, "Error: Invalid data byte '%s'\n", arg);
                goto err;
            }

            uint8_t val = (uint8_t)raw;
            size_t targetLen = msgs[nmsgs].len;

            while (buf_idx < targetLen) {
                msgs[nmsgs].buf[buf_idx++] = val;
                if (!*end) break;  /* no suffix - one data token consumed */

                switch (*end) {
                case '=': break;                          /* constant */
                case '+': val++; break;                   /* increment */
                case '-': val--; break;                   /* decrement */
                case 'p':                                 /* pseudo-random LFSR */
                    val = (val ^ 27) + 13;
                    val = (val << 1) | (val >> 7);
                    break;
                default:
                    fprintf(stderr, "Error: Unknown suffix '%c' in data '%s'\n", *end, arg);
                    goto err;
                }
            }

            if (buf_idx == targetLen) {
                nmsgs++;
                state = ST_DESC;
            }
            break;
        }
        }
    }

    if (state != ST_DESC || nmsgs == 0) {
        fprintf(stderr, "Error: Incomplete message\n");
        goto err;
    }

    /* ---- Execute messages ---- */
    /* Strategy: process messages sequentially. When a write is immediately
     * followed by a read to the same address, combine them into one
     * CH347StreamI2C call so the hardware generates a repeated-START. */
    int ret = 0;
    for (int i = 0; i < nmsgs && !ret; i++) {
        uint8_t *wbuf = NULL;
        size_t   wlen = 0;

        /* Look ahead: is the next message a read to the same address? */
        if (!msgs[i].isRead &&
            i + 1 < nmsgs &&
            msgs[i + 1].isRead &&
            msgs[i + 1].addr == msgs[i].addr) {
            /* Combine write[i] + read[i+1] into one transfer */
            wbuf = msgs[i].buf;
            wlen = msgs[i].len;
            if (do_transfer(idx, msgs[i].addr, wbuf, wlen,
                            msgs[i + 1].buf, msgs[i + 1].len)) {
                ret = -1;
            } else {
                i++;  /* skip the read message, already done */
            }
        } else {
            /* Single message (write-only or read-only) */
            if (msgs[i].isRead) {
                if (do_transfer(idx, msgs[i].addr, NULL, 0,
                                msgs[i].buf, msgs[i].len))
                    ret = -1;
            } else {
                if (do_transfer(idx, msgs[i].addr, msgs[i].buf, msgs[i].len,
                                NULL, 0))
                    ret = -1;
            }
        }
    }

    /* Print read results */
    for (int i = 0; i < nmsgs; i++) {
        if (!msgs[i].isRead) continue;
        printf("msg %d: read %zu bytes from 0x%02x:",
               i, msgs[i].len, msgs[i].addr);
        for (size_t j = 0; j < msgs[i].len; j++)
            printf(" 0x%02x", msgs[i].buf[j]);
        printf("\n");
    }

    /* Cleanup */
    for (int i = 0; i < nmsgs; i++)
        free(msgs[i].buf);

    CH347CloseDevice(idx);
    return ret ? 1 : 0;

err:
    for (int i = 0; i <= nmsgs; i++)
        free(msgs[i].buf);
    CH347CloseDevice(idx);
    return 1;
}
