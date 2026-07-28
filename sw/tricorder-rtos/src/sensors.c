/*
 * Tricorder sensor layer — spec docs/telemetry-spec.md §3 and §6.
 *
 * Owns both AXI IIC cores, the ADXL362 SPI link and the XADC DRP port.
 * Discovers what is actually on the board, reads what it can, and keeps
 * the blackboard in `sensors` honest about the rest. net_task never
 * calls in here; it reads the blackboard only (spec §6.5), so a wedged
 * I2C bus can never perturb the 1 Hz telemetry cadence.
 *
 * House style: raw MMIO, C99, no dynamic allocation, no BSP drivers.
 * The BSP's XIic_Send/XIic_Recv are deliberately NOT used — their
 * bus-busy and FIFO waits are unbounded `while` loops (see
 * bsp/libsrc/iic/src/xiic_l.c: XIic_DynRecv spins on BUS_BUSY with no
 * iteration cap), which is exactly the hang an absent or shorted device
 * on the JA harness would produce. Every wait here has a hard bound and
 * a SOFTR recovery path (spec §3.2).
 *
 * ---- Deviations from the spec, for the revision log -----------------
 *
 * D1. ADXL362 (#16-#20) and XADC (#21-#24) report ABSENT in this build.
 *     xparameters.h has exactly one SPI core, AXI_QUAD_SPI_0 @0x44a20000
 *     (the QSPI flash that main.c uses for the DFX slots), and no
 *     xadc_wiz/sysmon instance at all — the MIG still owns the XADC
 *     primitive (plan.md §5 deviation 4). This is spec §2.5 option (B):
 *     legal, fixed-shape, honest. Both drivers are written and compiled
 *     in behind XPAR_ guards, so they go LIVE with no source change the
 *     day the block design grows the instances.
 *
 * D2. "Present but no driver" maps to SENS_ERROR. The BME688, AS7341 and
 *     MAX30105 are identified by their ID registers, but turning their
 *     raw output into the engineering units the schema demands needs a
 *     full driver (BME688 burst compensation over ~30 calibration
 *     coefficients; AS7341 SMUX configuration; MAX30105 FIFO/LED slot
 *     setup) that is out of scope here. The frozen emitter helpers in
 *     net.c §6.4 clamp on the SENS_PRESENT path, so the ONLY way to keep
 *     the project owner's fixed requirement — never put a fabricated
 *     in-range number on the wire — is to hold those devices in
 *     SENS_ERROR, which emits -2 / -1000002. Their identity and raw
 *     registers are reported on UART at bring-up instead.
 *
 * D3. Consequently ui_live.i2c_devices counts JA-bus devices that ANSWER
 *     (SENS_PRESENT or SENS_ERROR) rather than SENS_PRESENT alone. That
 *     matches the prose meaning frozen in spec §2.2/§2.3 ("how many
 *     devices answer on the external JA bus", "0 today and 5-6 with the
 *     planned harness"); the strict popcount(mask) of §6.2 would report
 *     3 with a full harness under D2. sens_mask (#14) is unaffected and
 *     stays strictly PRESENT-only per §2.4.
 *
 * D4. The SCD-41 is probed with get_data_ready_status (0xE4B8) + CRC-8
 *     instead of get_serial_number (0x3682). Identical evidence quality
 *     (a valid CRC on a returned word), but 0x3682 NAKs while the sensor
 *     is in periodic measurement mode, which is the state we deliberately
 *     leave it in, so it would false-negative on every probe after the
 *     first. Recorded per the §3.2 note.
 *
 * D5. The 30 s exhaustive 0x08-0x77 walk (§3.5) is split across polls,
 *     4 addresses per poll, rather than run as one 112-probe burst. §3.6
 *     explicitly endorses splitting ("Split it across polls instead");
 *     224 ms in one pass would blow the 20 ms budget outright.
 */

#include "FreeRTOS.h"
#include "task.h"

#include "xparameters.h"
#include "xil_types.h"
#include "xil_printf.h"

#include "sensors.h"
#include "ui.h"

struct sensor_state sensors;

/* ---- AXI IIC register map (PG090; offsets from bsp xiic_l.h) -------- */

#define EXT_IIC_BASE   ((u32)XPAR_AXI_IIC_0_BASEADDR)  /* JA harness bus */
#define TMP_IIC_BASE   ((u32)XPAR_AXI_IIC_1_BASEADDR)  /* on-board ADT7420 */

#define IIC_DGIER      0x01Cu
#define IIC_IISR       0x020u
#define IIC_IIER       0x028u
#define IIC_RESETR     0x040u
#define IIC_CR         0x100u
#define IIC_SR         0x104u
#define IIC_DTR        0x108u
#define IIC_DRR        0x10Cu
#define IIC_RFD        0x120u

#define IIC_REG(base, off)  (*(volatile u32 *)((base) + (off)))

/* The IISR is TOGGLE-on-write (PG090): writing 1 to a clear bit SETS
 * it. A blind `IISR = mask` therefore fabricates TX_ERROR and every
 * transaction reads as an instant NAK. Clearing means writing back
 * exactly the bits that currently read as 1. */
static void i2c_isr_clear(u32 base, u32 mask)
{
    u32 v = (*(volatile u32 *)((base) + 0x020u)) & mask;

    if (v)
        *(volatile u32 *)((base) + 0x020u) = v;
}

#define SOFTR_KEY          0x0000000Au
#define CR_ENABLE          0x00000001u
#define CR_TX_FIFO_RESET   0x00000002u

#define SR_BUS_BUSY        0x00000004u
#define SR_TX_FIFO_FULL    0x00000010u
#define SR_RX_FIFO_EMPTY   0x00000040u
#define SR_TX_FIFO_EMPTY   0x00000080u

#define ISR_ARB_LOST       0x00000001u
#define ISR_TX_ERROR       0x00000002u  /* slave NAK / msg complete      */
#define ISR_TX_EMPTY       0x00000004u
#define ISR_RX_FULL        0x00000008u
#define ISR_BNB            0x00000010u

