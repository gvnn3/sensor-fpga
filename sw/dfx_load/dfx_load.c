/*
 * M5 DFX loader — bare-metal MicroBlaze. Streams partial bitstreams from
 * DDR2 into the AXI HWICAP and measures swap time with the AXI timer.
 *
 * Memory protocol (filled by xsdb before running):
 *   0x81000000  partial A (.bin words)
 *   0x82000000  partial B (.bin words)
 *   0x83000000  partial PDM (.bin words), length 0 = skip M6 phase
 *   0x84000000  partial SONAR (.bin words), length 0 = skip M7 phase
 *   0x80FFFFE8  u32 length of SONAR in bytes
 *   0x80FFFFF0  u32 length of A in bytes
 *   0x80FFFFF4  u32 length of B in bytes
 *   0x80FFFFEC  u32 length of PDM in bytes
 *   0x80FFFFF8  u32 flags: bit0 = byteswap words before ICAP write
 *
 * Sequence: decouple -> load -> undecouple -> read RM signature. Runs
 * A -> B -> A and prints microseconds per swap. Heartbeat prints between
 * swaps prove the static region never glitches.
 */

#define UART_BASE   0x40600000u
#define GPIO_BASE   0x40000000u /* ch1 LEDs, ch2 init_calib */
#define GPIO1_BASE  0x40010000u /* ch1 bit0 = decouple */
#define GPIO2_BASE  0x40020000u /* ch1 = ICAPE2 writer data, ch2 = go toggle */
#define DMA_BASE    0x41E00000u /* axi_dma MM2S -> ICAPE2 AXIS sink */
#define HWICAP_BASE 0x44A00000u
#define TIMER_BASE  0x41C00000u
#define RM_BASE     0x44A10000u

#define UART_TX_FIFO (*(volatile unsigned int *)(UART_BASE + 0x4))
#define UART_STATUS  (*(volatile unsigned int *)(UART_BASE + 0x8))
#define UART_TX_FULL 0x8u

#define ICAP_WF      (*(volatile unsigned int *)(HWICAP_BASE + 0x100))
#define ICAP_CR      (*(volatile unsigned int *)(HWICAP_BASE + 0x10C))
#define ICAP_SR      (*(volatile unsigned int *)(HWICAP_BASE + 0x110))
#define ICAP_WFV     (*(volatile unsigned int *)(HWICAP_BASE + 0x114))
#define ICAP_CR_WRITE 0x1u
#define ICAP_CR_FIFOCLR 0x4u

#define TMR_CSR0     (*(volatile unsigned int *)(TIMER_BASE + 0x0))
#define TMR_TCR0     (*(volatile unsigned int *)(TIMER_BASE + 0x8))

#define GPIO_DATA    (*(volatile unsigned int *)(GPIO_BASE + 0x0))
#define DECOUPLE     (*(volatile unsigned int *)(GPIO1_BASE + 0x0))
#define ICAPW_DATA   (*(volatile unsigned int *)(GPIO2_BASE + 0x0))
#define ICAPW_GO     (*(volatile unsigned int *)(GPIO2_BASE + 0x8))
#define DMA_MM2S_CR  (*(volatile unsigned int *)(DMA_BASE + 0x00))
#define DMA_MM2S_SR  (*(volatile unsigned int *)(DMA_BASE + 0x04))
#define DMA_MM2S_SA  (*(volatile unsigned int *)(DMA_BASE + 0x18))
#define DMA_MM2S_LEN (*(volatile unsigned int *)(DMA_BASE + 0x28))

#define RM_COUNT     (*(volatile unsigned int *)(RM_BASE + 0x0))
#define RM_SIG       (*(volatile unsigned int *)(RM_BASE + 0x4))

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

static void putdec(unsigned int v)
{
    char b[12];
    int i = 0;
    if (!v) {
        putch('0');
        return;
    }
    while (v) {
        b[i++] = '0' + v % 10;
        v /= 10;
    }
    while (i)
        putch(b[--i]);
}

static unsigned int bswap(unsigned int w)
{
    return (w >> 24) | ((w >> 8) & 0xFF00u) | ((w << 8) & 0xFF0000u) | (w << 24);
}

