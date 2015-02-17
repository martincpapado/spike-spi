#include <stdio.h>
#include <stdint.h>
#include "systemc"

// transmit byte serially, MSB first
void send_8bit_serial_data(unsigned char data)
{
   int i;
 
   // select device
   output_high(SD_CS);
 
   // send bits 7..0
   for (i = 0; i < 8; i++)
   {
       // consider leftmost bit
       // set line high if bit is 1, low if bit is 0
       if (data & 0x80)
           output_high(SD_DI);
       else
           output_low(SD_DI);
 
       // pulse clock to indicate that bit value should be read
       output_low(SD_CLK);
       output_high(SD_CLK);
 
       // shift byte left so next bit will be leftmost
       data <<= 1;
   }
 
   // deselect device
   output_low(SD_CS);
}

 int main(void) { printf("Hello world!\n"); return 0; }