#define DYN_START          0x00000100u
#define DYN_STOP           0x00000200u
#define OP_WR              0u
#define OP_RD              1u

#define IIC_RX_FIFO_DEPTH  16u          /* hard core limit, PG090        */

/* Bounded spins (spec §3.2). The literal is the spec's; note that each
 * iteration is an AXI read of several cycles, so the true wall-clock
 * ceiling is looser than 2 ms. bus_dead[] below is what actually keeps
 * the poll inside its budget: a wedged bus costs one timeout per pass,
 * not one per transaction. */
#define I2C_XFER_TIMEOUT_US  2000u
#define I2C_SPIN             (I2C_XFER_TIMEOUT_US * 10u) /* ~2-4 ms (F6) */

/* Set when a transaction on that bus timed out during the current poll
 * pass; every later transaction on it fails immediately. Cleared at the
 * top of sensors_poll() and sensors_init(). */
static u8 bus_dead[2];

static u32 bus_ix(u32 base) { return (base == TMP_IIC_BASE) ? 1u : 0u; }

/* SOFTR + re-enable, dynamic mode, interrupts off. */
static void i2c_recover(u32 base)
{
    volatile u32 settle;

    IIC_REG(base, IIC_RESETR) = SOFTR_KEY;
    for (settle = 0; settle < 1000u; settle++)
        ;
    IIC_REG(base, IIC_RFD)   = IIC_RX_FIFO_DEPTH - 1u;
    IIC_REG(base, IIC_CR)    = CR_TX_FIFO_RESET;
    IIC_REG(base, IIC_CR)    = CR_ENABLE;
    IIC_REG(base, IIC_DGIER) = 0u;             /* polled, never IRQ      */
    IIC_REG(base, IIC_IIER)  = 0u;
    i2c_isr_clear(base, 0xFFFu);               /* toggle-clear set bits  */
}

static int i2c_timeout(u32 base)
{
    i2c_recover(base);
    bus_dead[bus_ix(base)] = 1u;
    xil_printf("SENS: i2c bus %u timeout, core reset\r\n",
               (unsigned)bus_ix(base));
    return -1;
}

/* The core issues its own STOP on a NAK; all we owe it is a cleared ISR
 * and proof the bus came back to idle. */
static int i2c_nak(u32 base)
{
    u32 spin = I2C_SPIN;

    /* F9: the queued dynamic-mode stop word is still in the TX FIFO
     * after an address NAK - flush it or it heads the next transfer. */
    IIC_REG(base, IIC_CR) |= CR_TX_FIFO_RESET;
    IIC_REG(base, IIC_CR) &= ~CR_TX_FIFO_RESET;
    i2c_isr_clear(base, ISR_TX_ERROR | ISR_ARB_LOST);
    while (IIC_REG(base, IIC_SR) & SR_BUS_BUSY) {
        if (--spin == 0u)
            return i2c_timeout(base);
    }
    return 0;
}

/* Step 1 of §3.2: never start a transfer on a busy bus. */
static int bus_gate(u32 base)
{
    u32 spin = I2C_SPIN;

    while (IIC_REG(base, IIC_SR) & SR_BUS_BUSY) {
        if (--spin == 0u)
            return i2c_timeout(base);
    }
    return 0;
}

/*
 * One bounded dynamic-mode transaction.
 *   ntx>0, nrx==0 : write ntx bytes, STOP
 *   ntx>0, nrx>0  : write ntx bytes, repeated START, read nrx bytes, STOP
 *   ntx==0,nrx>0  : read nrx bytes, STOP (the address probe)
 * Returns 1 ACK/ok, 0 NAK (device absent), -1 timeout.
 */
static int i2c_xfer(u32 base, u8 addr, const u8 *tx, u32 ntx, u8 *rx, u32 nrx)
{
    u32 spin, i;

    if (nrx > IIC_RX_FIFO_DEPTH || (ntx == 0u && nrx == 0u))
        return -1;
    if (bus_dead[bus_ix(base)])
        return -1;
    if (bus_gate(base) < 0)
        return -1;

    i2c_isr_clear(base, ISR_ARB_LOST | ISR_TX_ERROR | ISR_TX_EMPTY |
                        ISR_RX_FULL | ISR_BNB);

    if (ntx) {
        IIC_REG(base, IIC_DTR) = DYN_START | ((u32)addr << 1) | OP_WR;
        for (i = 0; i < ntx; i++) {
            spin = I2C_SPIN;
            while (IIC_REG(base, IIC_SR) & SR_TX_FIFO_FULL) {
                if (IIC_REG(base, IIC_IISR) & ISR_TX_ERROR)
                    return i2c_nak(base);
                if (--spin == 0u)
                    return i2c_timeout(base);
            }
            IIC_REG(base, IIC_DTR) = (u32)tx[i] |
                ((nrx == 0u && i + 1u == ntx) ? DYN_STOP : 0u);
        }
        /* Drain before the restart so an address NAK surfaces here and
         * not halfway through the read phase. */
        spin = I2C_SPIN;
        while (!(IIC_REG(base, IIC_SR) & SR_TX_FIFO_EMPTY)) {
            if (IIC_REG(base, IIC_IISR) & ISR_TX_ERROR)
                return i2c_nak(base);
            if (--spin == 0u)
                return i2c_timeout(base);
        }
        if (IIC_REG(base, IIC_IISR) & ISR_TX_ERROR)
            return i2c_nak(base);
    }

    if (nrx) {
        IIC_REG(base, IIC_DTR) = DYN_START | ((u32)addr << 1) | OP_RD;
        IIC_REG(base, IIC_DTR) = DYN_STOP | nrx;
        for (i = 0; i < nrx; i++) {
            spin = I2C_SPIN;
            while (IIC_REG(base, IIC_SR) & SR_RX_FIFO_EMPTY) {
                if (IIC_REG(base, IIC_IISR) & ISR_TX_ERROR) {
                    /* PG090: this bit doubles as "slave transmit
                     * complete" - the master's OWN end-of-read NAK
                     * raises it, racing the final byte into the FIFO.
                     * Address NAK = the bit with no data ever landing. */
                    u32 grace = 2000u;

                    while ((IIC_REG(base, IIC_SR) & SR_RX_FIFO_EMPTY) &&
                           --grace)
                        ;
                    if (IIC_REG(base, IIC_SR) & SR_RX_FIFO_EMPTY)
                        return i2c_nak(base);
                    break;               /* byte arrived: completion    */
                }
                if (--spin == 0u)
                    return i2c_timeout(base);
            }
            rx[i] = (u8)IIC_REG(base, IIC_DRR);
        }
    }

    spin = I2C_SPIN;
    while (IIC_REG(base, IIC_SR) & SR_BUS_BUSY) {
        if (--spin == 0u)
            return i2c_timeout(base);
    }
    return 1;
}

