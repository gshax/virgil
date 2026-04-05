#pragma once

void uart_init(unsigned int baudrate);
void uart_putc(char c);
void uart_puts(const char* s);
char uart_getc(void);
int  uart_tstc(void);
void uart_flush_tx(void);
