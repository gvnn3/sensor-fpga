/*
 * Tricorder raw-Ethernet module (spec docs/ethernet-spec.md §8).
 *
 * No lwIP. EmacLite @ 0x40E00000 driven by raw MMIO from a single
 * FreeRTOS task (net_task): minimal ARP responder, ICMP echo responder,
 * 1 Hz UDP telemetry broadcast. Static IPv4 192.168.1.44/24, MAC
 * 02:18:3E:CF:44:01, UDP port 5001, destination 255.255.255.255.
 */

#ifndef NET_H
#define NET_H
#include "xil_types.h"

/* Bring-up per spec §9. Blocks (uses vTaskDelay) — call from a task,
 * after the scheduler starts. Returns a gate bitmask:
 *   bit0 NET_UP_MDIO  — PHY responded, ID matched (gate G1)
 *   bit1 NET_UP_LOOP  — loopback self-test passed (gate G2)
 * Negative return = hard failure (MDIO dead / TX stuck).            */
int  net_init(void);

/* Non-blocking RX service: drains up to 8 pending frames, answering
 * ARP for 192.168.1.44 and ICMP echo. Call every <= 10 ms.          */
void net_poll(void);

/* Build and transmit one telemetry broadcast (spec §7.3). Reads
 * ui_live + xTaskGetTickCount(); safe from any single task.         */
void net_send_telemetry(void);

/* Link state: BMSR bit 2 as last polled by net_poll (cached; net_poll
 * refreshes it at most once per second).                            */
int  net_link_up(void);

/* MDIO primitives (exposed for bring-up experiments / tests).       */
u16  mdio_read(u32 phyad, u32 regad);
void mdio_write(u32 phyad, u32 regad, u16 val);

#define NET_UP_MDIO 0x1
#define NET_UP_LOOP 0x2

/* FreeRTOS task (spec §8.5): net_init, then loop { net_poll every
 * 10 ms; net_send_telemetry every 1 s }. All four net functions above
 * are called from this one task only — no locks (spec §8.2).        */
void net_task(void *arg);

/* Debug counters (single-writer: net_task; read-only elsewhere).    */
struct net_stats {
    volatile u32 rx_frames;   /* frames pulled from the RX buffer     */
    volatile u32 rx_arp;      /* ARP requests answered                */
    volatile u32 rx_icmp;     /* ICMP echoes answered                 */
    volatile u32 rx_dropped;  /* frames ignored                       */
    volatile u32 tx_frames;   /* frames handed to the TX buffer       */
};
extern struct net_stats net_stats;

#endif /* NET_H */
