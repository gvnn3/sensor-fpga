/*
 * M4: FreeRTOS boot + I2C sensor scan on the Nexys A7 tricorder.
 *
 * Tasks:
 *   heartbeat — LED walk + 1 Hz tick print (proves scheduler)
 *   sensors   — scans the external JA I2C bus (axi_iic_0) for the five
 *               tricorder sensors, reads the on-board ADT7420 (axi_iic_1,
 *               always present) once a second as ground truth.
 */

#include "FreeRTOS.h"
#include "task.h"

#include "xparameters.h"
#include "xiic_l.h"
#include "xil_printf.h"
#include "xgpio.h"
#include "ui.h"
#include "net.h"
#include "sensors.h"
#include "slots.h"

#define EXT_IIC_BASE  XPAR_AXI_IIC_0_BASEADDR /* JA harness bus */
#define TMP_IIC_BASE  XPAR_AXI_IIC_1_BASEADDR /* on-board ADT7420 */
#define ADT7420_ADDR  0x4B

/* DFX loader plumbing (raw ICAPE2 writer path; addresses fixed in the BD) */
#define DECOUPLE   (*(volatile u32 *)0x40010000u)
#define ICAPW_DATA (*(volatile u32 *)0x40020000u)
#define ICAPW_GO   (*(volatile u32 *)0x40020008u)
#define UI_AUDIO   (*(volatile u32 *)0x40050000u) /* bit0 scan, bit1 alert */
#define RM_COUNT   (*(volatile u32 *)0x44A10000u)
#define RM_SIG     (*(volatile u32 *)0x44A10004u)
#define RM_CTRL    (*(volatile u32 *)0x44A10008u)
#define RM_AUX     (*(volatile u32 *)0x44A1000Cu)
/* axi_timer counter 1 (counter 0 drives the FreeRTOS tick) */
#define TMR_CSR1   (*(volatile u32 *)0x41C00010u)
#define TMR_TCR1   (*(volatile u32 *)0x41C00018u)
/* axi_quad_spi (command mode) — flash slot reads */
#define SPI_BASE   0x44A20000u
#define SPI_SRR    (*(volatile u32 *)(SPI_BASE + 0x40))
#define SPI_CR     (*(volatile u32 *)(SPI_BASE + 0x60))
#define SPI_SR     (*(volatile u32 *)(SPI_BASE + 0x64))
#define SPI_DTR    (*(volatile u32 *)(SPI_BASE + 0x68))
#define SPI_DRR    (*(volatile u32 *)(SPI_BASE + 0x6C))
#define SPI_SSR    (*(volatile u32 *)(SPI_BASE + 0x70))
#define SPICR_BASE 0x086u  /* MANUAL_SS | MASTER | SPE */
#define SPICR_INH  0x100u
#define SPISR_RXE  0x1u
#define SPISR_TXE  0x4u
#define FLASH_BOUNCE 0x85000000u

/* axi_quad_spi (command mode) — flash slot reads */
#define SPI_BASE   0x44A20000u
#define SPI_SRR    (*(volatile u32 *)(SPI_BASE + 0x40))
#define SPI_CR     (*(volatile u32 *)(SPI_BASE + 0x60))
#define SPI_SR     (*(volatile u32 *)(SPI_BASE + 0x64))
#define SPI_DTR    (*(volatile u32 *)(SPI_BASE + 0x68))
#define SPI_DRR    (*(volatile u32 *)(SPI_BASE + 0x6C))
#define SPI_SSR    (*(volatile u32 *)(SPI_BASE + 0x70))
#define SPICR_BASE 0x086u  /* MANUAL_SS | MASTER | SPE */
#define SPICR_INH  0x100u
#define SPISR_RXE  0x1u
#define SPISR_TXE  0x4u
#define FLASH_BOUNCE 0x85000000u

/* axi_dma MM2S -> AXIS ICAPE2 sink */
#define DMA_MM2S_CR  (*(volatile u32 *)0x41E00000u)
#define DMA_MM2S_SR  (*(volatile u32 *)0x41E00004u)
#define DMA_MM2S_SA  (*(volatile u32 *)0x41E00018u)
#define DMA_MM2S_LEN (*(volatile u32 *)0x41E00028u)

