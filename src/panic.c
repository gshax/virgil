#include <virgil/common.h>
#include <virgil/uart.h>

void __noreturn panic(char* str)
{
    uart_puts("\n\nunable to continue: ");
    uart_puts(str);
    uart_puts("\n");
    uart_flush_tx();
    for (;;) ;
}

/* required by libgcc's __aeabi_idiv0 (division by zero) */
int __noreturn raise(int signal)
{
	(void)signal;
	panic("raise called (division by zero?)");
}
