/*
 * QSPI bootloader — lives in the LMB BRAM inside the bitstream.
 * Waits for DDR2 calibration, quad-reads the boot image from flash
 * (slot at 0xD00000, layout from tools/mkbootimg.py):
 *   u32 magic 0x54524942 ("TRIB"), u32 nseg, u32 entry,
 *   then per segment: u32 addr, u32 len, len bytes (4-aligned)
 * scatter-loads every segment (the app's LMB vectors overwrite ours —
 * fine, we are done with them) and jumps to the entry point.
 */

#define GPIO2_CALIB (*(volatile unsigned int *)0x40000008u)

#define SPI_BASE   0x44A20000u
#define SPI_SRR    (*(volatile unsigned int *)(SPI_BASE + 0x40))
#define SPI_CR     (*(volatile unsigned int *)(SPI_BASE + 0x60))
#define SPI_SR     (*(volatile unsigned int *)(SPI_BASE + 0x64))
#define SPI_DTR    (*(volatile unsigned int *)(SPI_BASE + 0x68))
#define SPI_DRR    (*(volatile unsigned int *)(SPI_BASE + 0x6C))
#define SPI_SSR    (*(volatile unsigned int *)(SPI_BASE + 0x70))
#define SPICR_BASE 0x086u
#define SPICR_INH  0x100u
#define SPISR_RXE  0x1u
#define SPISR_TXE  0x4u

#define UART_TX    (*(volatile unsigned int *)0x40600004u)
#define UART_STAT  (*(volatile unsigned int *)0x40600008u)

#define BOOT_SLOT  0x00D00000u
#define BOOT_MAGIC 0x54524942u

typedef unsigned int u32;
typedef unsigned char u8;

static void putch(char c)
{
    while (UART_STAT & 0x8u)
        ;
    UART_TX = (u32)c;
}

static void puts_b(const char *s)
{
    while (*s)
        putch(*s++);
}

static void puthex_b(u32 v)
{
    static const char d[] = "0123456789abcdef";
    for (int i = 28; i >= 0; i -= 4)
        putch(d[(v >> i) & 0xF]);
}

static void spi_xfer(u8 *buf, u32 n)
{
    u32 done = 0, filled = 0;

    SPI_CR = SPICR_BASE | SPICR_INH | 0x60u;
    SPI_SSR = ~1u;
    while (filled < n) {
        u32 burst = n - filled;
        if (burst > 128)
            burst = 128;
        for (u32 i = 0; i < burst; i++)
            SPI_DTR = buf[filled++];
        SPI_CR = SPICR_BASE;
        while (!(SPI_SR & SPISR_TXE))
            ;
        SPI_CR = SPICR_BASE | SPICR_INH;
        for (u32 i = 0; i < burst; i++)
            buf[done++] = (u8)SPI_DRR;
    }
    SPI_SSR = ~0u;
}

/* Quad-output fast read, framing per notebook entry 26: 8 overhead bytes */
static void flash_read(u32 off, u8 *dst, u32 len)
{
    static u8 chunk[8 + 200];

    while (len) {
        u32 n = len > 200 ? 200 : len;
        chunk[0] = 0x6B;
        chunk[1] = (u8)(off >> 16);
        chunk[2] = (u8)(off >> 8);
        chunk[3] = (u8)off;
        chunk[4] = chunk[5] = chunk[6] = chunk[7] = 0x00;
        for (u32 i = 0; i < n; i++)
            chunk[8 + i] = 0xFF;
        spi_xfer(chunk, 8 + n);
        for (u32 i = 0; i < n; i++)
            dst[i] = chunk[8 + i];
        dst += n;
        off += n;
        len -= n;
    }
}

int main(void)
{
    u32 hdr[3];

    puts_b("BOOT: qspi bootloader\r\n");

    while (!(GPIO2_CALIB & 1u))   /* DDR2 must calibrate before we load */
        ;

    SPI_SRR = 0xA;
    for (volatile int i = 0; i < 1000000; i++)
        ;
    SPI_CR = SPICR_BASE | SPICR_INH | 0x60u;
    {   /* throwaway transaction settles the core post-reset */
        u8 j[4] = { 0x9F, 0xFF, 0xFF, 0xFF };
        spi_xfer(j, 4);
    }

    flash_read(BOOT_SLOT, (u8 *)hdr, 12);
    if (hdr[0] != BOOT_MAGIC) {
        puts_b("BOOT: bad magic ");
        puthex_b(hdr[0]);
        puts_b(" ");
        puthex_b(hdr[1]);
        puts_b(" ");
        puthex_b(hdr[2]);
        puts_b(", halting\r\n");
        for (;;)
            ;
    }

    u32 nseg = hdr[1], entry = hdr[2], off = BOOT_SLOT + 12;
    for (u32 s = 0; s < nseg; s++) {
        u32 sh[2];
        flash_read(off, (u8 *)sh, 8);
        off += 8;
        flash_read(off, (u8 *)sh[0], sh[1]);
        off += (sh[1] + 3u) & ~3u;
    }

    puts_b("BOOT: launching app\r\n");
    ((void (*)(void))entry)();
    for (;;)
        ;
    return 0;
}