/* Thin wrappers with the return conventions of spec §6.2. */

static int i2c_probe(u32 base, u8 addr)          /* 1 ACK, 0 NAK, -1 to */
{
    u8 b;
    return i2c_xfer(base, addr, (const u8 *)0, 0u, &b, 1u);
}

static int i2c_wr(u32 base, u8 addr, const u8 *tx, u32 n)   /* 0 ok, -1 */
{
    return (i2c_xfer(base, addr, tx, n, (u8 *)0, 0u) == 1) ? 0 : -1;
}

static int i2c_rd(u32 base, u8 addr, u8 reg, u8 *rx, u32 n) /* 0 ok, -1 */
{
    return (i2c_xfer(base, addr, &reg, 1u, rx, n) == 1) ? 0 : -1;
}

/* SCCB (OV7670) has no repeated-start read: address phase must be
 * terminated with a STOP before the read is issued. */
static int sccb_rd(u32 base, u8 addr, u8 reg, u8 *rx, u32 n)
{
    if (i2c_wr(base, addr, &reg, 1u) < 0)
        return -1;
    return (i2c_xfer(base, addr, (const u8 *)0, 0u, rx, n) == 1) ? 0 : -1;
}

/* ---- small integer helpers (no libm, no floats) --------------------- */

/* ~us microseconds at 100 MHz. Crude on purpose: used only for the
 * SCD-41's mandatory inter-phase gaps, twice per 5 s at worst. */
static void delay_us(u32 us)
{
    volatile u32 n = us * 25u;

    while (n)
        n--;
}

static u32 isqrt32(u32 v)
{
    u32 rem = 0, root = 0;
    int i;

    for (i = 0; i < 16; i++) {
        root <<= 1;
        rem = (rem << 2) | (v >> 30);
        v <<= 2;
        if (root < rem) {
            rem -= root | 1u;
            root += 2u;
        }
    }
    return root >> 1;
}

/* Sensirion CRC-8: poly 0x31, init 0xFF, no reflection, no final xor. */
static u8 crc8_sensirion(const u8 *d, u32 n)
{
    u8 crc = 0xFF;
    u32 i;
    int b;

    for (i = 0; i < n; i++) {
        crc ^= d[i];
        for (b = 0; b < 8; b++)
            crc = (crc & 0x80u) ? (u8)((crc << 1) ^ 0x31u) : (u8)(crc << 1);
    }
    return crc;
}

static u32 popcount32(u32 v)
{
    u32 n = 0;

    while (v) {
        v &= v - 1u;
        n++;
    }
    return n;
}

/* ---- discovery state machine (spec §3.1) ---------------------------- */

static const char *const sens_name[SENS_COUNT] = {
    "ADT7420", "ADXL362", "XADC", "BME688", "SCD41", "VEML7700",
    "AS7341", "MAX30105", "OV7670", "AD9226", "AD9708", "SIPM",
    "AD8332", "PIEZO", "PDM"
};

static const char *st_name(u32 s)
{
    switch (s) {
    case SENS_ABSENT:  return "ABSENT";
    case SENS_PRESENT: return "PRESENT";
    case SENS_ERROR:   return "ERROR";
    default:           return "UNKNOWN";
    }
}

static u8 probe_fail[SENS_COUNT];   /* consecutive NAKs while in ERROR  */

/* rc: 1 present, 0 absent/NAK, -1 read error or bus timeout. */
static void sens_set(enum sensor_id id, int rc)
{
    u32 old = sensors.st[id];
    u32 nw;

    if (rc > 0) {
        nw = SENS_PRESENT;
        probe_fail[id] = 0u;
    } else if (rc == 0) {
        if (old == SENS_PRESENT) {
            nw = SENS_ERROR;                 /* first miss: hysteresis  */
            probe_fail[id] = 1u;
        } else if (old == SENS_ERROR) {
            probe_fail[id]++;
            nw = (probe_fail[id] >= 2u) ? SENS_ABSENT : SENS_ERROR;
        } else {
            nw = SENS_ABSENT;
        }
    } else {
        /* A bus timeout is not evidence of removal — only demote a
         * device we believed in, and never all the way to ABSENT. */
        nw = (old == SENS_PRESENT || old == SENS_ERROR) ? SENS_ERROR : old;
    }

    if (nw != old) {
        sensors.st[id] = nw;
        xil_printf("SENS: %s %s->%s\r\n", sens_name[id], st_name(old),
                   st_name(nw));
    }
}

/* ---- ADT7420 (on-board, bus 1 @ 0x4B) ------------------------------- */

#define ADT7420_ADDR   0x4Bu
#define ADT7420_ID     0x0Bu

static int rd_adt7420(void)
{
    u8 b[2];
    int t16, centi;

    if (i2c_rd(TMP_IIC_BASE, ADT7420_ADDR, 0x00u, b, 2u) < 0)
        return -1;

    /* 13-bit two's complement, 1/16 C per LSB — same math as the proven
     * path in main.c's original sensors_task. */
    t16 = ((b[0] << 8) | b[1]) >> 3;
    if (t16 & 0x1000)
        t16 -= 0x2000;
    centi = t16 * 100 / 16;

    sensors.adt_temp_cc = (s32)centi;
    ui_live.temp_centi  = centi;          /* the 7-seg UI still wants it */
    return 0;
}

