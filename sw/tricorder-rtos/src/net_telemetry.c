/*
 * Tricorder telemetry emitter — implements docs/telemetry-spec.md
 * (schema 1, 75 fixed-shape keys). This file contains ONLY
 * net_send_telemetry() and its local helpers; it is linked in place of
 * the 10-key version that lives in net.c today.
 *
 * ---------------------------------------------------------------------
 * INTEGRATOR NOTES — what has to change outside this file
 * ---------------------------------------------------------------------
 * 1. Delete net_send_telemetry() from sw/tricorder-rtos/src/net.c. Its
 *    declaration in net.h is unchanged and still applies; nothing else
 *    in net.c or net.h needs to move.
 *
 * 2. Drop `static` from the seven objects below in net.c so they can be
 *    shared with this translation unit, and add the matching extern
 *    declarations to net.h (or leave them here — they are repeated in
 *    the "shared with net.c" block and must stay byte-compatible):
 *
 *        u8        txf[1536];          /_ TX staging, payload at [42]  _/
 *        u8        our_ip[4];          /_ DHCP lease, 0.0.0.0 = none   _/
 *        const u8  bcast_mac[6];
 *        u16       ip_id;              /_ IPv4 ID, incrementing        _/
 *        u32       tele_seq;           /_ telemetry seq, wraps         _/
 *        void      eth_header(u8 *f, const u8 *dst, u16 ethertype);
 *        u16       inet_csum(const u8 *p, u32 len);
 *        int       emac_send(u32 len);
 *
 *    link_up_cached is NOT exported: this file calls the public
 *    net_link_up() from net.h, which returns exactly that variable.
 *
 * 3. Add sensors.h / sensors.c (spec §6.1, §6.2) and replace the
 *    sensors_task body in main.c with sensors_init() + 1 Hz
 *    sensors_poll(). Until sensors.c exists, `sensors` is all zeroes,
 *    every st[] reads SENS_UNKNOWN, and every sensor key emits its
 *    ABSENT sentinel — a legal, fixed-shape, honest frame.
 *
 *    DEFECT IN THE SPEC'S §6.1 LISTING, must be fixed when sensors.h is
 *    written: it declares both `enum sensor_state` and
 *    `struct sensor_state`. C has ONE tag namespace for struct, union and
 *    enum (C99 6.2.3), so that header cannot compile — gcc rejects it
 *    with "'sensor_state' defined as wrong kind of tag". Rename one of
 *    them (suggest `enum sensor_st` for the state enum, since `struct
 *    sensor_state sensors` is the name the spec uses everywhere else).
 *    This file is unaffected either way: it names `enum sensor_id` and
 *    the SENS_* constants only, never the state enum's tag.
 *
 * 4. Add net_telemetry.c to the Vitis application sources (same folder,
 *    picked up automatically by the default source scan).
 *
 * ---------------------------------------------------------------------
 * DELIBERATE, SPEC-MANDATED CHANGES vs the current 10-key frame
 * ---------------------------------------------------------------------
 * a. The leading `type=telemetry` line is REMOVED. Spec §2 freezes the
 *    payload at exactly 75 lines, §5's worked example has no `type`, and
 *    acceptance gate G1 counts 75 lines per frame. Telemetry is now told
 *    apart from the event frame (§1.7) by `schema=`/`seq=` and by the
 *    event frame's own `type=event` line, which net_send_event() keeps.
 *    This is the only consumer-visible removal.
 * b. `energy` and `peak` move from kv_u to kv_i (spec §2.3.1) so they can
 *    carry -1/-2/-3. Printed text is byte-identical for every real value.
 * c. `temp_cc` now emits -1000002 on an ADT7420 read failure instead of
 *    repeating the last good value (spec §2.3).
 * The 10 legacy keys otherwise keep their names, order, units and
 * meanings exactly (spec §2.2).
 *
 * ---------------------------------------------------------------------
 * SOURCES READ (spec §6.5 concurrency rule)
 * ---------------------------------------------------------------------
 * Word-aligned loads from `sensors` (sensors.h blackboard) and `ui_live`
 * (ui.h), xTaskGetTickCount(), and — only when the capability gate says
 * the loaded personality provides them — the RM registers at
 * 0x44A10008/0x44A1000C. Nothing here calls sensors_poll(), touches an
 * AXI IIC or Quad SPI register, takes a lock, or blocks. The RM reads are
 * performed strictly INSIDE the gate, so a swap in progress
 * (ui_live.cur_rm == -1, decoupler engaged) is never read through. The
 * 1 Hz cadence is therefore structurally independent of I2C and DFX.
 *
 * C99, raw MMIO, no dynamic allocation, no floats, no lwIP.
 */

