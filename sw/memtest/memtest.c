/*
 * M3 DDR2 memtest — bare-metal MicroBlaze, no BSP. Waits for MIG
 * calibration (GPIO ch2), then tests the full 128 MiB through the
 * uncached data-port AXI. Addresses per hw/bd/static_region.tcl.
 */

#define UART_BASE 0x40600000u
#define GPIO_BASE 0x40000000u
#define DDR_BASE  0x80000000u
#define DDR_BYTES 0x08000000u /* 128 MiB */

#define UART_TX_FIFO (*(volatile unsigned int *)(UART_BASE + 0x4))
#define UART_STATUS  (*(volatile unsigned int *)(UART_BASE + 0x8))
#define UART_TX_FULL 0x8u

#define GPIO_DATA    (*(volatile unsigned int *)(GPIO_BASE + 0x0))
#define GPIO2_DATA   (*(volatile unsigned int *)(GPIO_BASE + 0x8))

static void putch(char c)
{
    while (UART_STATUS & UART_TX_FULL)
        ;
    UART_TX_FIFO = (unsigned int)c;
}

static void puts_uart(const char *s)
{
    while (*s)
        putch(*s++);
}

static void puthex(unsigned int v)
{
    static const char d[] = "0123456789abcdef";
    for (int i = 28; i >= 0; i -= 4)
        putch(d[(v >> i) & 0xF]);
}

int main(void)
{
    volatile unsigned int *ddr = (volatile unsigned int *)DDR_BASE;
    const unsigned int nwords = DDR_BYTES / 4;
    unsigned int errors = 0;

    puts_uart("MEMTEST: waiting for DDR2 calibration\r\n");
    while (!(GPIO2_DATA & 1))
        ;
    puts_uart("MEMTEST: init_calib_complete=1\r\n");

    /* Pass 1: address-in-address, full range */
    puts_uart("MEMTEST: pass1 write addr-in-addr\r\n");
    for (unsigned int i = 0; i < nwords; i++)
        ddr[i] = i;
    puts_uart("MEMTEST: pass1 verify\r\n");
    for (unsigned int i = 0; i < nwords; i++)
        if (ddr[i] != i) {
            if (++errors <= 4) {
                puts_uart("  MISMATCH @");
                puthex(i * 4);
                puts_uart(" got ");
                puthex(ddr[i]);
                puts_uart("\r\n");
            }
        }

    /* Pass 2: inverted pattern, full range */
    puts_uart("MEMTEST: pass2 write ~addr\r\n");
    for (unsigned int i = 0; i < nwords; i++)
        ddr[i] = ~i;
    puts_uart("MEMTEST: pass2 verify\r\n");
    for (unsigned int i = 0; i < nwords; i++)
        if (ddr[i] != ~i)
            errors++;

    /* Pass 3: walking ones at sparse strides */
    puts_uart("MEMTEST: pass3 walking ones\r\n");
    for (unsigned int bit = 0; bit < 32; bit++) {
        unsigned int idx = (bit * 0x400001u) & (nwords - 1);
        ddr[idx] = 1u << bit;
        if (ddr[idx] != (1u << bit))
            errors++;
    }

    if (errors == 0) {
        puts_uart("MEMTEST: PASS errors=0 (128 MiB)\r\n");
        GPIO_DATA = 0xF;
    } else {
        puts_uart("MEMTEST: FAIL errors=");
        puthex(errors);
        puts_uart("\r\n");
        GPIO_DATA = 0x1;
    }

    for (;;)
        ;
    return 0;
}
