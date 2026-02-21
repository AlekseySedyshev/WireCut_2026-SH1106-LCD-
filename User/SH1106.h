#ifndef SH1106_h
#define SH1106_h

#include "main.h"

void CMD(unsigned char byte);
void lcd_init(void); // INIT LCD T191
void lcd_gotoxy(unsigned char x, unsigned char y);
void lcd_mode(unsigned char mode);
void lcd_char(unsigned char flash_o, unsigned char mode); // Print Symbol
void lcd_printStr(char *s, unsigned char mode);           // Print String
void lcd_clear(void);                                     // Clear LCD
void lcd_clearStr(unsigned char y, unsigned char qnt);
void lcd_printDec(long val, unsigned char mode);            // Print Dec
void lcd_printHex(long val, unsigned char mode);            // Print Hex
void lcd_printBin(unsigned char value, unsigned char mode); // Print Bin
void lcd_puts_UTF8(unsigned char *s);
void lcd_wire_center(uint8_t yy, uint8_t wire_mode); // yy = 0...7, x = 0..127
/*
void I2C_write(uint8_t byte_send);
void I2C_Start(uint8_t addr);
void I2C_Stop(void);

void Flip_mode(unsigned char flip);
void slide_right(unsigned char start_page, unsigned char end_page, unsigned char interval);
void slide_left(unsigned char start_page, unsigned char end_page, unsigned char interval);
void stop_slide(void);
void start_slide(void);
void scroll_vright(unsigned char start_page, unsigned char end_page, unsigned char interval, unsigned char v_offset);
void scroll_vleft(unsigned char start_page, unsigned char end_page, unsigned char interval, unsigned char v_offset);
void scroll_area(unsigned char start_page, unsigned char end_page);
*/
#endif