/* ---- VEML7700 (JA bus @ 0x10) --------------------------------------- */

#define VEML_ADDR      0x10u
#define VEML_ALS_CONF  0x00u
#define VEML_ALS       0x04u

/* ALS_CONF = 0x0000 -> gain x1, integration time 100 ms, ALS powered on.
 * Resolution at that setting is 0.0576 lux/count, i.e. lux = raw*36/625.
 * Because we own the configuration, the correction factor is a constant
 * and light_lux needs no runtime gain/IT lookup. */
static int rd_veml7700(void)
{
    u8 b[2];
    u32 raw;

    if (i2c_rd(EXT_IIC_BASE, VEML_ADDR, VEML_ALS, b, 2u) < 0)
        return -1;

    raw = (u32)b[0] | ((u32)b[1] << 8);     /* little-endian on the wire */
    sensors.light_raw = (s32)raw;
    sensors.light_lux = (s32)((raw * 36u) / 625u);
    return 0;
}

/* ---- SCD-41 (JA bus @ 0x62) ----------------------------------------- */

#define SCD_ADDR       0x62u

static int scd_cmd(u16 cmd)
{
    u8 tx[2];

    tx[0] = (u8)(cmd >> 8);
    tx[1] = (u8)cmd;
    return i2c_wr(EXT_IIC_BASE, SCD_ADDR, tx, 2u);
}

/* Command, mandatory gap, then a CRC-checked multi-word read. */
static int scd_read(u16 cmd, u16 *w, u32 nwords)
{
    u8 rx[9];
    u32 i;

    if (nwords > 3u)
        return -1;
    if (scd_cmd(cmd) < 0)
        return -1;
    delay_us(1200u);                     /* datasheet: ~1 ms turnaround */
    if (i2c_xfer(EXT_IIC_BASE, SCD_ADDR, (const u8 *)0, 0u, rx,
                 nwords * 3u) != 1)
        return -1;

    for (i = 0; i < nwords; i++) {
        if (crc8_sensirion(&rx[i * 3u], 2u) != rx[i * 3u + 2u])
            return -1;
        w[i] = (u16)(((u16)rx[i * 3u] << 8) | rx[i * 3u + 1u]);
    }
    return 0;
}

static int scd41_valid = 0;

static int rd_scd41(void)
{
    u16 w[3];
    s32 t, rh;

    if (scd_read(0xE4B8u, w, 1u) < 0)          /* get_data_ready_status */
        return -1;
    if ((w[0] & 0x07FFu) == 0u)
        return scd41_valid ? 0 : -1;           /* F4b: never publish 0s */

    if (scd_read(0xEC05u, w, 3u) < 0)          /* read_measurement      */
        return -1;

    /* co2 = w0 ppm; T = -45 + 175*w1/65535 C; RH = 100*w2/65535 % */
    t  = -4500 + (s32)(((u32)w[1] * 17500u) / 65535u);
    rh = (s32)(((u32)w[2] * 100u) / 65535u);

    sensors.co2_ppm    = (s32)w[0];
    sensors.scd_temp_cc = t;
    sensors.scd_rh_pct  = rh;
    scd41_valid = 1;
    return 0;
}

/* ---- OV7670 SCCB identity (JA bus @ 0x21) — class S ----------------- */

#define OV7670_ADDR    0x21u

static int rd_ov7670_id(void)
{
    u8 pid, ver;

    if (sccb_rd(EXT_IIC_BASE, OV7670_ADDR, 0x0Au, &pid, 1u) < 0)
        return -1;
    if (sccb_rd(EXT_IIC_BASE, OV7670_ADDR, 0x0Bu, &ver, 1u) < 0)
        return -1;
    if (pid != 0x76u)
        return -1;

    sensors.cam_pid = (s32)(((u32)pid << 8) | ver);   /* 0x7673 = 30323 */
    return 0;
}

/* ---- BME688 / AS7341 / MAX30105 — identity only (deviation D2) ------ */

#define BME_ADDR_A     0x76u
#define BME_ADDR_B     0x77u
#define AS7341_ADDR    0x39u
#define MAX30105_ADDR  0x57u

static u8 bme_addr;          /* 0x76, 0x77, or 0 when neither answers   */

/*
 * Confirms the chip ID and pulls the raw T/P/H ADC words so a bench
 * session can see the device breathing on UART. It deliberately does NOT
 * write bme_temp_cc / bme_rh_pct / bme_pres_pa / bme_gas_ohm: without
 * the ~30-coefficient compensation those raw counts are not centi-C, not
 * percent and not pascals, and putting them in a unit-suffixed key would
 * be a fabricated in-range reading. Returns -1 so the caller parks the
 * device in SENS_ERROR and every bme_* key emits its sentinel.
 */
static int rd_bme688(void)
{
    u8 raw[8];
    u32 t_adc, p_adc, h_adc;

    if (bme_addr == 0u)
        return -1;
    /* 0x1F..0x26: press_adc[3], temp_adc[3], hum_adc[2] */
    if (i2c_rd(EXT_IIC_BASE, bme_addr, 0x1Fu, raw, 8u) < 0)
        return -1;

    p_adc = ((u32)raw[0] << 12) | ((u32)raw[1] << 4) | ((u32)raw[2] >> 4);
    t_adc = ((u32)raw[3] << 12) | ((u32)raw[4] << 4) | ((u32)raw[5] >> 4);
    h_adc = ((u32)raw[6] << 8)  |  (u32)raw[7];
    xil_printf("SENS: BME688@%02x raw t=%u p=%u h=%u (no compensation "
               "driver: bme_* stay sentinel)\r\n",
               bme_addr, (unsigned)t_adc, (unsigned)p_adc, (unsigned)h_adc);
    return -1;
}

/* AS7341 spectral channels need SMUX programming before ADATA0..5 mean
 * anything; identity only. Same contract as rd_bme688(). */
