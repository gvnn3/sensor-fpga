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

int main(void)
{
    XGpio_Initialize(&gpio, XPAR_AXI_GPIO_0_BASEADDR);
    XGpio_SetDataDirection(&gpio, 1, 0x0);

    xil_printf("RTOS: FreeRTOS on MicroBlaze — tricorder M4\r\n");

    xTaskCreate(heartbeat_task, "beat", 512, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(sensors_task, "sens", 1024, NULL, tskIDLE_PRIORITY + 2, NULL);

    vTaskStartScheduler();

    /* Only reached on allocation failure */
    xil_printf("RTOS: scheduler failed to start\r\n");
    for (;;)
        ;
    return 0;
}
