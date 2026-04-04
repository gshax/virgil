#include <stdint.h>
#include <virgil/common.h>
#include <virgil/romapi.h>
#include <virgil/hw.h>
#include <virgil/uart.h>
#include <virgil/ddr.h>

void __noreturn panic(char* str)
{
    uart_puts("\n\nPANIC: ");
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

void __entry _start(uint32_t rom_version, const dspg_bootrom_api_t* rom_calls, int bootsel)
{
	/* let the bootrom's "ok" finish draining before we print */
	rom_calls->udelay(25000);

	uart_puts("\nvirgil\n");
    uart_puts(__DATE__ "\n\n");

	if (ddr_init(rom_calls) != 0) {
		uart_puts("failed!\n");
		panic("ddr initialization failed");
	}
	uart_puts("ok!!\n\n");

    uart_puts("hello, world!\n");
	for (;;) ;
}