static int rd_as7341(void)
{
    u8 id;

    if (i2c_rd(EXT_IIC_BASE, AS7341_ADDR, 0x92u, &id, 1u) < 0)
        return -1;
    if ((id & 0xFCu) != 0x24u)
        return -1;
    xil_printf("SENS: AS7341 id=%02x (no SMUX driver: as_* stay "
               "sentinel)\r\n", id);
    return -1;
}

/* MAX30105 FIFO output needs mode/LED-current/slot configuration first;
 * identity only. Same contract as rd_bme688(). */
static int rd_max30105(void)
{
    u8 id[2];

    if (i2c_rd(EXT_IIC_BASE, MAX30105_ADDR, 0xFEu, id, 2u) < 0)
        return -1;                    /* 0xFE REV_ID, 0xFF PART_ID      */
    if (id[1] != 0x15u)
        return -1;
    xil_printf("SENS: MAX30105 rev=%02x part=%02x (no FIFO driver: "
               "ppg_*/hr_bpm/spo2_pct stay sentinel)\r\n", id[0], id[1]);
    return -1;
}

/* ---- ADXL362 over SPI (on-board; absent in this BD — deviation D1) -- */

#if defined(XPAR_AXI_QUAD_SPI_1_BASEADDR)

#define ACC_SPI_BASE  ((u32)XPAR_AXI_QUAD_SPI_1_BASEADDR)
#define ACC_SRR       (*(volatile u32 *)(ACC_SPI_BASE + 0x40))
#define ACC_CR        (*(volatile u32 *)(ACC_SPI_BASE + 0x60))
#define ACC_SR        (*(volatile u32 *)(ACC_SPI_BASE + 0x64))
#define ACC_DTR       (*(volatile u32 *)(ACC_SPI_BASE + 0x68))
#define ACC_DRR       (*(volatile u32 *)(ACC_SPI_BASE + 0x6C))
#define ACC_SSR       (*(volatile u32 *)(ACC_SPI_BASE + 0x70))
#define ACC_CR_BASE   0x086u          /* MANUAL_SS | MASTER | SPE       */
#define ACC_CR_INH    0x100u
#define ACC_SR_TXE    0x4u
#define ACC_SPI_SPIN  100000u

/* Shift n bytes out of buf, capture n back into buf. Bounded. */
static int acc_xfer(u8 *buf, u32 n)
{
    u32 spin, i;

    ACC_CR  = ACC_CR_BASE | ACC_CR_INH | 0x60u;   /* FIFO resets        */
    ACC_SSR = ~1u;
    for (i = 0; i < n; i++)
        ACC_DTR = buf[i];
    ACC_CR = ACC_CR_BASE;                          /* run               */
    spin = ACC_SPI_SPIN;
    while (!(ACC_SR & ACC_SR_TXE)) {
        if (--spin == 0u) {
            ACC_CR  = ACC_CR_BASE | ACC_CR_INH;
            ACC_SSR = ~0u;
            return -1;
        }
    }
    ACC_CR = ACC_CR_BASE | ACC_CR_INH;
    for (i = 0; i < n; i++)
        buf[i] = (u8)ACC_DRR;
    ACC_SSR = ~0u;
    return 0;
}

static int acc_probe(void)
{
    u8 b[5] = { 0x0B, 0x00, 0xFF, 0xFF, 0xFF };   /* read DEVID_AD..     */
    u8 cfg[3];

    ACC_SRR = 0xA;
    for (volatile int i = 0; i < 10000; i++)
        ;
    ACC_CR = ACC_CR_BASE | ACC_CR_INH | 0x60u;

    if (acc_xfer(b, 5u) < 0)
        return -1;
    /* DEVID_AD 0xAD, DEVID_MST 0x1D, PARTID 0xF2 */
    if (b[2] != 0xADu || b[3] != 0x1Du || b[4] != 0xF2u)
        return 0;

    cfg[0] = 0x0A; cfg[1] = 0x2C; cfg[2] = 0x13;  /* +-2 g, 100 Hz ODR  */
    (void)acc_xfer(cfg, 3u);
    cfg[0] = 0x0A; cfg[1] = 0x2D; cfg[2] = 0x02;  /* measurement mode   */
    (void)acc_xfer(cfg, 3u);
    return 1;
}

