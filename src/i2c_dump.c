#include <windows.h>
#include "CH347DLL.H"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
static ULONG g_uIndex = 0;
/* Read a single byte from one register address.
 * Uses separate write+read transactions (byte-data access) so it works
 * with both standard I2C and SMBus devices that don't support block reads. */
static BOOL i2c_read_byte(uint8_t addr, uint8_t reg, uint8_t *val) {
  uint8_t out[2] = {0};
  out[0] = addr << 1;
  out[1] = reg;
  return CH347StreamI2C(g_uIndex, 2, out, 1, val);
}

void i2c_dump(uint8_t addr, uint8_t *dat) {
  for (int i = 0x00; i < 0xff; i++) {
    if (!i2c_read_byte(addr, (uint8_t)i, &dat[i])) {
      dat[i] = 0xFF;  /* NACK -> show as -- */
    }
  }
}

void i2c_dump_output(uint8_t *dat) {
  char str[17] = {0};
  printf("   ");
  for (int j = 0; j < 0x10; ++j) {
    printf(" %2x", j);
  }
  printf(" |0123456789abcdef|");
  
  for (int i = 0; i < 0xff; i += 0x10) {
    printf("\n%02x:", i);
    hex2str(&dat[i], str, 0x10);
    for (int j = 0; j < 0x10; ++j) {
      printf(" %02x", dat[i + j]);
    }
    printf(" |%s|", str);
  }
}

const char * help = "Usage: i2cdump I2CBUS ADDRESS\n I2CBUS is an integer\n ADDRESS is an hex integer (0x03 - 0x77)\n";

int main(int argc, char *argv[]) 
{
  uint8_t addr;
  if (argc != 1)
  {
    g_uIndex = atoi(argv[1]);
    uint32_t input_hex;
    sscanf(argv[2], "%x", &input_hex);
    if(input_hex < 0x03 || input_hex > 0x77) {
      printf("%s",help);
      return 0;
    } else {
      addr = input_hex;
    }
  }
  printf("current using CH347 index : %ld\n", g_uIndex);

  if (!CH347OpenDevice(g_uIndex))
  {
    printf("Can't open device.");
    return 1;
  }

  // 00=低速/20KHz,01=标准/100KHz(默认值),10=快速/400KHz,11=高速/750KHz
  if (!CH347I2C_Set(g_uIndex, 1))
  {
    return 1;
  }

  uint8_t dat[512] = {0};
  i2c_dump(addr, dat);
  i2c_dump_output(dat);
  CH347CloseDevice(g_uIndex);
  return 0;
}