/* returns elapsed timer ticks (100 MHz) for the ICAP streaming alone */
static unsigned int icap_load(const unsigned int *words, unsigned int nwords,
                              int swap)
{
    unsigned int t0, t1;

    TMR_CSR0 = 0x0;        /* stop */
    TMR_CSR0 = 0x20;       /* load 0 */
    TMR_CSR0 = 0x80;       /* run up */
    t0 = TMR_TCR0;

    ICAP_CR = ICAP_CR_FIFOCLR;

    /* Driver-style chunked transfer: fill a chunk, start the write, wait
     * for it to finish. Writing WF while a CR write is active loses the
     * transfer (observed: WFV pinned at 0, CPU spinning). */
    for (unsigned int i = 0; i < nwords; ) {
        unsigned int burst = nwords - i;
        if (burst > 16)
            burst = 16;
        for (unsigned int j = 0; j < burst; j++, i++) {
            while (ICAP_WFV == 0)
                ;
            ICAP_WF = swap ? bswap(words[i]) : words[i];
        }
        ICAP_CR = ICAP_CR_WRITE;
        for (unsigned int spin = 0; ICAP_CR & ICAP_CR_WRITE; spin++) {
            if (spin > 20000000u) {           /* ~1 s: wedged */
                puts_uart("ICAP WEDGE at word ");
                puthex(i);
                puts_uart(" CR=");
                puthex(ICAP_CR);
                puts_uart(" SR=");
                puthex(ICAP_SR);
                puts_uart(" WFV=");
                puthex(ICAP_WFV);
                puts_uart("\r\n");
                return 0;
            }
        }
        if ((i & 0xFFFF) < 16) {              /* every ~64K words */
            puts_uart("  icap ");
            puthex(i);
            puts_uart("/");
            puthex(nwords);
            puts_uart(" SR=");
            puthex(ICAP_SR);
            puts_uart("\r\n");
        }
    }

    t1 = TMR_TCR0;
    return t1 - t0;
}

/* Primary fast path: axi_dma streams the partial from DDR2 into the
 * AXIS->ICAPE2 sink (one word/cycle; hardware does the bit reversal, so
 * the raw .bin bytes in DDR need no software preprocessing). Returns
 * timer ticks, 0 on DMA error/timeout. */
static unsigned int dma_icap_load(unsigned int addr, unsigned int bytes)
{
    unsigned int t0, t1;

    TMR_CSR0 = 0x0;
    TMR_CSR0 = 0x20;
    TMR_CSR0 = 0x80;
    t0 = TMR_TCR0;

    DMA_MM2S_CR = 1;                       /* run */
    DMA_MM2S_SA = addr;
    DMA_MM2S_LEN = bytes;                  /* triggers transfer */
    for (unsigned int spin = 0; !(DMA_MM2S_SR & 0x2u); spin++) {  /* Idle */
        if (DMA_MM2S_SR & 0x70u || spin > 50000000u) {
            puts_uart("DMA: error/timeout SR=");
            puthex(DMA_MM2S_SR);
            puts_uart("\r\n");
            return 0;
        }
    }

    t1 = TMR_TCR0;
    return t1 - t0;
}

/* Fallback: raw ICAPE2 writer in the top wrapper, one word per go-toggle */
static unsigned int gpio_icap_load(const unsigned int *words,
                                   unsigned int nwords, int swap)
{
    unsigned int t0, t1, go = ICAPW_GO & 1u;

    TMR_CSR0 = 0x0;
    TMR_CSR0 = 0x20;
    TMR_CSR0 = 0x80;
    t0 = TMR_TCR0;

    for (unsigned int i = 0; i < nwords; i++) {
        ICAPW_DATA = swap ? bswap(words[i]) : words[i];
        go ^= 1u;
        ICAPW_GO = go;
    }

    t1 = TMR_TCR0;
    return t1 - t0;
}

static int swap_to(const char *name, unsigned int addr, unsigned int bytes,
                   int swap, unsigned int want_sig)
{
    unsigned int ticks;

    puts_uart("DFX: loading RM-");
    puts_uart(name);
    puts_uart(" (");
    putdec(bytes);
    puts_uart(" B)\r\n");

    DECOUPLE = 1;
    ticks = dma_icap_load(addr, bytes);
    if (ticks == 0) {
        puts_uart("DFX: DMA path failed -> GPIO ICAPE2 writer fallback\r\n");
        ticks = gpio_icap_load((const unsigned int *)addr, bytes / 4, swap);
    }
    DECOUPLE = 0;

    puts_uart("DFX: icap stream ");
    putdec(ticks / 100);
    puts_uart(" us; sig=");
    puthex(RM_SIG);
    puts_uart(" count0=");
    puthex(RM_COUNT);
    puts_uart(" count1=");
    puthex(RM_COUNT);
    puts_uart("\r\n");

    if (RM_SIG != want_sig) {
        puts_uart("DFX: SIGNATURE MISMATCH\r\n");
        return 1;
    }
    return 0;
}

