#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

//#define IO_BASE 0xFFFFFFFFFFFC1000 // or whatever
#define IO_BASE 0x00000000000C1000
//#define IO_BASE 0xFFFFFFFFFFFFFF00


#define S_MOSI    0x10
#define S_MISO 		0x14
#define S_CLK			0x18
#define S_SS		  0x1C
#define S_MODE		0x20

#define  SPI_MOSI()      (*((volatile char *)(IO_BASE+S_MOSI)))		//0xFFFC1010
#define  SPI_MISO()   (*((volatile char *)(IO_BASE+S_MISO)))	//0xFFFC1014
#define  SPI_CLK()    (*((volatile char *)(IO_BASE+S_CLK)))	//0xFFFC1018
#define  SPI_SS()    (*((volatile char *)(IO_BASE+S_SS)))	//0xFFFC101C
#define  SPI_MODE()    (*((volatile char *)(IO_BASE+S_MODE)))	//0xFFFC101C

#define SPI_MSB_FIRST (0x40)
#define SPI_LSB_FIRST (0x44)

#define SPI_MODE_0 (0x80)
#define SPI_MODE_1 (0x84)
#define SPI_MODE_2 (0x88)
#define SPI_MODE_3 (0x8C)

char inverse(char byte)
{
	char mask=0x01, result=0x00;
	while (mask)
	{
		if (byte & 0x80)
			result |= mask;
		mask <<= 1;
		byte <<= 1;
	}
	return (result);
}

char spi_transfer(char d)
{
	char c=d;
	SPI_SS() = 0x00;
	
	int bit=0;
  for (bit = 0; bit < 8; bit++) 
    {
    // set up MOSI on falling edge of previous SCK (sampled on rising edge)
    //if (mosi_ != NO_PIN)
      //{
      if (c & 0x80)
          SPI_MOSI()=0x01;
      else
          SPI_MOSI()=0x00;
      //}
    // finished with MS bit, get ready to receive next bit      
    c <<= 1;
 
    // read MISO
    //if (miso_ != NO_PIN)
    //c |= SPI_MISO();
 
    // clock high
    SPI_CLK()=0x01;
    
    // delay between rise and fall of clock
    //usleep(1000);
    
    c |= SPI_MISO();
 
    // clock low
    SPI_CLK()=0x00;

    // delay between rise and fall of clock
    //usleep(1000);
    }  // end of for loop, for each bit
  SPI_SS() = 0x01;
  return c;	
	
}

int main(void) { 
 	//printf("\nRx Received:"); 
 	//char c=uart_polled_read();
 //	printf(" %c\n",c);
 	
 	char d = 0x01;
 	char c = spi_transfer(d); 
 	printf("MISO sent: %x\n",c);
 	d = 0x10;
 	c = spi_transfer(d);
 	printf("MISO sent: %x\n",c);
 	c = spi_transfer(d);
 	printf("MISO sent: %x\n",c);
 	
 	
 	return 0; 
 	
 }
