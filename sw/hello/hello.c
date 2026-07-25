/*
 * M2 hello — bare-metal MicroBlaze, no BSP. Prints over AXI UARTlite and
 * walks the LEDs. Addresses fixed by hw/bd/static_region.tcl.
 */

#define UART_BASE 0x40600000u
#define GPIO_BASE 0x40000000u

#define UART_TX_FIFO (*(volatile unsigned int *)(UART_BASE + 0x4))
#define UART_STATUS  (*(volatile unsigned int *)(UART_BASE + 0x8))
#define UART_TX_FULL 0x8u

#define GPIO_DATA    (*(volatile unsigned int *)(GPIO_BASE + 0x0))

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

int main(void)
{
    unsigned int n = 0;

    for (;;) {
        puts_uart("hello tricorder\r\n");
        GPIO_DATA = n++ & 0xF;
        for (volatile unsigned int i = 0; i < 2000000; i++)
            ;
    }
    return 0;
}
