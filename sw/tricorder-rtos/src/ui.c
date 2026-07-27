/*
 * Tricorder test-mode UI task.
 *
 * Reads the 16 slide switches (axi_gpio_3), maps SW0..SW14 onto the 15
 * test-mode functions, and renders an 8-character readout into the
 * dual-channel display GPIO (axi_gpio_4).  The top-level 7-seg scanner
 * consumes the 64-bit {ch2,ch1} value directly.
 *
 * Display protocol: byte i = segment pattern for digit i, digit 0 =
 * rightmost (AN0).  Within a byte bit0=CA .. bit6=CG, bit7=DP,
 * ACTIVE-HIGH here (the RTL inverts for the board's active-low pins).
 *
 * SW15 is reserved and ignored.  0 switches -> idle banner, exactly one
 * -> that function's readout, more than one -> "Err SEL".
 */

#include "FreeRTOS.h"
#include "task.h"

#include "xil_types.h"
#include "ui.h"

#define UI_AUDIO (*(volatile unsigned int *)0x40050000u)

/* axi_gpio_3 ch1: 16-bit switch input */
#define SW_DATA  (*(volatile u32 *)0x40030000u)
/* axi_gpio_4: ch1 = digits 0-3, ch2 = digits 4-7 */
#define DISP_LO  (*(volatile u32 *)0x40040000u)
#define DISP_HI  (*(volatile u32 *)0x40040008u)

#define SW_MASK  0x7FFFu   /* SW0..SW14; SW15 reserved */

/* S14 "uEr" readout: build/version ID, bump on UI protocol changes */
static const char build_id[] = "1.00";

struct ui_live ui_live;

/*
 * 7-seg font, active-high, bit0=CA .. bit6=CG (bit7=DP is applied by
 * ui_show for '.').  Covers the 7-seg-safe set plus crude stand-ins for
 * M and W, which the mandated function tags (PdM, PCM, SWAP, tSWP) use.
 */
static u8 segchar(char c)
{
    switch (c) {
    case '0': case 'O': return 0x3F;
    case '1':           return 0x06;
    case '2':           return 0x5B;
    case '3':           return 0x4F;
    case '4':           return 0x66;
    case '5':           return 0x6D;
    case '6':           return 0x7D;
    case '7':           return 0x07;
    case '8':           return 0x7F;
    case '9':           return 0x6F;
    case 'A':           return 0x77;
    case 'b':           return 0x7C;
    case 'C':           return 0x39;
    case 'c':           return 0x58;
    case 'd':           return 0x5E;
    case 'E':           return 0x79;
    case 'F':           return 0x71;
    case 'G':           return 0x3D;
    case 'H':           return 0x76;
    case 'h':           return 0x74;
    case 'I':           return 0x30;
    case 'J':           return 0x1E;
    case 'L':           return 0x38;
    case 'n':           return 0x54;
    case 'o':           return 0x5C;
    case 'P':           return 0x73;
    case 'q':           return 0x67;
    case 'r':           return 0x50;
    case 'S':           return 0x6D;
    case 't':           return 0x78;
    case 'U':           return 0x3E;
    case 'u':           return 0x1C;
    case 'y':           return 0x6E;
    case '-':           return 0x40;
    case 'M':           return 0x37; /* approximation: sides + top */
    case 'W':           return 0x3E; /* drawn as U (manual documents this) */
    default:            return 0x00; /* blank */
    }
}

/*
 * Render an 8-column string to the display.  s[0] is the leftmost digit
 * (AN7).  A '.' does not take a column: it sets DP on the character to
 * its left.  Short strings leave the remaining right-hand digits blank.
 */
