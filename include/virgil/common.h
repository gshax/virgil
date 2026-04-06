#pragma once

#define _STRINGIZE(x) #x
#define STRINGIZE(x) _STRINGIZE(x)

#define __entry __attribute__((section(".entry")))
#define __noreturn __attribute__((noreturn))

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define IO_ADDRESS(a) ((void*)a)

#define writel(v, a)  (*(volatile unsigned long *)(a) = (v))
#define writew(v, a)  (*(volatile unsigned short *)(a) = (v))
#define readl(a)      (*(volatile unsigned long *)(a))
#define readw(a)      (*(volatile unsigned short *)(a))
#define readb(a)      (*(volatile unsigned char *)(a))

#define waitfor(x)       while (!(x))

extern void __noreturn panic(char* str);
