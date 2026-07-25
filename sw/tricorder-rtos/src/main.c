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

#define EXT_IIC_BASE  XPAR_AXI_IIC_0_BASEADDR /* JA harness bus */
#define TMP_IIC_BASE  XPAR_AXI_IIC_1_BASEADDR /* on-board ADT7420 */
#define ADT7420_ADDR  0x4B

/* DFX loader plumbing (raw ICAPE2 writer path; addresses fixed in the BD) */
#define DECOUPLE   (*(volatile u32 *)0x40010000u)
#define ICAPW_DATA (*(volatile u32 *)0x40020000u)
#define ICAPW_GO   (*(volatile u32 *)0x40020008u)
#define RM_COUNT   (*(volatile u32 *)0x44A10000u)
#define RM_SIG     (*(volatile u32 *)0x44A10004u)
#define RM_CTRL    (*(volatile u32 *)0x44A10008u)
#define RM_AUX     (*(volatile u32 *)0x44A1000Cu)
/* axi_timer counter 1 (counter 0 drives the FreeRTOS tick) */
#define TMR_CSR1   (*(volatile u32 *)0x41C00010u)
#define TMR_TCR1   (*(volatile u32 *)0x41C00018u)
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
    u8 buf[2];
    int found;

    (void)arg;
    for (;;) {
        /* External bus walk: 1-byte read probe, NAK -> absent */
        found = 0;
        xil_printf("I2C-EXT: scan:");
        for (u8 a = 0x08; a <= 0x77; a++) {
            if (XIic_Recv(EXT_IIC_BASE, a, buf, 1, XIIC_STOP) == 1) {
                xil_printf(" 0x%02x", a);
                found++;
                for (unsigned i = 0; i < sizeof(expected)/sizeof(expected[0]); i++)
                    if (expected[i].addr == a)
                        xil_printf("(%s)", expected[i].name);
            }
        }
        xil_printf(" -> %d device(s)\r\n", found);

        /* On-board ADT7420: 13-bit temperature, reg 0x00/0x01 */
        buf[0] = 0x00;
        if (XIic_Send(TMP_IIC_BASE, ADT7420_ADDR, buf, 1, XIIC_REPEATED_START) == 1 &&
            XIic_Recv(TMP_IIC_BASE, ADT7420_ADDR, buf, 2, XIIC_STOP) == 2) {
            int t16 = ((buf[0] << 8) | buf[1]) >> 3; /* 13-bit two's complement */
            if (t16 & 0x1000)
                t16 -= 0x2000;
            /* 1/16 C per LSB; print as centi-C without floats */
            int centi = t16 * 100 / 16;
            xil_printf("ADT7420: %d.%02d C\r\n", centi / 100,
                       (centi < 0 ? -centi : centi) % 100);
        } else {
            xil_printf("ADT7420: read FAILED\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

__attribute__((optimize("O2"))) static void dfx_task(void *arg)
{
    unsigned idx = 0;

    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(8000));
        const struct rm_desc *rm = &rms[idx];
        idx = (idx + 1) % (sizeof(rms) / sizeof(rms[0]));
        u32 len = *rm->len;
        if (!len || len > 0x200000u) {
            xil_printf("DFX: no partial staged for RM-%s, skipping\r\n", rm->name);
            continue;
        }

        xil_printf("DFX: swapping to RM-%s\r\n", rm->name);
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
        DMA_MM2S_SA = rm->addr;
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
        xil_printf("DFX: RM-%s live in %u us, sig=%08x %s\r\n", rm->name,
                   (unsigned)((t1 - t0) / 100u), sig,
                   sig == rm->sig ? "OK" : "MISMATCH");

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

    xTaskCreate(heartbeat_task, "beat", 512, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(sensors_task, "sens", 1024, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(dfx_task, "dfx", 1024, NULL, tskIDLE_PRIORITY + 3, NULL);

    vTaskStartScheduler();

    /* Only reached on allocation failure */
    xil_printf("RTOS: scheduler failed to start\r\n");
    for (;;)
        ;
    return 0;
}