static void ui_show(const char *s)
{
    u8 seg[8] = { 0 };
    int d = 7; /* next digit to fill, left to right */

    for (const char *p = s; *p; p++) {
        if (*p == '.') {
            if (d < 7)
                seg[d + 1] |= 0x80;
            continue;
        }
        if (d < 0)
            break;
        seg[d--] = segchar(*p);
    }
    DISP_LO = (u32)seg[0] | (u32)seg[1] << 8 |
              (u32)seg[2] << 16 | (u32)seg[3] << 24;
    DISP_HI = (u32)seg[4] | (u32)seg[5] << 8 |
              (u32)seg[6] << 16 | (u32)seg[7] << 24;
}

/* Append v in decimal at p; returns the new end (NUL-terminated). */
static char *fmt_dec(char *p, long v)
{
    char tmp[12];
    int n = 0;
    unsigned long u = (unsigned long)v;

    if (v < 0) {
        *p++ = '-';
        u = (unsigned long)-v;
    }
    do {
        tmp[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    while (n)
        *p++ = tmp[--n];
    *p = '\0';
    return p;
}

/*
 * Build the 8-column line: tag at the left, value right-aligned.  '.'
 * in the value costs no column (it becomes a DP).  If the value is too
 * wide, the tag is truncated from the right to make room — the value is
 * the datum, the tag is the label.
 */
static void compose(char *line, const char *tag, const char *val)
{
    int vcols = 0, vlen = 0, tlen = 0, i = 0;

    for (const char *p = val; *p; p++, vlen++)
        if (*p != '.')
            vcols++;
    while (tag[tlen])
        tlen++;
    if (tlen > 8 - vcols)
        tlen = (8 - vcols < 0) ? 0 : 8 - vcols;
    for (int k = 0; k < tlen; k++)
        line[i++] = tag[k];
    for (int k = 0; k < 8 - vcols - tlen; k++)
        line[i++] = ' ';
    for (int k = 0; k < vlen; k++)
        line[i++] = val[k];
    line[i] = '\0';
}

/* --- Per-function value formatters (out has room for 12 chars) ------- */

/* Placeholder functions: no real device in test mode, show an ON marker. */
static void val_on(char *out)
{
    out[0] = 'O';
    out[1] = 'n';
    out[2] = '\0';
}

/* S0-S3: switching one on REQUESTS that DFX personality; readout tracks
 * the load (---- while swapping, run when live). ui_task sets req_rm. */
static void val_rm(char *out, int rm)
{
    const char *m = (ui_live.cur_rm == rm) ? "run" :
                    (ui_live.cur_rm == -1) ? "----" : "LOAd";
    while (*m)
        *out++ = *m++;
    *out = '\0';
}
static void val_rm_pdm(char *out)   { val_rm(out, 0); }
static void val_rm_snr(char *out)   { val_rm(out, 1); }
static void val_rm_b(char *out)     { val_rm(out, 2); }
static void val_rm_a(char *out)     { val_rm(out, 3); }

/* S10-S12: live from the RM registers when the right personality is in */
#define RM_REG8  (*(volatile u32 *)0x44A10008u)
#define RM_REGC  (*(volatile u32 *)0x44A1000Cu)
static void val_pcm(char *out)
{
    if (ui_live.cur_rm == 0)
        fmt_dec(out, (long)(int)RM_REG8);
    else
        { out[0]='O'; out[1]='F'; out[2]='F'; out[3]='\0'; }
}
static void val_energy(char *out)
{
    if (ui_live.cur_rm == 0)
        fmt_dec(out, (long)(RM_REGC & 0x7FFFFFFFu));
    else
        { out[0]='O'; out[1]='F'; out[2]='F'; out[3]='\0'; }
}
static void val_peak(char *out)
{
    if (ui_live.cur_rm == 1)
        fmt_dec(out, (long)(RM_REGC >> 16 & 0xFFFu));
    else
        { out[0]='O'; out[1]='F'; out[2]='F'; out[3]='\0'; }
}

static void val_temp(char *out)     /* S4: centi-C -> "dd.d" deci-C */
{
    int d = ui_live.temp_centi / 10;
    char *p = out;

    if (d < 0) {
        *p++ = '-';
        d = -d;
    }
    p = fmt_dec(p, d / 10);
    *p++ = '.';
    *p++ = (char)('0' + d % 10);
    *p = '\0';
}

static void val_i2c(char *out)      /* S5: external bus device count */
{
    fmt_dec(out, ui_live.i2c_devices);
}

static void val_swaps(char *out)    /* S7: completed DFX swap count */
{
    fmt_dec(out, (long)ui_live.swaps);
}

static void val_uptime(char *out)   /* S9: seconds since boot */
{
    fmt_dec(out, (long)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000u));
}

static void val_swap_us(char *out)  /* S13: last swap in us, 0 = none yet */
{
    if (ui_live.last_swap_us == 0)
        val_on(out);
    else
        fmt_dec(out, (long)ui_live.last_swap_us);
}

static void val_build(char *out)    /* S14: static build/version ID */
{
    const char *p = build_id;
    char *q = out;

    while (*p)
        *q++ = *p++;
    *q = '\0';
}

/* --- The 15 test-mode functions, indexed by switch number ------------ */

static const struct ui_fn {
    const char *tag;
    void (*val)(char *out);
} ui_fns[15] = {
    { "PdM",  val_rm_pdm },  /* S0  loads the PDM personality */
    { "Snr",  val_rm_snr },  /* S1  loads the sonar personality */
    { "HELA", val_rm_a },    /* S2  loads Hello-A */
    { "HELB", val_rm_b },    /* S3  loads Hello-B */
    { "tEnP", val_temp },    /* S4  ADT7420 temperature (LIVE) */
    { "I2C",  val_i2c },     /* S5  external bus scan count (LIVE) */
    { "ddr",  val_on },      /* S6  DDR2 status */
    { "SWAP", val_swaps },   /* S7  DFX swap counter (LIVE) */
    { "FLSH", val_on },      /* S8  QSPI flash status */
    { "UP",   val_uptime },  /* S9  uptime seconds (LIVE) */
    { "PCM",  val_pcm },     /* S10 PDM PCM sample (live when PDM loaded) */
    { "EnG",  val_energy },  /* S11 PDM energy (live when PDM loaded) */
    { "PEA",  val_peak },    /* S12 sonar peak (live when sonar loaded) */
    { "tSWP", val_swap_us }, /* S13 last swap time, us (LIVE) */
    { "uEr",  val_build },   /* S14 build/version ID */
};

void ui_task(void *arg)
{
    char line[12], val[12];
    int prev_id = -1;      /* -1 idle, -2 error, 0..14 selected function */
    u32 audio = UI_AUDIO & 3u;   /* seed from the boot chirp's toggle state */

    (void)arg;
    for (;;) {
        u32 sw = SW_DATA & SW_MASK;
        int n = 0, id = 0;

        for (u32 b = sw; b; b >>= 1) {
            if (b & 1u)
                n++;
        }
        if (n == 0) {
            ui_show("trICordr");
            prev_id = -1;
        } else if (n > 1) {
            ui_show("Err SEL");
            if (prev_id != -2) {           /* entering error: alert sound */
                audio ^= 2u;
                UI_AUDIO = audio;
            }
            prev_id = -2;
        } else {
            while (!(sw & (1u << id)))
                id++;
            if (id != prev_id) {           /* new function on: scan sound */
                audio ^= 1u;
                UI_AUDIO = audio;
            }
            prev_id = id;
            /* S0-S3 request their DFX personality (ui->dfx handoff) */
            static const int sw2rm[4] = { 0, 1, 3, 2 }; /* HELA=A(3), HELB=B(2) */
            ui_live.req_rm = (id < 4) ? sw2rm[id] : ui_live.req_rm;
            ui_fns[id].val(val);
            compose(line, ui_fns[id].tag, val);
            ui_show(line);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
