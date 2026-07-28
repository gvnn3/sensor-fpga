/*
 * Tricorder test-mode UI: switch-selected 7-seg readouts.
 *
 * Other tasks publish live values through ui_live; ui_task samples the
 * switches every 100 ms and drives the 8-digit display registers.
 */

#ifndef UI_H
#define UI_H

/* Live values filled in by main.c's tasks (single-writer per field;
 * plain volatile is enough for these monotonic/latest-value readouts). */
struct ui_live {
    volatile int req_rm;        /* -1 none, 0 PDM, 1 sonar, 2 B, 3 A */
    volatile unsigned int ip;   /* DHCP lease, host byte order; 0 = none */
    volatile int cur_rm;        /* loaded personality, -1 while swapping */
    volatile int      temp_centi;   /* ADT7420, centi-C (S4) */
    volatile unsigned swaps;        /* completed DFX swaps (S7) */
    volatile unsigned last_swap_us; /* last swap duration, us; 0 = none (S13) */
    volatile int      i2c_devices;  /* external bus scan result (S5) */
    volatile int      pcm;          /* last PDM PCM sample (S10, future) */
    volatile unsigned energy;       /* last PDM energy window (S11, future) */
    volatile unsigned peak;         /* last sonar peak (S12, future) */
};

extern struct ui_live ui_live;

void ui_task(void *arg);

#endif /* UI_H */
