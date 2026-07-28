/*
 * Tricorder sensor blackboard — spec docs/telemetry-spec.md §6.
 *
 * Latest-value blackboard, no snapshot semantics. Every field is a
 * naturally aligned 32-bit word written by exactly one task and read by
 * net_task; MicroBlaze aligned word loads/stores are atomic, so no lock
 * is required and no field can tear. Fields may be skewed relative to
 * each other by up to one poll period (1 s). That is accepted.
 *
 * Writers:
 *   sensors_task  — st[], mask, probe_s, next_probe, every I2C/SPI/XADC
 *                   value field (via sensors_init / sensors_poll)
 *   dfx_task/acq  — the fabric fields, via sensors_publish_* /
 *                   sensors_command_* only
 *   net_task      — nothing. Reads only (spec §6.5).
 */
#ifndef SENSORS_H
#define SENSORS_H

#include "xil_types.h"

#define TELE_SCHEMA        1
#define TELE_KEYS          75
#define TELE_PAYLOAD_MAX   1400
#define TELE_WORST_PAYLOAD 1296

/* Compile-time guard, spec §4.4. C99: no _Static_assert required. */
typedef char tele_size_check[(TELE_WORST_PAYLOAD <= TELE_PAYLOAD_MAX) ? 1 : -1];

/* ---- Sentinel literals (spec §1.4 / §6.3) ---------------------------
 * Type U (non-negative quantity): the naive -1/-2/-3 are out of range by
 * construction. Type S (range straddles zero): offset by -1,000,000 so
 * they cannot collide with a real reading — temp_cc=-1 is a valid
 * -0.01 C and acc_x_mg=-1 is a valid ADXL362 sample.                  */
#define TS_ABSENT_U   (-1)
#define TS_ERROR_U    (-2)
#define TS_NA_U       (-3)
#define TS_ABSENT_S   (-1000001)
#define TS_ERROR_S    (-1000002)
#define TS_NA_S       (-1000003)

/*
 * NOTE (spec defect, for the revision log): docs/telemetry-spec.md
 * declares BOTH `enum sensor_state` (§3.1) and `struct sensor_state`
 * (§6.1). In C, struct/union/enum tags share a single namespace, so the
 * header as written in §6.1 does not compile. The enum tag is renamed to
 * `enum sensor_st`; the struct tag, the extern instance name `sensors`
 * and all four enumerator names are unchanged, which is everything
 * net.c's §6.4 helpers and main.c actually reference by name.
 */
enum sensor_st {
    SENS_UNKNOWN = 0,   /* never probed (only before the boot sweep)   */
    SENS_ABSENT  = 1,   /* probe completed with no ACK / declared off  */
    SENS_PRESENT = 2,   /* probe ACKed AND the ID register matched     */
    SENS_ERROR   = 3    /* was PRESENT; the last data read failed      */
};

/* Bit positions in sensors.mask and in the sens_mask telemetry key. */
enum sensor_id {
    SENS_ADT7420 = 0,   /* on-board, I2C bus 1 @ 0x4B                  */
    SENS_ADXL362,       /* on-board, axi_quad_spi_1                    */
    SENS_XADC,          /* on-board, DRP                               */
    SENS_BME688,        /* JA I2C 0x76 / 0x77                          */
    SENS_SCD41,         /* JA I2C 0x62                                 */
    SENS_VEML7700,      /* JA I2C 0x10                                 */
    SENS_AS7341,        /* JA I2C 0x39                                 */
    SENS_MAX30105,      /* JA I2C 0x57                                 */
    SENS_OV7670,        /* JA I2C (SCCB) 0x21                          */
    SENS_AD9226,        /* non-probeable, declared + liveness          */
    SENS_AD9708,        /* non-probeable, class C only                 */
    SENS_SIPM,          /* non-probeable, declared + liveness          */
    SENS_AD8332,        /* non-probeable, class C only                 */
    SENS_PIEZO,         /* non-probeable, declared + liveness          */
    SENS_PDM,           /* on-board ADMP421 + optional 6-mic array     */
    SENS_COUNT
};

/* Devices that hang off the external JA harness bus (axi_iic_0). The
 * i2c telemetry key counts these; sens_mask reports all of them.     */
#define JA_BUS_MASK  ((1u << SENS_BME688)  | (1u << SENS_SCD41)     | \
                      (1u << SENS_VEML7700)| (1u << SENS_AS7341)    | \
                      (1u << SENS_MAX30105)| (1u << SENS_OV7670))

/* Operator declaration of attached analog front ends (spec §3.3).
 * Default: nothing wired. Override with -DTELE_HARNESS_MASK=0x... */
#ifndef TELE_HARNESS_MASK
#define TELE_HARNESS_MASK  0u
#endif

