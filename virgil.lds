OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
	virgil :     ORIGIN = 0x08000000, LENGTH = 256k
	ram_dma :       ORIGIN = 0x00200000, LENGTH = 16k
}

SECTIONS
{
	.text.entry : {
		*(.entry)
	} > virgil

	.text : {
	  *(.text)
	} > virgil

	.rodata : { *(.rodata*) } > virgil
	.data :   { *(.data) } > virgil
	.got :    { *(.got) } > virgil
	.glue :   { *(.glue*) } > virgil
	.bss :    { *(.bss) } > virgil

	.dma :
	{
		*(.bss.dma);
	} > ram_dma
}