static void pdm_probe(void)
{
    int pcm_min = 0x7FFFFFFF, pcm_max = -0x7FFFFFFF;
    unsigned int e_min = 0xFFFFFFFFu, e_max = 0u, w0, w1;

    /* ~2 s of samples; window counter should advance ~74 (27 ms/window) */
    w0 = RM_COUNT & 0xFFFFu;
    for (unsigned int i = 0; i < 200000; i++) {
        int s = (int)*(volatile unsigned int *)(RM_BASE + 0x8);
        unsigned int e = *(volatile unsigned int *)(RM_BASE + 0xC);
        if (s < pcm_min) pcm_min = s;
        if (s > pcm_max) pcm_max = s;
        if (e < e_min) e_min = e;
        if (e > e_max) e_max = e;
    }
    w1 = RM_COUNT & 0xFFFFu;

    puts_uart("PDM: windows ");
    putdec((w1 - w0) & 0xFFFFu);
    puts_uart(" pcm[");
    puthex((unsigned)pcm_min);
    puts_uart(",");
    puthex((unsigned)pcm_max);
    puts_uart("] energy[");
    putdec(e_min);
    puts_uart(",");
    putdec(e_max);
    puts_uart("]\r\n");

    if (pcm_min == pcm_max)
        puts_uart("PDM: FAIL (flatlined PCM - mic dead?)\r\n");
    else
        puts_uart("PDM: PASS (live audio)\r\n");
}

int main(void)
{
    unsigned int len_a = *(volatile unsigned int *)0x80FFFFF0u;
    unsigned int len_b = *(volatile unsigned int *)0x80FFFFF4u;
    unsigned int len_p = *(volatile unsigned int *)0x80FFFFECu;
    unsigned int len_s = *(volatile unsigned int *)0x80FFFFE8u;
    int swap = (int)(*(volatile unsigned int *)0x80FFFFF8u & 1u);
    int errors = 0;

    puts_uart("DFX: loader up; byteswap=");
    putdec((unsigned)swap);
    puts_uart("\r\n");

    /* Let the JTAG session die completely before touching the ICAP:
     * 7-series config arbitration gives JTAG priority, and an active TAP
     * can starve ICAP transfers. ~6 s at 100 MHz. */
    puts_uart("DFX: settling (JTAG quiesce)\r\n");
    for (volatile unsigned int i = 0; i < 150000000u; i++)
        ;
    puts_uart("DFX: initial sig=");
    puthex(RM_SIG);
    puts_uart("\r\n");

    errors += swap_to("B", 0x82000000u, len_b, swap, 0x5B5B5B5Bu);
    puts_uart("DFX: static alive between swaps\r\n");
    errors += swap_to("A", 0x81000000u, len_a, swap, 0xA5A5A5A5u);
    errors += swap_to("B", 0x82000000u, len_b, swap, 0x5B5B5B5Bu);

    if (len_p) {
        errors += swap_to("PDM", 0x83000000u, len_p, swap, 0x9D3A0001u);
        if (!errors)
            pdm_probe();
    }

    if (len_s && !errors) {
        errors += swap_to("SONAR", 0x84000000u, len_s, swap, 0x50A40001u);
        if (!errors) {
            for (int shot = 0; shot < 3; shot++) {
                *(volatile unsigned int *)(RM_BASE + 0x8) = 1; /* fire */
                while (*(volatile unsigned int *)(RM_BASE + 0x8) & 1)
                    ;
                unsigned int pk = *(volatile unsigned int *)(RM_BASE + 0xC);
                puts_uart("SONAR: burst done peak|t=");
                puthex(pk);
                puts_uart("\r\n");
            }
            puts_uart("SONAR: bursts=");
            puthex(RM_COUNT);
            puts_uart(" (echo ranging needs harness)\r\n");
        }
        /* prove PDM<->SONAR round trip */
        if (!errors && len_p)
            errors += swap_to("PDM", 0x83000000u, len_p, swap, 0x9D3A0001u);
    }

    if (errors == 0)
        puts_uart("DFX: PASS (all swaps ok)\r\n");
    else
        puts_uart("DFX: FAIL\r\n");

    GPIO_DATA = errors ? 0x1 : 0xF;
    for (;;)
        ;
    return 0;
}
