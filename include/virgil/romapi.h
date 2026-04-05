#pragma once

#include <stddef.h>

typedef struct dspg_dvf101_bootrom_api {
	int	(*bootdev_read)(void* dest, unsigned long offset, unsigned long size);
	unsigned long (*crc32)(void* buffer, unsigned long size);
	void (*clkchg_pre)(int clk, unsigned long freq);
	void (*clkchg_post)(int clk, unsigned long freq);
	int (*xmodem_rcv)(void* dest, unsigned long max_size, unsigned long baud_detect);
	void (*udelay)(unsigned long usec);

	/* sboot_check_hdr
	 *
	 * run integrity check on an image header
	 *
	 * return:
	 * 0 = integrity verified
	 * !0 = error (look at bootrom.h for error codes)
	 */
	unsigned long (*sboot_check_hdr)(void* hdr);

	/* sboot_check_img
	 *
	 * check that image sha256 equals expected_sha
	 *
	 * return:
	 * 0 = equals
	 * !0 = not equals
	 */
	unsigned long (*sboot_check_img)(
		unsigned char* img, size_t imglen, unsigned char expected_sha[32]
	);
	/* sha256
	 *
	 * digest <= sha256[message]
	 */
	void (*sha256)(
		const unsigned char* message, unsigned int len, unsigned char *digest
	);

	/* expmod - modular exponentiation
	 *
	 * (cb) = (mb)^(el) mod (nb)
	 *
	 * mlen = mb size in bytes;
	 * nlen = nb size in bytes;
	 *
	 * return:
	 * 0 = success
	 * !0 = error
	 */
	int (*expmod)(
		unsigned char cb[256], unsigned char* mb, unsigned int mlen,
		unsigned long el, unsigned char* nb, unsigned int nlen
	);

	/* is_dev_secured
	 *
	 * return:
	 * 0 = not secured
	 * 1 = secured
	 * -1 = error reading OTP
	 *
	 */
	int (*is_dev_secured)(void);
} dspg_dvf101_bootrom_api_t;