static int rd_adxl362(void)
{
    u8 b[10] = { 0x0B, 0x0E, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    s32 x, y, z, t;
    u32 mag2;

    if (acc_xfer(b, 10u) < 0)
        return -1;

    x = (s32)(s16)((u16)b[2] | ((u16)b[3] << 8));
    y = (s32)(s16)((u16)b[4] | ((u16)b[5] << 8));
    z = (s32)(s16)((u16)b[6] | ((u16)b[7] << 8));
    t = (s32)(s16)((u16)b[8] | ((u16)b[9] << 8));

    /* +-2 g range -> 1 mg/LSB; TEMP is 0.065 C/LSB. */
    sensors.acc_x_mg = x;
    sensors.acc_y_mg = y;
    sensors.acc_z_mg = z;
    mag2 = (u32)(x * x) + (u32)(y * y) + (u32)(z * z);
    sensors.acc_mag_mg  = (s32)isqrt32(mag2);
    sensors.acc_temp_cc = (t * 65) / 10;
    return 0;
}

#else  /* no second SPI core in the block design — see deviation D1 */

static int acc_probe(void)   { return 0; }   /* declared absent */

static int rd_adxl362(void)
{
    /* Keep isqrt32() (a spec §6.2 required internal, used only by the
     * acc_mag_mg path above) referenced so it cannot bit-rot while the
     * block design has no second SPI core. */
    (void)isqrt32;
    return -1;
}

#endif

/* ---- XADC DRP (absent in this BD — deviation D1) -------------------- */

#if defined(XPAR_XADC_WIZ_0_BASEADDR)
#define XADC_BASE  ((u32)XPAR_XADC_WIZ_0_BASEADDR)
#elif defined(XPAR_SYSMON_0_BASEADDR)
#define XADC_BASE  ((u32)XPAR_SYSMON_0_BASEADDR)
#endif

#if defined(XADC_BASE)

#define XADC_DRP(reg)  (*(volatile u32 *)(XADC_BASE + 0x200u + ((reg) * 4u)))

static u32 xadc_raw(u32 reg) { return XADC_DRP(reg) & 0xFFF0u; }

static int xadc_probe(void)
{
    /* A live XADC never reads all-zero on VCCINT. */
    return (xadc_raw(0x01u) != 0u) ? 1 : 0;
}

static int rd_xadc(void)
{
    u32 t   = xadc_raw(0x00u) >> 4;
    u32 vi  = xadc_raw(0x01u) >> 4;
    u32 va  = xadc_raw(0x02u) >> 4;
    u32 vb  = xadc_raw(0x06u) >> 4;

    if (vi == 0u)
        return -1;

    /* T(C) = raw*503.975/4096 - 273.15 ; V(mV) = raw*3000/4096 */
    sensors.xadc_temp_cc   = (s32)((t * 50398u) / 4096u) - 27315;
    sensors.xadc_vccint_mv = (s32)((vi * 3000u) / 4096u);
    sensors.xadc_vccaux_mv = (s32)((va * 3000u) / 4096u);
    sensors.xadc_vbram_mv  = (s32)((vb * 3000u) / 4096u);
    return 0;
}

#else  /* MIG still owns the XADC primitive: spec §2.5 option (B) */

static int xadc_probe(void) { return 0; }
static int rd_xadc(void)    { return -1; }

#endif

/* ---- probe table (spec §3.2) ---------------------------------------- */

#define PROBE_SLOTS  8u

/* Slot -> (bus, address, sensor). Slots 6 and 7 are the two BME688
 * address straps and share one sensor id. */
static const struct {
    u8 bus;          /* 0 = JA harness (axi_iic_0), 1 = on-board        */
    u8 addr;
    u8 id;           /* enum sensor_id                                  */
} probe_tab[PROBE_SLOTS] = {
    { 1u, ADT7420_ADDR,  (u8)SENS_ADT7420  },
    { 0u, VEML_ADDR,     (u8)SENS_VEML7700 },
    { 0u, OV7670_ADDR,   (u8)SENS_OV7670   },
    { 0u, AS7341_ADDR,   (u8)SENS_AS7341   },
    { 0u, MAX30105_ADDR, (u8)SENS_MAX30105 },
    { 0u, SCD_ADDR,      (u8)SENS_SCD41    },
    { 0u, BME_ADDR_A,    (u8)SENS_BME688   },
    { 0u, BME_ADDR_B,    (u8)SENS_BME688   },
};

static u32 slot_base(u32 slot)
{
    return probe_tab[slot].bus ? TMP_IIC_BASE : EXT_IIC_BASE;
}

/* ID confirmation for a slot whose address ACKed. 1 match, 0 mismatch,
 * -1 bus failure. Two-stage "ACK then ID" (§3.2): a wrong ID constant
 * fails safe to ABSENT rather than reporting garbage as a reading. */
static int slot_ident(u32 slot)
{
    u32 base = slot_base(slot);
    u8 addr  = probe_tab[slot].addr;
    u8 b[2];
    u16 w;

    switch (slot) {
    case 0:                                     /* ADT7420 id 0x0B      */
        if (i2c_rd(base, addr, ADT7420_ID, b, 1u) < 0)
            return -1;
        return (b[0] == 0xCBu) ? 1 : 0;

    case 1:                                     /* VEML7700: no ID reg  */
        {
            u8 tx[3] = { VEML_ALS_CONF, 0x00u, 0x00u };
            if (i2c_wr(base, addr, tx, 3u) < 0)
                return -1;
        }
        if (i2c_rd(base, addr, VEML_ALS_CONF, b, 2u) < 0)
            return -1;
        return (b[0] == 0x00u && b[1] == 0x00u) ? 1 : 0;

    case 2:                                     /* OV7670 PID/VER       */
        return (rd_ov7670_id() == 0) ? 1 : 0;

    case 3:                                     /* AS7341 ID 0x92       */
        if (i2c_rd(base, addr, 0x92u, b, 1u) < 0)
            return -1;
        return ((b[0] & 0xFCu) == 0x24u) ? 1 : 0;

    case 4:                                     /* MAX30105 PART_ID     */
        if (i2c_rd(base, addr, 0xFFu, b, 1u) < 0)
            return -1;
        return (b[0] == 0x15u) ? 1 : 0;

    case 5:                                     /* SCD-41, deviation D4 */
        if (scd_read(0xE4B8u, &w, 1u) < 0)
            return 0;
        return 1;

    default:                                    /* BME688 CHIP_ID 0xD0  */
        if (i2c_rd(base, addr, 0xD0u, b, 1u) < 0)
            return -1;
        return (b[0] == 0x61u) ? 1 : 0;
    }
}

/* One bounded round-robin probe turn. */
static void probe_slot(u32 slot)
{
    enum sensor_id id = (enum sensor_id)probe_tab[slot].id;
    u32 base = slot_base(slot);
    u8  addr = probe_tab[slot].addr;
    int ack, ident;

    /* The SCD-41 answers a bare 1-byte read with undefined data and can
     * NAK it outright while measuring, so skip the generic ACK probe and
     * let its CRC-checked identity read be the whole test (it returns 0,
     * never -1, on a NAK, so a missing device still lands on ABSENT). */
    ack = (id == SENS_SCD41) ? 1 : i2c_probe(base, addr);
    if (ack < 0) {
        if (id == SENS_BME688 && bme_addr == addr)
            bme_addr = 0u;
        sens_set(id, -1);
        return;
    }
    if (ack == 0) {
        if (id == SENS_BME688) {
            if (bme_addr == addr)
                bme_addr = 0u;
            if (bme_addr != 0u)
                return;              /* the other strap still answers   */
        }
        sens_set(id, 0);
        return;
    }

    ident = slot_ident(slot);
    if (ident < 0) {
        sens_set(id, -1);
        return;
    }
    if (ident == 0) {
        /* Foreign device squatting on the address: ABSENT, not PRESENT. */
        if (id == SENS_BME688) {
            if (bme_addr == addr)
                bme_addr = 0u;
            if (bme_addr != 0u)
                return;
        }
        sens_set(id, 0);
        return;
    }

    if (id == SENS_BME688)
        bme_addr = addr;

    /* SCD-41 must be told to measure; harmless if it already is. */
    if (id == SENS_SCD41 && sensors.st[SENS_SCD41] != SENS_PRESENT) {
        (void)scd_cmd(0x21B1u);                /* start_periodic_meas   */
        delay_us(1200u);
    }
    /* Identity-only devices (D2): promote straight to ERROR so the
     * emitter never sees a PRESENT window with unwritten fields
     * (verifier F4: the old path flapped PRESENT<->ERROR every lap and
     * put fabricated in-range values on the wire for one frame). */
    if (id == SENS_BME688 || id == SENS_AS7341 || id == SENS_MAX30105) {
        sens_set(id, -1);
        return;
    }
    sens_set(id, 1);
}

/* ---- non-probeable front ends (spec §3.3) --------------------------- */

/* Liveness counters; the counters ARE the hysteresis (§3.3). */
static u8  adc_rail_runs;
static u8  sipm_quiet_runs;
static u8  piezo_quiet_runs;

static int declared(enum sensor_id id)
{
    return (((u32)TELE_HARNESS_MASK >> (u32)id) & 1u) != 0u;
}

/* Nothing can promote a device the operator never declared. */
static void set_declared(enum sensor_id id)
{
    sensors.st[id] = declared(id) ? SENS_PRESENT : SENS_ABSENT;
}

/* ---- exhaustive UART-only bus walk (spec §3.5, deviation D5) -------- */

#define WALK_PER_POLL  4u

static u8  walk_addr;          /* 0 = idle, else next address to probe  */
static u8  walk_found;
static u32 i2c_scan_cnt;       /* reserved key #80; UART diagnostic     */

static void walk_step(void)
{
    u32 n;

    if (walk_addr == 0u)
        return;

    for (n = 0; n < WALK_PER_POLL && walk_addr <= 0x77u; n++) {
        if (i2c_probe(EXT_IIC_BASE, walk_addr) == 1) {
            xil_printf("I2C-EXT: 0x%02x ACK\r\n", walk_addr);
            walk_found++;
        }
        walk_addr++;
    }
    if (walk_addr > 0x77u) {
        walk_addr    = 0u;
        i2c_scan_cnt = walk_found;
        xil_printf("I2C-EXT: walk -> %u device(s)\r\n", (unsigned)walk_found);
    }
}

/* ---- mask / derived state ------------------------------------------- */

static u32 compute_mask(void)
{
    u32 m = 0, i;

    for (i = 0; i < (u32)SENS_COUNT; i++)
        if (sensors.st[i] == SENS_PRESENT)
            m |= 1u << i;
    return m;
}

/* Deviation D3: "devices that answer on the JA bus", which is what the
 * i2c key has always meant. PRESENT|ERROR, because a device held in
 * ERROR for want of a value driver is still electrically there. */
static u32 ja_answering(void)
{
    u32 m = 0, i;

    for (i = 0; i < (u32)SENS_COUNT; i++)
        if ((JA_BUS_MASK & (1u << i)) &&
            (sensors.st[i] == SENS_PRESENT || sensors.st[i] == SENS_ERROR))
            m |= 1u << i;
    return popcount32(m);
}

/* ---- public API ------------------------------------------------------ */

void sensors_init(void)
{
    u32 i;

    for (i = 0; i < sizeof(sensors) / sizeof(u32); i++)
        ((volatile u32 *)&sensors)[i] = 0u;
    for (i = 0; i < (u32)SENS_COUNT; i++) {
        sensors.st[i]   = SENS_UNKNOWN;
        probe_fail[i]   = 0u;
    }
    bus_dead[0] = bus_dead[1] = 0u;
    bme_addr = 0u;
    walk_addr = 0u;
    walk_found = 0u;
    adc_rail_runs = sipm_quiet_runs = piezo_quiet_runs = 0u;

    i2c_recover(EXT_IIC_BASE);
    i2c_recover(TMP_IIC_BASE);

    /* On-board, non-I2C. */
    sens_set(SENS_ADXL362, acc_probe());
    sens_set(SENS_XADC,    xadc_probe());
    sensors.st[SENS_PDM] = SENS_PRESENT;   /* ADMP421 is soldered down  */

    /* Analog front ends: operator declaration only (§3.3). */
    set_declared(SENS_AD9226);
    set_declared(SENS_AD9708);
    set_declared(SENS_SIPM);
    set_declared(SENS_AD8332);
    set_declared(SENS_PIEZO);

    /* One full sweep: 8 slots x 2 ms ceiling = 16 ms. */
    for (i = 0; i < PROBE_SLOTS; i++)
        probe_slot(i);

    sensors.next_probe = 0u;
    sensors.probe_s    = 0u;
    sensors.mask       = compute_mask();
    ui_live.i2c_devices = (int)ja_answering();

    xil_printf("SENS: init mask=%08x i2c=%d\r\n", (unsigned)sensors.mask,
               ui_live.i2c_devices);
}

void sensors_poll(void)
{
    static u32 polls;
    u32 old_mask;

    bus_dead[0] = bus_dead[1] = 0u;
    old_mask = sensors.mask;

    /* 1. Refresh every device we believe is present. A failure demotes
     *    it to SENS_ERROR; its next round-robin turn re-probes it. */
    if (sensors.st[SENS_ADT7420] == SENS_PRESENT)
        sens_set(SENS_ADT7420, rd_adt7420() == 0 ? 1 : -1);
    if (sensors.st[SENS_ADXL362] == SENS_PRESENT)
        sens_set(SENS_ADXL362, rd_adxl362() == 0 ? 1 : -1);
    if (sensors.st[SENS_XADC] == SENS_PRESENT)
        sens_set(SENS_XADC, rd_xadc() == 0 ? 1 : -1);
    if (sensors.st[SENS_VEML7700] == SENS_PRESENT)
        sens_set(SENS_VEML7700, rd_veml7700() == 0 ? 1 : -1);
    if (sensors.st[SENS_SCD41] == SENS_PRESENT)
        sens_set(SENS_SCD41, rd_scd41() == 0 ? 1 : -1);
    if (sensors.st[SENS_OV7670] == SENS_PRESENT)
        sens_set(SENS_OV7670, rd_ov7670_id() == 0 ? 1 : -1);
    /* Identity-only devices (D2): the reader always returns -1, so they
     * settle in SENS_ERROR and every one of their keys emits -2/-1000002
     * rather than a raw count wearing an engineering-unit suffix. */
    if (sensors.st[SENS_BME688] == SENS_PRESENT)
        sens_set(SENS_BME688, rd_bme688() == 0 ? 1 : -1);
    if (sensors.st[SENS_AS7341] == SENS_PRESENT)
        sens_set(SENS_AS7341, rd_as7341() == 0 ? 1 : -1);
    if (sensors.st[SENS_MAX30105] == SENS_PRESENT)
        sens_set(SENS_MAX30105, rd_max30105() == 0 ? 1 : -1);

    /* 2. Exactly one bounded probe, round robin (N = 8 s re-probe). */
    probe_slot(sensors.next_probe);
    if (++sensors.next_probe >= PROBE_SLOTS) {
        sensors.next_probe = 0u;
        sensors.probe_s    = 0u;               /* sweep complete        */
    } else {
        sensors.probe_s++;
    }

    /* 3. Exhaustive UART-only walk, started every 30 s, 4 addrs/poll. */
    if ((polls % 30u) == 0u && walk_addr == 0u) {
        walk_addr  = 0x08u;
        walk_found = 0u;
    }
    walk_step();
    polls++;

    /* 4. Republish derived state. */
    sensors.mask = compute_mask();
    ui_live.i2c_devices = (int)ja_answering();
    if (sensors.mask != old_mask)
        xil_printf("SENS: mask %08x\r\n", (unsigned)sensors.mask);
}

/* ---- fabric publishers (class F) ------------------------------------ */

void sensors_publish_pdm(s32 n_ch, s32 spl_db, s32 doa_deg)
{
    sensors.mic_n_cnt  = n_ch;
    sensors.mic_spl_db = spl_db;
    sensors.doa_deg    = doa_deg;
    /* The on-board ADMP421 always counts as channel 0 (§3.3). */
    sensors.st[SENS_PDM] = SENS_PRESENT;
}

void sensors_publish_adc(s32 min_code, s32 max_code, s32 rms_code, s32 rail_cnt)
{
    sensors.adc_min_code = min_code;
    sensors.adc_max_code = max_code;
    sensors.adc_rms_code = rms_code;
    sensors.adc_rail_cnt = rail_cnt;

    /* Floating-input rail signature observed at M7 (plan.md §5): a stuck
     * window pinned at 0x000 or 0xFFF for 3 windows means nothing is
     * driving the converter. */
    if (min_code == max_code && (min_code == 0 || min_code == 4095)) {
        if (adc_rail_runs < 3u)
            adc_rail_runs++;
    } else {
        adc_rail_runs = 0u;
    }
    if (!declared(SENS_AD9226) || adc_rail_runs >= 3u)
        sensors.st[SENS_AD9226] = SENS_ABSENT;
    else
        sensors.st[SENS_AD9226] = SENS_PRESENT;
}

void sensors_publish_sonar(s32 range_mm, s32 tof_us, s32 snr_db, s32 bursts)
{
    sensors.sonar_range_mm   = range_mm;
    sensors.sonar_tof_us     = tof_us;
    sensors.sonar_snr_db     = snr_db;
    sensors.sonar_bursts_cnt = bursts;

    /* An attached transducer always couples ring-down into the RX path;
     * 10 consecutive bursts at or below the noise floor means it is not
     * there. */
    if (snr_db <= 0) {
        if (piezo_quiet_runs < 10u)
            piezo_quiet_runs++;
    } else {
        piezo_quiet_runs = 0u;
    }
    if (!declared(SENS_PIEZO) || piezo_quiet_runs >= 10u)
        sensors.st[SENS_PIEZO] = SENS_ABSENT;
    else
        sensors.st[SENS_PIEZO] = SENS_PRESENT;
}

void sensors_publish_sipm(s32 cps, s32 total)
{
    sensors.sipm_cps     = cps;
    sensors.sipm_tot_cnt = total;

    /* A biased SiPM always shows dark counts. */
    if (cps == 0 && sensors.sipm_thresh_mv > 0) {
        if (sipm_quiet_runs < 30u)
            sipm_quiet_runs++;
    } else {
        sipm_quiet_runs = 0u;
    }
    if (!declared(SENS_SIPM) || sipm_quiet_runs >= 30u)
        sensors.st[SENS_SIPM] = SENS_ABSENT;
    else
        sensors.st[SENS_SIPM] = SENS_PRESENT;
}

void sensors_publish_cam(s32 frames, s32 fps_hz, s32 lum_pct, s32 motion_pct)
{
    sensors.cam_frames_cnt = frames;
    sensors.cam_fps_hz     = fps_hz;
    sensors.cam_lum_pct    = lum_pct;
    sensors.cam_motion_pct = motion_pct;
}

/* ---- commanded state (class C) -------------------------------------- */

void sensors_command_dac(s32 mode, s32 freq_hz, s32 ampl_pct)
{
    sensors.dac_mode     = mode;
    sensors.dac_freq_hz  = freq_hz;
    sensors.dac_ampl_pct = ampl_pct;
}

void sensors_command_afe(s32 gain_db, s32 hilo)
{
    sensors.afe_gain_db = gain_db;
    sensors.afe_hilo    = hilo;
}

void sensors_command_sipm(s32 thresh_mv)
{
    sensors.sipm_thresh_mv = thresh_mv;
}

void sensors_command_adc(s32 rate_ksps)
{
    sensors.adc_rate_ksps = rate_ksps;
}
