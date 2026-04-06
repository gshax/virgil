#pragma once

void uart_init(unsigned int baudrate);
void uart_putc(char c);
void uart_puts(const char* s);
char uart_getc(void);
void uart_putint(int value, int base);
void uart_putdec(int value);
void uart_puthex(int value);
int  uart_tstc(void);
void uart_flush_tx(void);
void uart_exit(void);
unsigned long uart_get_pclk(void);
