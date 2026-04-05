#pragma once

/*
 * receive a file via XMODEM-CRC.
 *
 * sends 'C' to initiate CRC mode and waits indefinitely for the
 * sender to begin. blocks are verified with CRC-16/XMODEM before
 * being written to the destination buffer.
 *
 * returns total bytes received on success, -1 on protocol error.
 *
 * this replaces the bootrom's xmodem_rcv which has a block number
 * wrap bug that silently drops every 256th block.
 */
int xmodem_rcv(void *dest, unsigned long max_size);
