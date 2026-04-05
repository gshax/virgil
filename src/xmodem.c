/*
 * XMODEM-CRC receiver
 *
 * self-contained implementation using the virgil UART driver.
 * replaces the bootrom's xmodem_rcv which has a block number wrap
 * bug (silently drops every 256th block after the first wrap).
 */

#include <stdint.h>
#include <virgil/xmodem.h>
#include <virgil/romapi.h>
#include <virgil/uart.h>

#define SOH     0x01
#define EOT     0x04
#define ACK     0x06
#define NAK     0x15
#define XMODEM_C 'C'

#define BLOCK_SIZE      128
#define INIT_TIMEOUT_MS 3000
#define BYTE_TIMEOUT_MS 1000
#define MAX_ERRORS      10

/* CRC-16/XMODEM: polynomial 0x1021, init 0x0000 */
static uint16_t crc16(const uint8_t *data, int len)
{
	uint16_t crc = 0;
	for (int i = 0; i < len; i++) {
		crc ^= (uint16_t)data[i] << 8;
		for (int j = 0; j < 8; j++)
			crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
	}
	return crc;
}

/* receive a byte with timeout. returns byte (0-255) or -1 on timeout. */
static int uart_getc_timeout(unsigned long timeout_ms)
{
	for (unsigned long i = 0; i < timeout_ms; i++) {
		if (uart_tstc())
			return (uint8_t)uart_getc();
		rom_api->udelay(1000);
	}
	return -1;
}

/* flush any stale data from the UART RX FIFO */
static void uart_flush_rx(void)
{
	while (uart_tstc())
		uart_getc();
}

int xmodem_rcv(void *dest, unsigned long max_size)
{
	uint8_t *dp = dest;
	uint8_t blk[BLOCK_SIZE];
	uint8_t expected = 1;
	unsigned long offset = 0;
	int errors = 0;

	uart_flush_rx();

	/* send 'C' until sender responds with SOH */
	int first_byte;
	for (;;) {
		uart_putc(XMODEM_C);
		first_byte = uart_getc_timeout(INIT_TIMEOUT_MS);
		if (first_byte == SOH)
			break;
		/* timeout or garbage — keep sending 'C' */
	}

	/* main receive loop */
	for (;;) {
		int b;

		/* first_byte is set on entry and after WAIT_NEXT */
		if (first_byte == EOT) {
			uart_putc(ACK);
			return offset;
		}

		if (first_byte != SOH) {
			/* unexpected byte — NAK and retry */
			if (++errors > MAX_ERRORS)
				return -1;
			uart_putc(NAK);
			goto wait_next;
		}

		/* read block#, ~block# */
		int blk_num = uart_getc_timeout(BYTE_TIMEOUT_MS);
		int blk_inv = uart_getc_timeout(BYTE_TIMEOUT_MS);
		if (blk_num < 0 || blk_inv < 0)
			goto timeout_err;

		/* read 128 data bytes */
		for (int i = 0; i < BLOCK_SIZE; i++) {
			b = uart_getc_timeout(BYTE_TIMEOUT_MS);
			if (b < 0)
				goto timeout_err;
			blk[i] = b;
		}

		/* read CRC16 (big-endian) */
		int crc_hi = uart_getc_timeout(BYTE_TIMEOUT_MS);
		int crc_lo = uart_getc_timeout(BYTE_TIMEOUT_MS);
		if (crc_hi < 0 || crc_lo < 0)
			goto timeout_err;

		/* verify complement */
		if ((blk_num ^ blk_inv) != 0xff) {
			if (++errors > MAX_ERRORS)
				return -1;
			uart_putc(NAK);
			goto wait_next;
		}

		/* verify CRC */
		uint16_t recv_crc = (crc_hi << 8) | crc_lo;
		if (crc16(blk, BLOCK_SIZE) != recv_crc) {
			if (++errors > MAX_ERRORS)
				return -1;
			uart_putc(NAK);
			goto wait_next;
		}

		/* check block number */
		if ((uint8_t)blk_num == expected) {
			/* new block — copy to destination */
			if (offset + BLOCK_SIZE <= max_size) {
				for (int i = 0; i < BLOCK_SIZE; i++)
					dp[offset + i] = blk[i];
				offset += BLOCK_SIZE;
			}
			expected++;  /* uint8_t wraps 255→0 naturally */
			errors = 0;
			uart_putc(ACK);
		} else if ((uint8_t)blk_num == (uint8_t)(expected - 1)) {
			/* duplicate of last block — ACK but don't store */
			uart_putc(ACK);
		} else {
			/* out of sequence */
			if (++errors > MAX_ERRORS)
				return -1;
			uart_putc(NAK);
		}

wait_next:
		first_byte = uart_getc_timeout(BYTE_TIMEOUT_MS);
		if (first_byte < 0)
			goto timeout_err;
		continue;

timeout_err:
		if (++errors > MAX_ERRORS)
			return -1;
		uart_flush_rx();
		uart_putc(NAK);
		first_byte = uart_getc_timeout(BYTE_TIMEOUT_MS);
		if (first_byte < 0)
			return -1;
	}
}