/* ---- Personality capability bitmap (spec §3.4) ----------------------
 * Kept here rather than in net.c so sensors.c, net.c and dfx_task all
 * agree on one set of literals. Guarded so an independent definition in
 * net.c with the same values is not a redefinition error.            */
#ifndef CAP_PDM
#define RM_PDM      0
#define RM_SONAR    1
#define RM_HELLO_B  2
#define RM_HELLO_A  3
#define RM_CAM      4        /* reserved, not yet built */
#define RM_SIPM     5        /* reserved */
#define RM_RF       6        /* reserved */

#define CAP_PDM     0x01u
#define CAP_SONAR   0x02u
#define CAP_ADC     0x04u
#define CAP_DAC     0x08u
#define CAP_CAM     0x10u
#define CAP_SIPM    0x20u
#define CAP_AFE     0x40u
#endif

struct sensor_state {
    /* ---- discovery ------------------------------------------------ */
    volatile u32 st[SENS_COUNT];  /* enum sensor_st, one word each     */
    volatile u32 mask;            /* bit i set iff st[i] == SENS_PRESENT */
    volatile u32 probe_s;         /* seconds since the last full sweep */
    volatile u32 next_probe;      /* round-robin cursor, 0..7          */

    /* ---- on-board (written by sensors_task) ----------------------- */
    volatile s32 adt_temp_cc;
    volatile s32 acc_x_mg, acc_y_mg, acc_z_mg, acc_mag_mg, acc_temp_cc;
    volatile s32 xadc_temp_cc, xadc_vccint_mv, xadc_vccaux_mv, xadc_vbram_mv;

    /* ---- JA I2C (written by sensors_task) ------------------------- */
    volatile s32 bme_temp_cc, bme_rh_pct, bme_pres_pa, bme_gas_ohm;
    volatile s32 co2_ppm, scd_temp_cc, scd_rh_pct;
    volatile s32 light_lux, light_raw;
    volatile s32 as_cnt[10];      /* 415,445,480,515,555,590,630,680,clear,nir */
    volatile s32 as_flicker_hz;
    volatile s32 ppg_red_cnt, ppg_ir_cnt, ppg_grn_cnt;
    volatile s32 hr_bpm, spo2_pct, max_temp_cc;
    volatile s32 cam_pid;         /* (PID << 8) | VER, class S         */

    /* ---- fabric / RM (written by dfx_task or the acq path) -------- */
    volatile s32 mic_n_cnt, mic_spl_db, doa_deg;
    volatile s32 adc_rate_ksps, adc_min_code, adc_max_code;
    volatile s32 adc_rms_code, adc_rail_cnt;
    volatile s32 dac_mode, dac_freq_hz, dac_ampl_pct;
    volatile s32 cam_frames_cnt, cam_fps_hz, cam_lum_pct, cam_motion_pct;
    volatile s32 sipm_cps, sipm_tot_cnt, sipm_thresh_mv;
    volatile s32 afe_gain_db, afe_hilo;
    volatile s32 sonar_range_mm, sonar_tof_us, sonar_snr_db, sonar_bursts_cnt;
};

extern struct sensor_state sensors;

/* Zero the blackboard, set every st[] to SENS_UNKNOWN, soft-reset both
 * AXI IIC cores, and run one full bounded probe sweep (spec §3.5).
 * Blocks for <= 20 ms. Call once from sensors_task before the loop, and
 * before net_task can emit its first frame. */
void sensors_init(void);

/* One 1 Hz service pass: read every SENS_PRESENT device, probe exactly
 * one round-robin table slot, republish mask/probe_s. MUST return in
 * <= 20 ms (spec §3.6). Never called from net_task. */
void sensors_poll(void);

/* Fabric publishers, called by dfx_task / the acq path after a window.
 * Plain word stores into the blackboard; no locking. */
void sensors_publish_pdm(s32 n_ch, s32 spl_db, s32 doa_deg);
void sensors_publish_adc(s32 min_code, s32 max_code, s32 rms_code, s32 rail_cnt);
void sensors_publish_sonar(s32 range_mm, s32 tof_us, s32 snr_db, s32 bursts);
void sensors_publish_sipm(s32 cps, s32 total);
void sensors_publish_cam(s32 frames, s32 fps_hz, s32 lum_pct, s32 motion_pct);

/* Commanded state (class C) — call when the command is issued. */
void sensors_command_dac(s32 mode, s32 freq_hz, s32 ampl_pct);
void sensors_command_afe(s32 gain_db, s32 hilo);
void sensors_command_sipm(s32 thresh_mv);
void sensors_command_adc(s32 rate_ksps);

#endif /* SENSORS_H */