/* Partial-bitstream staging protocol (filled by xsdb before boot) */
struct rm_desc {
    const char *name;
    u32 addr;
    volatile u32 *len;
    u32 sig;
};
static const struct rm_desc rms[] = {
    { "PDM",   0x83000000u, (volatile u32 *)0x80FFFFECu, 0x9D3A0001u },
    { "SONAR", 0x84000000u, (volatile u32 *)0x80FFFFE8u, 0x50A40001u },
    { "B",     0x82000000u, (volatile u32 *)0x80FFFFF4u, 0x5B5B5B5Bu },
    { "A",     0x81000000u, (volatile u32 *)0x80FFFFF0u, 0xA5A5A5A5u },
};

/* QSPI slot manifest (generated with the flash image): DMA reads the
 * partial straight out of the XIP window — no DDR staging at all. */
const char *ui_rm_name(int id)
{
    return (id >= 0 && id < 4) ? rms[id].name : "NONE";
}

static const struct { u32 addr, len; } flash_slots[] = {
    { QSPI_XIP_BASE + SLOT_PDM_OFF,   SLOT_PDM_LEN },
    { QSPI_XIP_BASE + SLOT_SONAR_OFF, SLOT_SONAR_LEN },
    { QSPI_XIP_BASE + SLOT_B_OFF,     SLOT_B_LEN },
    { QSPI_XIP_BASE + SLOT_A_OFF,     SLOT_A_LEN },
};
static int flash_ok = 1;

__attribute__((optimize("O2"))) static void spi_init(void)
{
    SPI_SRR = 0xA;
    for (volatile int i = 0; i < 1000000; i++) /* let the soft reset settle */
        ;
    SPI_CR = SPICR_BASE | SPICR_INH | 0x60u;   /* fifo resets, inhibited */
}

/* One SPI transaction: shift n bytes out of buf, capture n back into buf.
 * Manual slave select held low across the exchange; per 16-byte chunk:
 * fill TX, run until TX empty, drain RX, inhibit. */
__attribute__((optimize("O2"))) static void spi_xfer(u8 *buf, u32 n)
{
    u32 done = 0, filled = 0;

    SPI_CR = SPICR_BASE | SPICR_INH | 0x60u;
    SPI_SSR = ~1u;                           /* assert SS0 */
    while (filled < n) {
        u32 burst = n - filled;
        if (burst > 128)                     /* FIFO is 256 deep */
            burst = 128;
        for (u32 i = 0; i < burst; i++)
            SPI_DTR = buf[filled++];
        SPI_CR = SPICR_BASE;                 /* run */
        while (!(SPI_SR & SPISR_TXE))        /* all bits shifted */
            ;
        SPI_CR = SPICR_BASE | SPICR_INH;
        /* RX now holds exactly `burst` bytes: drain without per-byte
         * status polls (halves the AXI traffic per byte) */
        for (u32 i = 0; i < burst; i++)
            buf[done++] = (u8)SPI_DRR;
    }
    SPI_SSR = ~0u;                           /* deassert */
}