#include "FreeRTOS.h"
#include "task.h"

#include "xil_types.h"
#include "xil_printf.h"

#include "net.h"
#include "ui.h"
#include "sensors.h"

/* ---- Shared with net.c (see INTEGRATOR NOTES 2) ---------------------- */

extern u8  txf[1536];
extern u8  our_ip[4];
extern const u8 bcast_mac[6];
extern u16 ip_id;
extern u32 tele_seq;

extern void eth_header(u8 *f, const u8 *dst, u16 ethertype);
extern u16  inet_csum(const u8 *p, u32 len);
extern int  emac_send(u32 len);

/* Overflow counter for spec §4.4. The spec spells this
 * net_stats.tele_overflow, but struct net_stats in net.h has no such
 * member; it lives here with external linkage instead. Integrator: if
 * you add `volatile u32 tele_overflow;` to struct net_stats, replace the
 * two references below and delete this definition. Must stay 0 in every
 * soak test — a truncated frame is a bug, not a mode. */
u32 net_tele_overflow;

/* ---- Local constants (spec §7 framing, frozen) ---------------------- */

#define UDP_PORT      5001u

/* RM register window (docs/plan.md §3; same addresses main.c and ui.c
 * use). Read-only here — RM_CTRL bit 0 is the sonar trigger and is
 * written by dfx_task, never by net_task. */
#define RM_CTRL_R     (*(volatile u32 *)0x44A10008u)
#define RM_AUX_R      (*(volatile u32 *)0x44A1000Cu)

/* Sentinel literals (spec §6.3). Guarded so a future sensors.h that
 * defines them too does not collide. */
#ifndef TS_ABSENT_U
#define TS_ABSENT_U   (-1)
#define TS_ERROR_U    (-2)
#define TS_NA_U       (-3)
#define TS_ABSENT_S   (-1000001)
#define TS_ERROR_S    (-1000002)
#define TS_NA_S       (-1000003)
#endif

/* Personality capability bitmap (spec §3.4). */
#ifndef CAP_PDM
#define RM_PDM        0
#define RM_SONAR      1
#define RM_HELLO_B    2
#define RM_HELLO_A    3
#define RM_CAM        4        /* reserved, not yet built */
#define RM_SIPM       5        /* reserved */
#define RM_RF         6        /* reserved */

#define CAP_PDM       0x01u
#define CAP_SONAR     0x02u
#define CAP_ADC       0x04u
#define CAP_DAC       0x08u
#define CAP_CAM       0x10u
#define CAP_SIPM      0x20u
#define CAP_AFE       0x40u
#endif

static const u32 rm_caps[8] = {
    CAP_PDM,                                    /* 0 RM_PDM             */
    CAP_SONAR | CAP_ADC | CAP_DAC | CAP_AFE,    /* 1 RM_SONAR           */
    0u,                                         /* 2 RM_HELLO_B         */
    0u,                                         /* 3 RM_HELLO_A         */
    CAP_CAM,                                    /* 4 RM_CAM  (reserved) */
    CAP_SIPM,                                   /* 5 RM_SIPM (reserved) */
    CAP_ADC | CAP_DAC,                          /* 6 RM_RF   (reserved) */
    0u                                          /* 7 reserved           */
};

/* Gate predicate (spec §3.4). cur_rm == -1 (swap in progress) => no
 * capability, so every fabric key reports N/A while the RP is dark. */
static int cap_live(u32 cap)
{
    int r = ui_live.cur_rm;

    return (r >= 0 && r < 8) ? ((rm_caps[r] & cap) != 0) : 0;
}

/* ---- Formatting helpers (copied verbatim from net.c, spec §6.5) ------
 * Behaviour is byte-identical to net.c's statics of the same names; both
 * copies are static, so there is no linkage conflict and net_send_event()
 * keeps using net.c's. */

