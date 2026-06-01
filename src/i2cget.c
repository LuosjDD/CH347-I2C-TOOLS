/*
 * i2cget.c - Read an I2C register via CH347 on Windows
 *
 * Port of Linux i2c-tools i2cget using the WCH CH347 DLL.
 *
 * Usage:
 *   i2cget.exe DEVICE_INDEX ADDRESS [REG] [MODE]
 *
 * MODE (optional):
 *   b  read byte data (default)
 *   w  read word data (little-endian, 2 bytes)
 *   c  write byte / read byte (SMBus protocol)
 *   i  read I2C block data (default 32 bytes)
 */

#include <windows.h>
#include "CH347DLL.H"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define BLOCK_MAX 32

static void print_help(void)
{
    fprintf(stderr,
        "Usage: i2cget [-y] DEVICE_INDEX ADDRESS [REG] [MODE]\n"
        "\n"
        "  OPTIONS:\n"
        "    -y  force access without confirmation\n"
        "\n"
        "  ADDRESS is a hex integer (0x03 - 0x77)\n"
        "  REG     is an optional register address (hex, e.g. 0x10)\n"
        "  MODE    is one of:\n"
        "    b  read byte data (default when REG is given)\n"
        "    w  read word data (2 bytes, little-endian)\n"
        "    c  write byte / read byte (SMBus protocol)\n"
        "    i  read I2C block data (default %d bytes)\n", BLOCK_MAX);

    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "  i2cget.exe -y 0 0x4f 0x10 b\n");
    fprintf(stderr, "    Read byte at register 0x10 from device 0x4f\n");
    fprintf(stderr, "  i2cget.exe -y 0 0x50 0x00 w\n");
    fprintf(stderr, "    Read word (2 bytes) at register 0x00 from EEPROM 0x50\n");
    fprintf(stderr, "  i2cget.exe -y 0 0x4f 0x60 i\n");
    fprintf(stderr, "    Read block data starting at register 0x60 from device 0x4f\n");
}

/* Write reg address then read n bytes in one CH347StreamI2C call (repeated START). */
static int read_bytes(ULONG idx, uint8_t addr, uint8_t reg,
                      uint8_t *buf, size_t n)
{
    uint8_t out[2] = {0};
    out[0] = addr << 1;
    out[1] = reg;
    if (!CH347StreamI2C(idx, 2, out, (ULONG)n, buf))
        return -1;
    return 0;
}

/* Read n bytes without writing a register address first. */
static int read_bytes_no_reg(ULONG idx, uint8_t addr,
                             uint8_t *buf, size_t n)
{
    uint8_t out[2] = { addr << 1, 0 };
    if (!CH347StreamI2C(idx, 2, out, (ULONG)n, buf))
        return -1;
    return 0;
}

int main(int argc, char *argv[])
{
    ULONG idx = 0;
    int yes = 0;
    uint8_t addr = 0, reg = 0;
    char mode = 'b';       /* default: byte data */
    size_t blockLen = BLOCK_MAX;

    if (argc < 3) {
        print_help();
        return 1;
    }

    /* Parse optional flags */
    int ai = 1;
    while (ai < argc && argv[ai][0] == '-') {
        switch (argv[ai][1]) {
        case 'y': yes = 1; break;
        default:
            fprintf(stderr, "Error: Unknown option '-%c'\n", argv[ai][1]);
            print_help();
            return 1;
        }
        ai++;
    }

    if (argc < ai + 2) {
        print_help();
        return 1;
    }

    /* Device index */
    idx = (ULONG)atoi(argv[ai]);

    /* Chip address */
    uint32_t addrVal;
    sscanf(argv[ai + 1], "%x", &addrVal);
    if (addrVal < 0x03 || addrVal > 0x77) {
        fprintf(stderr, "Error: Invalid chip address '%s' (must be 0x03-0x77)\n", argv[ai + 1]);
        return 1;
    }
    addr = (uint8_t)addrVal;

    /* Optional register address */
    int hasReg = 0;
    if (argc > ai + 2) {
        uint32_t regVal;
        sscanf(argv[ai + 2], "%x", &regVal);
        if (regVal > 0xFF) {
            fprintf(stderr, "Error: Invalid register address '%s'\n", argv[ai + 2]);
            return 1;
        }
        reg = (uint8_t)regVal;
        hasReg = 1;
    }

    /* Optional mode */
    if (argc > ai + 3) {
        const char *mStr = argv[ai + 3];
        switch (mStr[0]) {
        case 'b': mode = 'b'; break;
        case 'w': mode = 'w'; break;
        case 'c': mode = 'c'; break;
        case 'i': mode = 'i'; break;
        default:
            fprintf(stderr, "Error: Invalid mode '%s'\n", mStr);
            print_help();
            return 1;
        }
    }

    /* Open device */
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

    /* Execute read based on mode */
    uint8_t buf[BLOCK_MAX + 1] = {0};
    int ret = 0;

    switch (mode) {
    case 'b': { /* Byte data: write reg, read 1 byte */
        if (!hasReg) {
            fprintf(stderr, "Error: Mode 'b' requires a register address\n");
            ret = -1;
        } else if (read_bytes(idx, addr, reg, buf, 1)) {
            fprintf(stderr, "Error: Read failed\n");
            ret = -1;
        } else {
            printf("0x%02x\n", buf[0]);
        }
        break;
    }

    case 'w': { /* Word data: write reg, read 2 bytes (little-endian) */
        if (!hasReg) {
            fprintf(stderr, "Error: Mode 'w' requires a register address\n");
            ret = -1;
        } else if (read_bytes(idx, addr, reg, buf, 2)) {
            fprintf(stderr, "Error: Read failed\n");
            ret = -1;
        } else {
            uint16_t word = buf[0] | (buf[1] << 8);
            printf("0x%04x\n", word);
        }
        break;
    }

    case 'c': { /* Write byte / read byte: send reg, then read without reg */
        if (!hasReg) {
            fprintf(stderr, "Error: Mode 'c' requires a register address (write byte)\n");
            ret = -1;
        } else {
            /* Step 1: write the byte (reg value) to device */
            uint8_t out[2] = { addr << 1, reg };
            if (!CH347StreamI2C(idx, 2, out, 0, NULL)) {
                fprintf(stderr, "Warning - write failed\n");
            }
            /* Step 2: read one byte from device */
            if (read_bytes_no_reg(idx, addr, buf, 1)) {
                fprintf(stderr, "Error: Read failed\n");
                ret = -1;
            } else {
                printf("0x%02x\n", buf[0]);
            }
        }
        break;
    }

    case 'i': { /* I2C block data */
        if (!hasReg) {
            fprintf(stderr, "Error: Mode 'i' requires a register address\n");
            ret = -1;
        } else if (read_bytes(idx, addr, reg, buf, blockLen)) {
            fprintf(stderr, "Error: Read failed\n");
            ret = -1;
        } else {
            for (size_t j = 0; j < blockLen - 1; j++)
                printf("0x%02x ", buf[j]);
            printf("0x%02x\n", buf[blockLen - 1]);
        }
        break;
    }
    }

    CH347CloseDevice(idx);
    return ret ? 1 : 0;
}