__attribute__((optimize("O2"))) static void spi_flash_read(u32 off, u8 *dst, u32 len)
{
    static u8 chunk[8 + 200];

    while (len) {
        u32 n = len > 200 ? 200 : len;
        /* 0x6B quad-output read. Overhead is 8 RX bytes: cmd + 3 addr
         * (single-lane, 1:1) + 8 dummy cycles at quad = 4 bytes. TX must
         * supply matching clock entries. (Measured on hardware: data
         * arrived shifted +3 vs the naive 5-byte framing.) */
        chunk[0] = 0x6B;
        chunk[1] = (u8)(off >> 16);
        chunk[2] = (u8)(off >> 8);
        chunk[3] = (u8)off;
        chunk[4] = 0x00;
        chunk[5] = 0x00;
        chunk[6] = 0x00;
        chunk[7] = 0x00;
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
 /* falls back to DDR staging on first bad sig */


__attribute__((always_inline)) static inline u32 bswap32(u32 w)
{
    return (w >> 24) | ((w >> 8) & 0xFF00u) | ((w << 8) & 0xFF0000u) | (w << 24);
}

static XGpio gpio;

static const struct { u8 addr; const char *name; } expected[] = {
    { 0x10, "VEML7700" },
    { 0x39, "AS7341" },
    { 0x57, "MAX30105" },
    { 0x62, "SCD-41" },
    { 0x76, "BME688" },
    { 0x77, "BME688(alt)" },
};

static void heartbeat_task(void *arg)
{
    u32 n = 0;

    (void)arg;
    for (;;) {
        XGpio_DiscreteWrite(&gpio, 1, n & 0xF);
        if ((n & 3) == 0)
            xil_printf("RTOS: tick %lu\r\n", (unsigned long)(n >> 2));
        n++;
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

static void sensors_task(void *arg)
{
    (void)arg;
    sensors_init();
    for (;;) {
        sensors_poll();
        /* feed the display/telemetry blackboard from the sensor layer */
        if (sensors.st[SENS_ADT7420] == SENS_PRESENT)
            ui_live.temp_centi = sensors.adt_temp_cc;
        {
            int n = 0;
            static const enum sensor_id ja[5] = {
                SENS_BME688, SENS_SCD41, SENS_VEML7700,
                SENS_AS7341, SENS_MAX30105
            };
            for (int i = 0; i < 5; i++)
                if (sensors.st[ja[i]] == SENS_PRESENT ||
                    sensors.st[ja[i]] == SENS_ERROR)
                    n++;
            ui_live.i2c_devices = n;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

__attribute__((optimize("O2"))) static void dfx_task(void *arg)
{
    unsigned sonar_tick = 0;

    (void)arg;
    ui_live.req_rm = -1;
    ui_live.cur_rm = 3;              /* full config A boots with Hello-A */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(250));

        /* Sonar personality selected: fire a ranging burst every second */
        if (ui_live.cur_rm == 1 && ++sonar_tick >= 4) {
            sonar_tick = 0;
            *(volatile u32 *)0x44A10008u = 1;
        }

        int req = ui_live.req_rm;
        if (req < 0 || req == ui_live.cur_rm)
            continue;

        /* Switch-driven personality change (SW0-SW3 via the ui task) */
        const unsigned slot = (unsigned)req;
        const struct rm_desc *rm = &rms[slot];
        u32 src, len;
        const int prev_rm = ui_live.cur_rm;
        ui_live.cur_rm = -1;
        if (flash_ok) {
            src = FLASH_BOUNCE;
            len = flash_slots[slot].len;
        } else {
            src = rm->addr;
            len = *rm->len;
        }
        if (!len || len > 0x200000u) {
            xil_printf("DFX: no partial staged for RM-%s, skipping\r\n", rm->name);
            continue;
        }

        xil_printf("DFX: swapping to RM-%s from %s\r\n", rm->name,
                   flash_ok ? "FLASH" : "DDR");
        u32 t_spi = 0;
        if (flash_ok) {
            TMR_CSR1 = 0x0; TMR_CSR1 = 0x20; TMR_CSR1 = 0x80;
            u32 s0 = TMR_TCR1;
            spi_flash_read(flash_slots[slot].addr - QSPI_XIP_BASE,
                           (u8 *)FLASH_BOUNCE, len);
            t_spi = TMR_TCR1 - s0;
        }
        /* Critical section: tick interrupts add ~40x to the GPIO pacing
         * loop. Ticks freeze during the stream, so time it with the
         * timer's second counter (100 MHz, counter 0 owns the tick). */
        TMR_CSR1 = 0x0;
        TMR_CSR1 = 0x20;   /* load 0 */
        TMR_CSR1 = 0x80;   /* run up */
        u32 t0 = TMR_TCR1;
        DECOUPLE = 1;
        /* DMA streams the partial into the AXIS->ICAPE2 sink; the fabric
         * does byteswap+bitrev, so no CPU word pacing (and no critical
         * section) is needed. GPIO writer path remains in hardware as a
         * fallback (sw/dfx_load keeps that code path). */
        DMA_MM2S_CR = 1;
        DMA_MM2S_SA = src;
        DMA_MM2S_LEN = len;
        {
            u32 spin = 0;
            while (!(DMA_MM2S_SR & 0x2u)) {
                if (DMA_MM2S_SR & 0x70u || ++spin > 100000000u) {
                    xil_printf("DMA: error SR=%08x\r\n", DMA_MM2S_SR);
                    break;
                }
            }
        }
        DECOUPLE = 0;
        u32 t1 = TMR_TCR1;

        u32 sig = RM_SIG;
        if (sig == rm->sig) {
            ui_live.swaps++;
            ui_live.last_swap_us = (t1 - t0) / 100u;
            ui_live.cur_rm = req;
        }
        /* Publish the swap record for net_task (attempted swaps too:
         * a mismatch is exactly what telemetry should carry). */
        ui_live.dfx_from = prev_rm;
        ui_live.dfx_to   = req;
        ui_live.dfx_from_len = (prev_rm >= 0)
            ? (flash_ok ? flash_slots[prev_rm].len : *rms[prev_rm].len)
            : 0u;
        ui_live.dfx_to_len = len;
        ui_live.dfx_us = (t1 - t0) / 100u;
        ui_live.dfx_ok = (sig == rm->sig);
        ui_live.dfx_seq++;
        xil_printf("DFX: RM-%s live in %u us (flash read %u us), sig=%08x %s\r\n",
                   rm->name, (unsigned)((t1 - t0) / 100u),
                   (unsigned)(t_spi / 100u), sig,
                   sig == rm->sig ? "OK" : "MISMATCH");
        if (sig != rm->sig) {
            taskENTER_CRITICAL();
            UI_AUDIO = UI_AUDIO ^ 2u;      /* alert: bad personality load */
            taskEXIT_CRITICAL();
        }
        if (sig != rm->sig && flash_ok) {
            xil_printf("DFX: flash source failed - falling back to DDR staging\r\n");
            flash_ok = 0;
        }

        if (sig == 0x9D3A0001u) {
            vTaskDelay(pdMS_TO_TICKS(200));
            xil_printf("  PDM: pcm=%08x energy=%u\r\n", RM_CTRL, RM_AUX);
        } else if (sig == 0x50A40001u) {
            RM_CTRL = 1;
            while (RM_CTRL & 1)
                ;
            xil_printf("  SONAR: peak|t=%08x\r\n", RM_AUX);
        }
    }
}

int main(void)
{
    XGpio_Initialize(&gpio, XPAR_AXI_GPIO_0_BASEADDR);
    XGpio_SetDataDirection(&gpio, 1, 0x0);

    xil_printf("RTOS: FreeRTOS on MicroBlaze — tricorder M4\r\n");
    *(volatile u32 *)0x40050000u = 1;   /* boot chirp: play the scan sound */

    {   /* SPI liveness: JEDEC ID (S25FL128S = 01 20 18) */
        u8 j[4] = { 0x9F, 0xFF, 0xFF, 0xFF };
        spi_init();
        spi_xfer(j, 4);            /* throwaway: first xfer post-reset */
        j[0] = 0x9F; j[1] = j[2] = j[3] = 0xFF;
        spi_xfer(j, 4);
        xil_printf("QSPI: JEDEC %02x %02x %02x\r\n", j[1], j[2], j[3]);

        /* Quad reads need the S25FL128S QE bit (CR1 bit 1, non-volatile).
         * Set it once; skip if already set. */
        u8 c[2] = { 0x35, 0xFF };  /* RDCR */
        spi_xfer(c, 2);
        if (!(c[1] & 0x02)) {
            u8 wen[1] = { 0x06 };  /* WREN */
            u8 wrr[3] = { 0x01, 0x00, 0x02 }; /* WRR: SR1=0, CR1=QE */
            u8 rd[2];
            spi_xfer(wen, 1);
            spi_xfer(wrr, 3);
            do {                   /* poll WIP */
                rd[0] = 0x05; rd[1] = 0xFF;
                spi_xfer(rd, 2);
            } while (rd[1] & 0x01);
            c[0] = 0x35; c[1] = 0xFF;
            spi_xfer(c, 2);
            xil_printf("QSPI: QE set, CR1=%02x\r\n", c[1]);
        } else {
            xil_printf("QSPI: QE already set (CR1=%02x)\r\n", c[1]);
        }
    }

    xTaskCreate(heartbeat_task, "beat", 512, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(sensors_task, "sens", 1024, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(dfx_task, "dfx", 1024, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(ui_task, "ui", 1024, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(net_task, "net", 1024, NULL, tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();

    /* Only reached on allocation failure */
    xil_printf("RTOS: scheduler failed to start\r\n");
    for (;;)
        ;
    return 0;
}