static char *fmt_u32(char *p, u32 v)
{
    char tmp[10];
    int n = 0;

    do {
        tmp[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *p++ = tmp[--n];
    return p;
}

static char *fmt_i32(char *p, s32 v)
{
    u32 mag = (u32)v;

    if (v < 0) {
        *p++ = '-';
        mag = (u32)(-(v + 1)) + 1u;        /* INT_MIN-safe negate     */
    }
    return fmt_u32(p, mag);
}

static char *kv_u(char *p, const char *key, u32 v)
{
    while (*key)
        *p++ = *key++;
    *p++ = '=';
    p = fmt_u32(p, v);
    *p++ = '\n';
    return p;
}

static char *kv_i(char *p, const char *key, s32 v)
{
    while (*key)
        *p++ = *key++;
    *p++ = '=';
    p = fmt_i32(p, v);
    *p++ = '\n';
    return p;
}

/* ---- Sentinel selection (spec §6.4) ---------------------------------
 * Clamp before sentinel selection, never after: a sentinel is out of
 * range by construction and clamping would destroy it. clamp_s32() is
 * therefore reached only on the SENS_PRESENT path. */

/* F5: for keys whose valid range floor is > 0, a below-floor value means
 * "no data", not "smallest reading" - map it to the ERROR sentinel
 * rather than promoting it into range (a zero sonar window must not
 * read as a 30 mm target). */
static s32 su_floor(enum sensor_id id, s32 v, s32 lo, s32 hi)
{
    switch (sensors.st[id]) {
    case SENS_PRESENT:
        if (v < lo)
            return TS_ERROR_U;
        return v > hi ? hi : v;
    case SENS_ERROR:   return TS_ERROR_U;
    default:           return TS_ABSENT_U;
    }
}

static s32 fu_floor(u32 cap, enum sensor_id id, s32 v, s32 lo, s32 hi)
{
    if (!cap_live(cap))
        return TS_NA_U;
    if (sensors.st[id] != SENS_PRESENT)
        return sensors.st[id] == SENS_ERROR ? TS_ERROR_U : TS_ABSENT_U;
    if (v < lo)
        return TS_ERROR_U;
    return v > hi ? hi : v;
}

static s32 clamp_s32(s32 v, s32 lo, s32 hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Class S (static-region peripheral), Type U — never N/A. */
static s32 su(enum sensor_id id, s32 v, s32 lo, s32 hi)
{
    switch (sensors.st[id]) {
    case SENS_PRESENT: return clamp_s32(v, lo, hi);
    case SENS_ERROR:   return TS_ERROR_U;
    default:           return TS_ABSENT_U;      /* ABSENT or UNKNOWN */
    }
}

/* Class S, Type S. */
static s32 ss(enum sensor_id id, s32 v, s32 lo, s32 hi)
{
    switch (sensors.st[id]) {
    case SENS_PRESENT: return clamp_s32(v, lo, hi);
    case SENS_ERROR:   return TS_ERROR_S;
    default:           return TS_ABSENT_S;
    }
}

/* Class S, Type U, DERIVED value (spec §2.6: hr_bpm, spo2_pct). The
 * producer may publish a Type-U sentinel itself to mean "device present,
 * but no valid result right now" — device present, finger not on it.
 * Pass such a value through untouched; clamping -2 to the 30..220 range
 * would fabricate a heart rate. Behaviour is identical to su() for every
 * non-sentinel value, so no other key is affected. */
static s32 du(enum sensor_id id, s32 v, s32 lo, s32 hi)
{
    if (sensors.st[id] == SENS_PRESENT && v >= TS_NA_U && v <= TS_ABSENT_U)
        return v;
    return su_floor(id, v, lo, hi);   /* hr floor 30: below = no beat  */
}

/* Class F (fabric measurement) — capability gate first (spec §1.4). */
static s32 fu(u32 cap, enum sensor_id id, s32 v, s32 lo, s32 hi)
{
    if (!cap_live(cap))
        return TS_NA_U;
    return su(id, v, lo, hi);
}

static s32 fs(u32 cap, enum sensor_id id, s32 v, s32 lo, s32 hi)
{
    if (!cap_live(cap))
        return TS_NA_S;
    return ss(id, v, lo, hi);
}

/* Class C (commanded/derived state) — gated, never ABSENT: we always
 * know what we asked the hardware to do. */
static s32 cu(u32 cap, s32 v, s32 lo, s32 hi)
{
    return cap_live(cap) ? clamp_s32(v, lo, hi) : TS_NA_U;
}

static s32 cs(u32 cap, s32 v, s32 lo, s32 hi)
{
    return cap_live(cap) ? clamp_s32(v, lo, hi) : TS_NA_S;
}

/* ---- Compile-time size guards (spec §4.4) ---------------------------
 *
 * Per key: len(name) + 1 ('=') + W + 1 ('\n'), where
 * W = max(width(range_min), width(range_max), width(sentinel)) and the
 * sentinel is 2 chars for Type U ("-1") and 8 for Type S ("-1000001").
 * The §6.4 clamp makes this a hard bound: a printed value is either a
 * sentinel or a number inside its declared range.
 *
 *   Block 0 legacy ten   10 keys:  46 name + 20 sep +  80 val =  146
 *   Block 1 system        5 keys:  28 name + 10 sep +  24 val =   62
 *   Block 2 on-board     12 keys: 124 name + 24 sep +  70 val =  218
 *   Block 3 JA I2C       26 keys: 261 name + 52 sep + 135 val =  448
 *   Block 4 fabric       22 keys: 255 name + 44 sep + 121 val =  420
 *   ------------------------------------------------------------------
 *   Total                75 keys: 714      +150      + 430     = 1294
 *
 * 1294 <= TELE_WORST_PAYLOAD (1296, the spec's declared bound, which is
 * 2 bytes conservative) <= TELE_PAYLOAD_MAX (1400). On the wire:
 * 1294 + 8 (UDP) + 20 (IPv4) + 14 (Ethernet) = 1336 <= 1514, never
 * fragmented, and 42 + 1400 = 1442 <= sizeof txf. */

typedef char net_tele_size_check[
    (TELE_WORST_PAYLOAD <= TELE_PAYLOAD_MAX) &&
    (42 + TELE_PAYLOAD_MAX <= 1536) &&
    (TELE_KEYS == 75) ? 1 : -1];

/* ---- The emitter (spec §6.5) ---------------------------------------- */

void net_send_telemetry(void)
{
    char *p = (char *)&txf[42];            /* payload after headers   */
    u32 payload_len, iplen, udplen, lines, i;
    u16 csum;
    int lu;
    s32 pcm_v = 0, energy_v = 0, peak_v = 0;

    /* RM register reads, strictly inside the capability gate: with no
     * PDM/sonar personality loaded (or a swap in progress) we never touch
     * the RP's AXI window at all, and the values below are ignored
     * because fs()/fu() return the N/A sentinel on the same predicate. */
    if (cap_live(CAP_PDM)) {
        u32 ctrl = RM_CTRL_R;              /* PDM RM_CTRL @ 0x44A10008 */
        u32 aux  = RM_AUX_R;               /* PDM RM_AUX  @ 0x44A1000C */

        pcm_v    = (s32)(s16)(u16)ctrl;    /* 16-bit signed PCM sample */
        energy_v = (s32)(aux & 0x7FFFFFFFu);
    }
    if (cap_live(CAP_SONAR))               /* sonar RM_AUX = peak|time */
        peak_v = (s32)((RM_AUX_R >> 16) & 0xFFFu);   /* same field ui.c
                                                      * val_peak() uses */

    /* ---- Block 0: frozen legacy ten (spec §2.2) -------------------- */
    p = kv_u(p, "seq",     tele_seq);
    p = kv_u(p, "uptime",  (u32)(xTaskGetTickCount() / configTICK_RATE_HZ));
    p = kv_i(p, "temp_cc", ss(SENS_ADT7420, sensors.adt_temp_cc, -4000, 15000));
    p = kv_i(p, "i2c",     (s32)ui_live.i2c_devices);
    p = kv_i(p, "rm",      (s32)ui_live.cur_rm);      /* type E, exempt */
    p = kv_u(p, "swaps",   ui_live.swaps);
    p = kv_u(p, "swap_us", ui_live.last_swap_us);
    p = kv_i(p, "pcm",     fs(CAP_PDM,   SENS_PDM,   pcm_v,   -32768, 32767));
    p = kv_i(p, "energy",  fu(CAP_PDM,   SENS_PDM,   energy_v, 0, 2147483647));
    p = kv_i(p, "peak",    cu(CAP_SONAR, peak_v,   0, 2147483647)); /* F8: RM counter */

    /* ---- Block 1: system, appended (spec §2.4) --------------------- */
    lu = net_link_up();                    /* < 0 => MDIO never came up */
    p = kv_u(p, "schema",    TELE_SCHEMA);
    p = kv_u(p, "ip",        ui_live.ip);
    p = kv_i(p, "link",      lu < 0 ? TS_ERROR_U : (lu ? 1 : 0));
    p = kv_u(p, "sens_mask", sensors.mask);
    p = kv_u(p, "probe_s",   sensors.probe_s);

    /* ---- Block 2: on-board devices (spec §2.5) --------------------- */
    p = kv_i(p, "acc_x_mg",       ss(SENS_ADXL362, sensors.acc_x_mg,   -8000, 8000));
    p = kv_i(p, "acc_y_mg",       ss(SENS_ADXL362, sensors.acc_y_mg,   -8000, 8000));
    p = kv_i(p, "acc_z_mg",       ss(SENS_ADXL362, sensors.acc_z_mg,   -8000, 8000));
    p = kv_i(p, "acc_mag_mg",     su(SENS_ADXL362, sensors.acc_mag_mg,      0, 13857));
    p = kv_i(p, "acc_temp_cc",    ss(SENS_ADXL362, sensors.acc_temp_cc, -4000, 8500));
    p = kv_i(p, "xadc_temp_cc",   ss(SENS_XADC, sensors.xadc_temp_cc,   -4000, 12500));
    p = kv_i(p, "xadc_vccint_mv", su(SENS_XADC, sensors.xadc_vccint_mv,     0, 3000));
    p = kv_i(p, "xadc_vccaux_mv", su(SENS_XADC, sensors.xadc_vccaux_mv,     0, 3000));
    p = kv_i(p, "xadc_vbram_mv",  su(SENS_XADC, sensors.xadc_vbram_mv,      0, 3000));
    p = kv_i(p, "mic_n_cnt",      fu(CAP_PDM, SENS_PDM, sensors.mic_n_cnt,   0, 7));
    p = kv_i(p, "mic_spl_db",     fu(CAP_PDM, SENS_PDM, sensors.mic_spl_db,  0, 130));
    p = kv_i(p, "doa_deg",        fs(CAP_PDM, SENS_PDM, sensors.doa_deg,  -180, 180));

    /* ---- Block 3: JA I2C sensor harness (spec §2.6) ---------------- */
    p = kv_i(p, "bme_temp_cc",  ss(SENS_BME688, sensors.bme_temp_cc, -4000, 8500));
    p = kv_i(p, "bme_rh_pct",   su(SENS_BME688, sensors.bme_rh_pct,      0, 100));
    p = kv_i(p, "bme_pres_pa",  su_floor(SENS_BME688, sensors.bme_pres_pa, 30000, 110000));
    p = kv_i(p, "bme_gas_ohm",  su_floor(SENS_BME688, sensors.bme_gas_ohm, 10, 50000000));
    p = kv_i(p, "co2_ppm",      su(SENS_SCD41,  sensors.co2_ppm,         0, 40000));
    p = kv_i(p, "scd_temp_cc",  ss(SENS_SCD41,  sensors.scd_temp_cc, -1000, 6000));
    p = kv_i(p, "scd_rh_pct",   su(SENS_SCD41,  sensors.scd_rh_pct,      0, 100));
    p = kv_i(p, "light_lux",    su(SENS_VEML7700, sensors.light_lux,     0, 120000));
    p = kv_i(p, "light_raw",    su(SENS_VEML7700, sensors.light_raw,     0, 65535));
    p = kv_i(p, "as_415_cnt",   su(SENS_AS7341, sensors.as_cnt[0],       0, 65535));
    p = kv_i(p, "as_445_cnt",   su(SENS_AS7341, sensors.as_cnt[1],       0, 65535));
    p = kv_i(p, "as_480_cnt",   su(SENS_AS7341, sensors.as_cnt[2],       0, 65535));
    p = kv_i(p, "as_515_cnt",   su(SENS_AS7341, sensors.as_cnt[3],       0, 65535));
    p = kv_i(p, "as_555_cnt",   su(SENS_AS7341, sensors.as_cnt[4],       0, 65535));
    p = kv_i(p, "as_590_cnt",   su(SENS_AS7341, sensors.as_cnt[5],       0, 65535));
    p = kv_i(p, "as_630_cnt",   su(SENS_AS7341, sensors.as_cnt[6],       0, 65535));
    p = kv_i(p, "as_680_cnt",   su(SENS_AS7341, sensors.as_cnt[7],       0, 65535));
    p = kv_i(p, "as_clear_cnt", su(SENS_AS7341, sensors.as_cnt[8],       0, 65535));
    p = kv_i(p, "as_nir_cnt",   su(SENS_AS7341, sensors.as_cnt[9],       0, 65535));
    p = kv_i(p, "as_flicker_hz",su(SENS_AS7341, sensors.as_flicker_hz,   0, 120));
    p = kv_i(p, "ppg_red_cnt",  su(SENS_MAX30105, sensors.ppg_red_cnt,   0, 262143));
    p = kv_i(p, "ppg_ir_cnt",   su(SENS_MAX30105, sensors.ppg_ir_cnt,    0, 262143));
    p = kv_i(p, "ppg_grn_cnt",  su(SENS_MAX30105, sensors.ppg_grn_cnt,   0, 262143));
    p = kv_i(p, "hr_bpm",       du(SENS_MAX30105, sensors.hr_bpm, 30, 220)); /* du passes -2 through */
    p = kv_i(p, "spo2_pct",     du(SENS_MAX30105, sensors.spo2_pct,     70, 100));
    p = kv_i(p, "max_temp_cc",  ss(SENS_MAX30105, sensors.max_temp_cc,-4000, 8500));

    /* ---- Block 4: fabric / DFX-provided instruments (spec §2.7) ----
     * Class C keys report the commanded value whenever the personality
     * that owns them is loaded, wired or not; class F keys report -1 when
     * the personality is loaded but the front end is declared absent, and
     * -3 when the wrong personality is loaded. cam_pid is class S: the
     * camera's identity comes over the static-region SCCB bus, so it is
     * never N/A. */
    p = kv_i(p, "adc_rate_ksps", cu(CAP_ADC, sensors.adc_rate_ksps,   0, 40000));
    p = kv_i(p, "adc_min_code",  fu(CAP_ADC, SENS_AD9226, sensors.adc_min_code, 0, 4095));
    p = kv_i(p, "adc_max_code",  fu(CAP_ADC, SENS_AD9226, sensors.adc_max_code, 0, 4095));
    p = kv_i(p, "adc_rms_code",  fu(CAP_ADC, SENS_AD9226, sensors.adc_rms_code, 0, 4095));
    p = kv_i(p, "adc_rail_cnt",  cu(CAP_ADC, sensors.adc_rail_cnt, 0, 2147483647)); /* F8 */
    p = kv_i(p, "dac_mode",      cu(CAP_DAC, sensors.dac_mode,        0, 3));
    p = kv_i(p, "dac_freq_hz",   cu(CAP_DAC, sensors.dac_freq_hz,     0, 20000000));
    p = kv_i(p, "dac_ampl_pct",  cu(CAP_DAC, sensors.dac_ampl_pct,    0, 100));
    p = kv_i(p, "cam_pid",       su(SENS_OV7670, sensors.cam_pid,     0, 65535));
    p = kv_i(p, "cam_frames_cnt",fu(CAP_CAM, SENS_OV7670, sensors.cam_frames_cnt, 0, 2147483647));
    p = kv_i(p, "cam_fps_hz",    fu(CAP_CAM, SENS_OV7670, sensors.cam_fps_hz,     0, 30));
    p = kv_i(p, "cam_lum_pct",   fu(CAP_CAM, SENS_OV7670, sensors.cam_lum_pct,    0, 100));
    p = kv_i(p, "cam_motion_pct",fu(CAP_CAM, SENS_OV7670, sensors.cam_motion_pct, 0, 100));
    p = kv_i(p, "sipm_cps",      fu(CAP_SIPM, SENS_SIPM, sensors.sipm_cps,     0, 5000000));
    p = kv_i(p, "sipm_tot_cnt",  fu(CAP_SIPM, SENS_SIPM, sensors.sipm_tot_cnt, 0, 2147483647));
    p = kv_i(p, "sipm_thresh_mv",cu(CAP_SIPM, sensors.sipm_thresh_mv, 0, 3300));
    p = kv_i(p, "afe_gain_db",   cs(CAP_AFE, sensors.afe_gain_db,    -5, 48));
    p = kv_i(p, "afe_hilo",      cu(CAP_AFE, sensors.afe_hilo,        0, 1));
    p = kv_i(p, "sonar_range_mm",   fu_floor(CAP_SONAR, SENS_PIEZO, sensors.sonar_range_mm, 30, 4000));
    p = kv_i(p, "sonar_tof_us",     fu(CAP_SONAR, SENS_PIEZO, sensors.sonar_tof_us,     0, 30000));
    p = kv_i(p, "sonar_snr_db",     fs(CAP_SONAR, SENS_PIEZO, sensors.sonar_snr_db,   -20, 60));
    p = kv_i(p, "sonar_bursts_cnt", cu(CAP_SONAR, sensors.sonar_bursts_cnt, 0, 2147483647)); /* F8 */

    payload_len = (u32)(p - (char *)&txf[42]);

    /* ---- Runtime guards (spec §4.4, gate G1/G2) -------------------- */
    if (payload_len > TELE_PAYLOAD_MAX) {
        /* Truncate at the last complete line; never emit a partial one. */
        while (payload_len && txf[42 + payload_len - 1] != '\n')
            payload_len--;
        net_tele_overflow++;
        xil_printf("NET: telemetry overflow, truncated to %u\r\n",
                   (unsigned)payload_len);
    }

    lines = 0;
    for (i = 0; i < payload_len; i++)
        if (txf[42 + i] == '\n')
            lines++;
    if (lines != TELE_KEYS)                /* fixed shape is the contract */
        xil_printf("NET: telemetry SHAPE ERROR %u keys (want %u)\r\n",
                   (unsigned)lines, (unsigned)TELE_KEYS);

    /* ---- Framing: identical to net.c's (docs/ethernet-spec.md §7.3) - */
    iplen  = 28 + payload_len;
    udplen = 8 + payload_len;

    eth_header(txf, bcast_mac, 0x0800u);
    txf[14] = 0x45;                        /* Ver/IHL                 */
    txf[15] = 0x00;                        /* TOS                     */
    txf[16] = (u8)(iplen >> 8);            /* total length            */
    txf[17] = (u8)iplen;
    txf[18] = (u8)(ip_id >> 8);            /* ID                      */
    txf[19] = (u8)ip_id;
    ip_id++;
    txf[20] = 0x00; txf[21] = 0x00;        /* flags/frag              */
    txf[22] = 0x40;                        /* TTL 64                  */
    txf[23] = 0x11;                        /* proto UDP               */
    txf[24] = 0x00; txf[25] = 0x00;        /* csum placeholder        */
    for (int j = 0; j < 4; j++) {
        txf[26 + j] = our_ip[j];           /* src IP                  */
        txf[30 + j] = 0xFF;                /* dst 255.255.255.255     */
    }
    csum = inet_csum(&txf[14], 20);
    txf[24] = (u8)(csum >> 8);
    txf[25] = (u8)csum;
    txf[34] = (u8)(UDP_PORT >> 8);         /* src port 5001           */
    txf[35] = (u8)UDP_PORT;
    txf[36] = (u8)(UDP_PORT >> 8);         /* dst port 5001           */
    txf[37] = (u8)UDP_PORT;
    txf[38] = (u8)(udplen >> 8);           /* UDP length              */
    txf[39] = (u8)udplen;
    txf[40] = 0x00; txf[41] = 0x00;        /* UDP csum 0 (legal, v1)  */

    (void)emac_send(14 + iplen);

    if (tele_seq % 60u == 0)               /* E4, every 60th packet   */
        xil_printf("NET: telemetry seq=%u keys=%u len=%u\r\n",
                   (unsigned)tele_seq, (unsigned)lines,
                   (unsigned)payload_len);
    tele_seq++;
}
