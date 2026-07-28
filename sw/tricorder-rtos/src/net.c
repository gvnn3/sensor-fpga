/*
 * Tricorder raw-Ethernet module — spec docs/ethernet-spec.md §6–§9.
 *
 * axi_ethernetlite (ping-pong disabled, polled) + LAN8720A PHY behind
 * the RMII shim. Raw MMIO in the style of main.c's SPI/DFX code: no
 * BSP driver, no lwIP, no dynamic allocation.
 *
 * EmacLite buffers are 32-bit word access ONLY (no byte enables):
 * frames are built in u8 staging arrays in DDR and copied as words.
 *
 * Concurrency (spec §8.2): everything here runs in net_task only.
 * This file owns 0x40E0xxxx/0x40E1xxxx exclusively.
 */

#include "FreeRTOS.h"
#include "task.h"

#include "xil_types.h"
#include "xil_printf.h"

#include "net.h"
#include "ui.h"

/* ---- MMIO map (spec §3.2 / §6.1 / §8.3 — frozen) -------------------- */

#define EMAC_BASE     0x40E00000u
#define EMAC_TXBUF    (EMAC_BASE + 0x0000)
#define EMAC_MDIOADDR (EMAC_BASE + 0x07E4)
#define EMAC_MDIOWR   (EMAC_BASE + 0x07E8)
#define EMAC_MDIORD   (EMAC_BASE + 0x07EC)
#define EMAC_MDIOCTRL (EMAC_BASE + 0x07F0)
#define EMAC_TXLEN    (EMAC_BASE + 0x07F4)
#define EMAC_GIE      (EMAC_BASE + 0x07F8)
#define EMAC_TXCTRL   (EMAC_BASE + 0x07FC)
#define EMAC_RXBUF    (EMAC_BASE + 0x1000)
#define EMAC_RXCTRL   (EMAC_BASE + 0x17FC)
#define ETH_RSTN_GPIO 0x40E10000u    /* axi_gpio_eth ch1 data reg, bit0 */

#define REG32(a)      (*(volatile u32 *)(a))

/* MDIOADDR fields */
#define MDIO_OP_READ  (1u << 10)
/* MDIOCTRL bits */
#define MDIOCTRL_EN   0x8u
#define MDIOCTRL_GO   0x1u
/* TX_CTRL bits */
#define TXCTRL_GO     0x1u
#define TXCTRL_PROG   0x2u
/* RX_CTRL bits */
#define RXCTRL_READY  0x1u

/* LAN8720A registers / values (spec §9) */
#define PHY_BMCR      0u
#define PHY_BMSR      1u
#define PHY_ID1       2u
#define PHY_ID2       3u
#define PHY_ANAR      4u
#define BMCR_LOOP_100FD 0x6100u  /* loopback | 100 Mb | full duplex     */
#define BMCR_AN_RESTART 0x1200u  /* autoneg enable | restart            */
#define ANAR_100_ONLY   0x0181u  /* 100TX FD + 100TX HD + 802.3, no 10M */
#define LAN8720_ID1     0x0007u
#define LAN8720_ID2_MSK 0xFFF0u
#define LAN8720_ID2     0xC0F0u

/* Bounded spins (100 MHz core; generous vs the 10 ms / 1 ms budgets) */
#define TX_SPIN       2000000u   /* ~10 ms of TX_CTRL polling           */
#define MDIO_SPIN     200000u    /* ~1 ms of MDIOCTRL polling           */

/* ---- Network constants (spec §7 — frozen) --------------------------- */

static const u8 our_mac[6] = { 0x02, 0x18, 0x3E, 0xCF, 0x44, 0x01 };
static u8 our_ip[4] = { 0, 0, 0, 0 };  /* DHCP lease; 0.0.0.0 = none */
static int have_ip = 0;
static const u8 bcast_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

#define UDP_PORT      5001u
#define ETH_MIN       60u        /* min frame, no FCS                   */
#define ETH_MAX       1514u      /* max frame, no FCS                   */

/* ---- Module state (all touched by net_task only, spec §8.2) --------- */

struct net_stats net_stats;

static u8  rxf[1536];            /* RX staging (spec §8.4)              */
static u8  txf[1536];            /* TX staging                          */
static u32 phy_addr = 1;         /* Digilent strap; §9.3 scans if not   */
static int link_up_cached = 0;
static u16 ip_id = 0;            /* IPv4 ID, incrementing, starts 0     */
static u32 tele_seq = 0;         /* telemetry seq, starts 0, wraps      */
static TickType_t last_link_poll = 0;

/* ---- Word-only buffer copies (spec §6.2) ---------------------------- */

/* Frame byte N lives at buffer_base + N, byte 0 in bits [7:0] of word 0
 * (little-endian lane mapping). Assembled bytewise so the C code is
 * endian-agnostic. len rounds up to a word; staging arrays have slack. */
static void buf_copy_to(volatile u32 *dst, const u8 *src, u32 len)
{
    u32 words = (len + 3u) / 4u;
    for (u32 i = 0; i < words; i++)
        dst[i] = (u32)src[4 * i]
               | ((u32)src[4 * i + 1] << 8)
               | ((u32)src[4 * i + 2] << 16)
               | ((u32)src[4 * i + 3] << 24);
}

static void buf_copy_from(u8 *dst, const volatile u32 *src, u32 len)
{
    u32 words = (len + 3u) / 4u;
    for (u32 i = 0; i < words; i++) {
        u32 w = src[i];
        dst[4 * i]     = (u8)w;
        dst[4 * i + 1] = (u8)(w >> 8);
        dst[4 * i + 2] = (u8)(w >> 16);
        dst[4 * i + 3] = (u8)(w >> 24);
    }
}

/* ---- Internet checksum (spec §7.4, RFC 1071) ------------------------ */

static u16 inet_csum(const u8 *p, u32 len)
{
    u32 sum = 0;

    while (len > 1) {
        sum += ((u32)p[0] << 8) | p[1];
        p += 2;
        len -= 2;
    }
    if (len)
        sum += (u32)p[0] << 8;         /* odd trailing byte, low pad 0 */
    while (sum >> 16)
        sum = (sum & 0xFFFFu) + (sum >> 16);
    return (u16)~sum;                  /* host order; store hi byte first */
}

/* ---- MDIO primitives (spec §6.3) ------------------------------------ */

static int mdio_wait(void)
{
    for (u32 i = 0; i < MDIO_SPIN; i++)
        if (!(REG32(EMAC_MDIOCTRL) & MDIOCTRL_GO))
            return 0;
    return -1;
}

u16 mdio_read(u32 phyad, u32 regad)
{
    REG32(EMAC_MDIOADDR) = MDIO_OP_READ | (phyad << 5) | regad;
    REG32(EMAC_MDIOCTRL) = MDIOCTRL_EN | MDIOCTRL_GO;
    if (mdio_wait() < 0)
        return 0xFFFFu;
    return (u16)(REG32(EMAC_MDIORD) & 0xFFFFu);
}

void mdio_write(u32 phyad, u32 regad, u16 val)
{
    REG32(EMAC_MDIOADDR) = (phyad << 5) | regad;   /* OP = 0: write */
    REG32(EMAC_MDIOWR)   = val;
    REG32(EMAC_MDIOCTRL) = MDIOCTRL_EN | MDIOCTRL_GO;
    (void)mdio_wait();
}

/* ---- Transmit (spec §6.3) ------------------------------------------- */

/* Send txf[0..len). Pads to 60 with zeros in software. Returns 0, or
 * -1 if the previous TX never completed (TX_CTRL stuck busy).        */
static int emac_send(u32 len)
{
    u32 i;

    for (i = 0; i < TX_SPIN; i++)
        if (!(REG32(EMAC_TXCTRL) & TXCTRL_GO))
            break;
    if (i == TX_SPIN)
        return -1;

    while (len < ETH_MIN)
        txf[len++] = 0x00;
    if (len > ETH_MAX)
        len = ETH_MAX;

    buf_copy_to((volatile u32 *)EMAC_TXBUF, txf, len);
    REG32(EMAC_TXLEN)  = len;
    REG32(EMAC_TXCTRL) = TXCTRL_GO;
    net_stats.tx_frames++;
    return 0;
}

/* ---- Frame builders (spec §7) --------------------------------------- */

static void eth_header(u8 *f, const u8 *dst, u16 ethertype)
{
    for (int i = 0; i < 6; i++) {
        f[i]     = dst[i];
        f[6 + i] = our_mac[i];
    }
    f[12] = (u8)(ethertype >> 8);
    f[13] = (u8)ethertype;
}

/* ARP (spec §7.1). reply=1: answer the request in rxf. reply=0:
 * gratuitous request (OPER=1, target ourselves, dst broadcast).      */
static void send_arp(int reply)
{
    eth_header(txf, reply ? &rxf[22] : bcast_mac, 0x0806u);
    txf[14] = 0x00; txf[15] = 0x01;                 /* HTYPE 1        */
    txf[16] = 0x08; txf[17] = 0x00;                 /* PTYPE IPv4     */
    txf[18] = 0x06;                                 /* HLEN           */
    txf[19] = 0x04;                                 /* PLEN           */
    txf[20] = 0x00; txf[21] = reply ? 0x02 : 0x01;  /* OPER           */
    for (int i = 0; i < 6; i++)
        txf[22 + i] = our_mac[i];                   /* SHA            */
    for (int i = 0; i < 4; i++)
        txf[28 + i] = our_ip[i];                    /* SPA            */
    for (int i = 0; i < 6; i++)                     /* THA            */
        txf[32 + i] = reply ? rxf[22 + i] : our_mac[i];
    for (int i = 0; i < 4; i++)                     /* TPA            */
        txf[38 + i] = reply ? rxf[28 + i] : our_ip[i];
    (void)emac_send(42);                            /* pads to 60     */
}

/* ARP request accept filter (spec §7.1). rxf holds >= 42 bytes.      */
static int arp_matches(void)
{
    if (rxf[14] != 0x00 || rxf[15] != 0x01)  return 0;  /* HTYPE      */
    if (rxf[16] != 0x08 || rxf[17] != 0x00)  return 0;  /* PTYPE      */
    if (rxf[18] != 0x06 || rxf[19] != 0x04)  return 0;  /* HLEN/PLEN  */
    if (rxf[20] != 0x00 || rxf[21] != 0x01)  return 0;  /* OPER=req   */
    for (int i = 0; i < 4; i++)
        if (rxf[38 + i] != our_ip[i])        return 0;  /* TPA        */
    return 1;
}

/* ICMP echo reply (spec §7.2). rxf holds the whole request, flen =
 * 14 + IP total length.                                              */
static void send_icmp_reply(u32 flen)
{
    u32 iplen = flen - 14;                 /* == request total length */
    u16 csum;

    for (u32 i = 0; i < flen; i++)         /* start from the request  */
        txf[i] = rxf[i];

    eth_header(txf, &rxf[6], 0x0800u);     /* swap MACs               */
    txf[14] = 0x45;                        /* Ver/IHL                 */
    txf[15] = 0x00;                        /* TOS                     */
    /* total length, ID: copied verbatim already                      */
    txf[20] = 0x00; txf[21] = 0x00;        /* flags/frag              */
    txf[22] = 0x40;                        /* TTL 64                  */
    txf[23] = 0x01;                        /* proto ICMP              */
    txf[24] = 0x00; txf[25] = 0x00;        /* csum: zero, recompute   */
    for (int i = 0; i < 4; i++) {
        txf[26 + i] = our_ip[i];           /* src = us                */
        txf[30 + i] = rxf[26 + i];         /* dst = requester         */
    }
    csum = inet_csum(&txf[14], 20);
    txf[24] = (u8)(csum >> 8);
    txf[25] = (u8)csum;

    txf[34] = 0x00;                        /* ICMP type: echo reply   */
    txf[35] = 0x00;                        /* code                    */
    txf[36] = 0x00; txf[37] = 0x00;        /* csum: zero, recompute   */
    csum = inet_csum(&txf[34], iplen - 20);
    txf[36] = (u8)(csum >> 8);
    txf[37] = (u8)csum;
    /* ICMP ID/seq/payload copied verbatim                            */

    (void)emac_send(flen);                 /* pads to 60 if needed    */
}

/* ---- Telemetry (spec §7.3) ------------------------------------------ */

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

/* ---- DHCP client (DISCOVER -> OFFER -> REQUEST -> ACK) --------------
 * Broadcast BOOTP over UDP 68->67; xid from the tick counter. Retries
 * inside net_task's loop; on ACK sets our_ip and ui_live.ip. */

static u32 dhcp_xid;
static u8  dhcp_state = 0;      /* 0 idle, 1 discover-sent, 2 request-sent */
static u8  dhcp_server[4], dhcp_offer[4];

static u32 dhcp_build(u8 msgtype)
{
    u8 *b = txf;
    u32 i;

    for (i = 0; i < 400; i++)
        b[i] = 0;
    eth_header(b, bcast_mac, 0x0800u);
    /* IPv4: 0.0.0.0 -> 255.255.255.255, UDP */
    b[14] = 0x45; b[16] = 0; b[17] = 0;          /* len patched below  */
    b[18] = (u8)(ip_id >> 8); b[19] = (u8)ip_id; ip_id++;
    b[22] = 64; b[23] = 17;                       /* TTL, UDP           */
    b[30] = b[31] = b[32] = b[33] = 0xFF;         /* dst broadcast      */
    /* UDP 68 -> 67 */
    b[34] = 0; b[35] = 68; b[36] = 0; b[37] = 67;
    /* BOOTP */
    u8 *d = b + 42;
    d[0] = 1; d[1] = 1; d[2] = 6;                 /* op, htype, hlen    */
    d[4] = (u8)(dhcp_xid >> 24); d[5] = (u8)(dhcp_xid >> 16);
    d[6] = (u8)(dhcp_xid >> 8);  d[7] = (u8)dhcp_xid;
    d[10] = 0x80;                                 /* flags: broadcast   */
    for (i = 0; i < 6; i++)
        d[28 + i] = our_mac[i];
    d[236] = 0x63; d[237] = 0x82; d[238] = 0x53; d[239] = 0x63; /* magic */
    u32 o = 240;
    d[o++] = 53; d[o++] = 1; d[o++] = msgtype;    /* DHCP message type  */
    if (msgtype == 3) {                           /* REQUEST            */
        d[o++] = 50; d[o++] = 4;
        for (i = 0; i < 4; i++) d[o++] = dhcp_offer[i];
        d[o++] = 54; d[o++] = 4;
        for (i = 0; i < 4; i++) d[o++] = dhcp_server[i];
    }
    d[o++] = 61; d[o++] = 7; d[o++] = 1;          /* client-id: 01+MAC  */
    for (i = 0; i < 6; i++)
        d[o++] = our_mac[i];
    d[o++] = 55; d[o++] = 2; d[o++] = 1; d[o++] = 3; /* want mask, gw   */
    d[o++] = 255;
    while (o < 300)                                /* BOOTP min padding  */
        d[o++] = 0;

    u32 udp_len = 8 + 42 - 42 + 258 + o - 258;     /* 8 + bootp bytes    */
    udp_len = 8 + o;
    u32 ip_len = 20 + udp_len;
    b[16] = (u8)(ip_len >> 8); b[17] = (u8)ip_len;
    b[38] = (u8)(udp_len >> 8); b[39] = (u8)udp_len;
    u16 c = inet_csum(b + 14, 20);
    b[24] = (u8)(c >> 8); b[25] = (u8)c;
    return 14 + ip_len;
}

static void dhcp_start(void)
{
    dhcp_xid = 0x54524900u | (xTaskGetTickCount() & 0xFFu);
    dhcp_state = 1;
    (void)emac_send(dhcp_build(1));                /* DISCOVER          */
    xil_printf("NET: DHCP discover\r\n");
}

/* Handle a received UDP 67->68 BOOTP frame; f = rxf, flen = frame len */
static void dhcp_rx(u32 flen)
{
    u8 *d = rxf + 42;

    if (flen < 42 + 240 + 4)
        return;
    if (d[0] != 2)                                 /* BOOTREPLY          */
        return;
    if (((u32)d[4] << 24 | (u32)d[5] << 16 | (u32)d[6] << 8 | d[7]) != dhcp_xid)
        return;
    /* find option 53 + 54 */
    u8 msgtype = 0;
    const u8 *srv = 0;
    u32 o = 240, lim = flen - 42;
    while (o + 1 < lim && d[o] != 255) {
        u8 opt = d[o], len = d[o + 1];
        if (opt == 0) { o++; continue; }
        if (opt == 53 && len == 1) msgtype = d[o + 2];
        if (opt == 54 && len == 4) srv = &d[o + 2];
        o += 2 + len;
    }
    if (msgtype == 2 && dhcp_state == 1) {         /* OFFER              */
        for (int i = 0; i < 4; i++) {
            dhcp_offer[i] = d[16 + i];             /* yiaddr             */
            dhcp_server[i] = srv ? srv[i] : rxf[26 + i];
        }
        dhcp_state = 2;
        (void)emac_send(dhcp_build(3));            /* REQUEST            */
        xil_printf("NET: DHCP request %d.%d.%d.%d\r\n",
                   dhcp_offer[0], dhcp_offer[1], dhcp_offer[2], dhcp_offer[3]);
    } else if (msgtype == 5 && dhcp_state == 2) {  /* ACK                */
        for (int i = 0; i < 4; i++)
            our_ip[i] = d[16 + i];
        have_ip = 1;
        dhcp_state = 0;
        ui_live.ip = (u32)our_ip[0] << 24 | (u32)our_ip[1] << 16 |
                     (u32)our_ip[2] << 8 | our_ip[3];
        xil_printf("NET: DHCP IP %d.%d.%d.%d\r\n",
                   our_ip[0], our_ip[1], our_ip[2], our_ip[3]);
    } else if (msgtype == 6) {                     /* NAK: start over    */
        dhcp_state = 0;
    }
}

/* Switch-change event frame: same UDP broadcast as telemetry, marked
 * type=event, carrying the switch number, its function tag, the raw
 * switch mask, and the state (idle / active / error). Emitted by
 * net_task within ~10 ms of the flip; ui_task only publishes the record
 * (single-writer rule for the MAC). */
static char *kv_s(char *p, const char *k, const char *v)
{
    while (*k)
        *p++ = *k++;
    *p++ = '=';
    while (*v)
        *p++ = *v++;
    *p++ = '\n';
    return p;
}

void net_send_event(unsigned int seq, int sw, unsigned int mask)
{
    char *p = (char *)&txf[42];
    u32 payload_len, iplen, udplen;
    u16 csum;
    const char *state = (sw == -2) ? "error" : (sw == -1) ? "idle" : "active";

    p = kv_s(p, "type",   "event");
    p = kv_u(p, "evt",    seq);
    p = kv_i(p, "sw",     sw);
    p = kv_s(p, "name",   ui_fn_name(sw));
    p = kv_s(p, "state",  state);
    p = kv_u(p, "mask",   mask);
    p = kv_u(p, "uptime", (u32)(xTaskGetTickCount() / configTICK_RATE_HZ));

    payload_len = (u32)(p - (char *)&txf[42]);
    iplen  = 28 + payload_len;
    udplen = 8 + payload_len;

    eth_header(txf, bcast_mac, 0x0800u);
    txf[14] = 0x45;
    txf[15] = 0x00;
    txf[16] = (u8)(iplen >> 8);
    txf[17] = (u8)iplen;
    txf[18] = (u8)(ip_id >> 8);
    txf[19] = (u8)ip_id;
    ip_id++;
    txf[20] = 0x00; txf[21] = 0x00;
    txf[22] = 64;
    txf[23] = 0x11;
    txf[24] = 0x00; txf[25] = 0x00;
    for (int i = 0; i < 4; i++) {
        txf[26 + i] = our_ip[i];
        txf[30 + i] = 0xFF;
    }
    csum = inet_csum(&txf[14], 20);
    txf[24] = (u8)(csum >> 8);
    txf[25] = (u8)csum;
    txf[34] = 0x13; txf[35] = 0x89;        /* sport 5001              */
    txf[36] = 0x13; txf[37] = 0x89;        /* dport 5001              */
    txf[38] = (u8)(udplen >> 8);
    txf[39] = (u8)udplen;
    txf[40] = 0x00; txf[41] = 0x00;        /* UDP csum 0              */
    (void)emac_send(42 + payload_len);
}

void net_send_telemetry(void)
{
    char *p = (char *)&txf[42];            /* payload after headers   */
    u32 payload_len, iplen, udplen;
    u16 csum;

    /* Payload: frozen keys, order, formats (spec §7.3.1) */
    p = kv_s(p, "type",    "telemetry");
    p = kv_u(p, "seq",     tele_seq);
    p = kv_u(p, "uptime",  (u32)(xTaskGetTickCount() / configTICK_RATE_HZ));
    p = kv_i(p, "temp_cc", ui_live.temp_centi);
    p = kv_i(p, "i2c",     ui_live.i2c_devices);
    p = kv_i(p, "rm",      ui_live.cur_rm);
    p = kv_u(p, "swaps",   ui_live.swaps);
    p = kv_u(p, "swap_us", ui_live.last_swap_us);
    p = kv_i(p, "pcm",     ui_live.pcm);
    p = kv_u(p, "energy",  ui_live.energy);
    p = kv_u(p, "peak",    ui_live.peak);
    payload_len = (u32)(p - (char *)&txf[42]);
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
    for (int i = 0; i < 4; i++) {
        txf[26 + i] = our_ip[i];           /* src IP                  */
        txf[30 + i] = 0xFF;                /* dst 255.255.255.255     */
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
        xil_printf("NET: telemetry seq=%u\r\n", (unsigned)tele_seq);
    tele_seq++;
}

/* ---- RX dispatch (spec §8.4) ---------------------------------------- */

/* One ready frame is in the RX buffer. Copy, classify, release, reply. */
static void rx_dispatch(void)
{
    u16 ethertype;
    u32 flen;

    net_stats.rx_frames++;
    buf_copy_from(rxf, (const volatile u32 *)EMAC_RXBUF, 64);
    ethertype = (u16)(((u16)rxf[12] << 8) | rxf[13]);

    /* Bring-up logger: first 40 frames, show what actually arrives so a
     * DHCP OFFER (UDP dport 68) is distinguishable from "never sent". */
    if (net_stats.rx_frames <= 40) {
        u16 sp = (u16)(((u16)rxf[34] << 8) | rxf[35]);
        u16 dp = (u16)(((u16)rxf[36] << 8) | rxf[37]);
        xil_printf("RX %u: et=%04x dst=%02x%02x%02x proto=%02x %u->%u\r\n",
                   (unsigned)net_stats.rx_frames, ethertype,
                   rxf[0], rxf[1], rxf[2], rxf[23], sp, dp);
    }

    if (ethertype == 0x0806u) {                       /* ARP: 42 B     */
        REG32(EMAC_RXCTRL) = 0;                       /* release       */
        if (arp_matches()) {
            net_stats.rx_arp++;
            send_arp(1);
        } else {
            net_stats.rx_dropped++;
        }
        return;
    }

    if (ethertype == 0x0800u) {                       /* IPv4          */
        u32 iplen = ((u32)rxf[16] << 8) | rxf[17];
        flen = 14 + iplen;
        if (flen > ETH_MAX)
            flen = ETH_MAX;
        if (rxf[14] == 0x45 &&                        /* v4, IHL 5     */
            rxf[23] == 0x01 &&                        /* ICMP          */
            rxf[30] == our_ip[0] && rxf[31] == our_ip[1] &&
            rxf[32] == our_ip[2] && rxf[33] == our_ip[3] &&
            rxf[34] == 0x08 && rxf[35] == 0x00 &&     /* echo request  */
            flen >= 42) {
            if (flen > 64)                            /* rest of frame */
                buf_copy_from(rxf, (const volatile u32 *)EMAC_RXBUF, flen);
            REG32(EMAC_RXCTRL) = 0;                   /* release       */
            net_stats.rx_icmp++;
            send_icmp_reply(flen);
            return;
        }
        if (rxf[14] == 0x45 && rxf[23] == 0x11 &&     /* UDP           */
            rxf[36] == 0 && rxf[37] == 68 &&          /* dst port 68   */
            flen >= 42 + 240) {                       /* BOOTP reply   */
            if (flen > 64)
                buf_copy_from(rxf, (const volatile u32 *)EMAC_RXBUF, flen);
            REG32(EMAC_RXCTRL) = 0;
            dhcp_rx(flen);
            return;
        }
    }

    REG32(EMAC_RXCTRL) = 0;                           /* release       */
    net_stats.rx_dropped++;
}

void net_poll(void)
{
    TickType_t now;

    for (int n = 0; n < 8; n++) {
        if (!(REG32(EMAC_RXCTRL) & RXCTRL_READY))
            break;
        rx_dispatch();
    }

    /* Link watch: refresh BMSR bit 2 at most once per second (§8.4). */
    now = xTaskGetTickCount();
    if ((TickType_t)(now - last_link_poll) >= pdMS_TO_TICKS(1000)) {
        int up;

        last_link_poll = now;
        up = (mdio_read(phy_addr, PHY_BMSR) & 0x0004u) != 0;
        if (up && !link_up_cached) {
            xil_printf("NET: link up\r\n");           /* E3            */
            send_arp(0);                              /* gratuitous    */
        }
        link_up_cached = up;
    }
}

int net_link_up(void)
{
    return link_up_cached;
}

/* ---- Bring-up (spec §9) --------------------------------------------- */

/* Program the station MAC into the core (spec §6.3). Returns 0 or -1. */
static int emac_program_mac(void)
{
    u8 m[8] = { 0 };

    for (int i = 0; i < 6; i++)
        m[i] = our_mac[i];
    /* PG135 sequence: MAC bytes in the TX buffer, TX_LEN = 6, set the
     * PROGRAM bit alone, then wait for the PROGRAM bit (not GO) to
     * clear. Setting GO here transmits the MAC as a frame instead of
     * programming the address filter - which leaves the MAC accepting
     * broadcast only (observed: no unicast frame ever received). */
    /* NOTE (bring-up): adding TX_LEN=6 per PG135 killed reception
     * entirely (rx_frames 0), and PROG-only never self-clears here.
     * This form is the one that leaves broadcast RX/TX working; the
     * unicast filter is still unproven - see notebook entry 32. */
    buf_copy_to((volatile u32 *)EMAC_TXBUF, m, 6);
    REG32(EMAC_TXCTRL) = TXCTRL_PROG | TXCTRL_GO;
    for (u32 i = 0; i < TX_SPIN; i++)
        if (!(REG32(EMAC_TXCTRL) & TXCTRL_GO))
            return 0;
    return -1;
}

/* PHY internal loopback self-test (spec §9.4): 60-byte frame to
 * ourselves, EtherType 0x88B5; returns matched byte count (0..60).   */
static u32 loopback_try(void)
{
    u32 matched = 0;

    eth_header(txf, our_mac, 0x88B5u);     /* dst = src = OUR_MAC     */
    for (u32 i = 0; i < 46; i++)
        txf[14 + i] = (u8)i;
    if (emac_send(ETH_MIN) < 0)
        return 0;

    for (int ms = 0; ms < 100; ms++) {     /* poll RX up to 100 ms    */
        if (REG32(EMAC_RXCTRL) & RXCTRL_READY) {
            buf_copy_from(rxf, (const volatile u32 *)EMAC_RXBUF, ETH_MIN);
            REG32(EMAC_RXCTRL) = 0;
            for (u32 i = 0; i < ETH_MIN; i++)
                if (rxf[i] == txf[i])
                    matched++;
            return matched;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return 0;
}

int net_init(void)
{
    u16 id1, id2;
    u32 matched;
    int up = 0;

    /* 1. Program station MAC while the PHY is still held in reset
     *    (axi_gpio_eth powers up driving 0 = reset asserted).        */
    if (emac_program_mac() < 0) {
        xil_printf("NET: MAC program TX stuck\r\n");
        return -2;
    }

    /* 2. Reset pulse: >= 25 ms low, then 25 ms strap-latch settle.   */
    REG32(ETH_RSTN_GPIO) = 0;
    vTaskDelay(pdMS_TO_TICKS(25));
    REG32(ETH_RSTN_GPIO) = 1;
    vTaskDelay(pdMS_TO_TICKS(25));

    /* 3. MDIO enable + PHY ID check (gate G1). Expect address 1.     */
    REG32(EMAC_MDIOCTRL) = MDIOCTRL_EN;
    id1 = mdio_read(phy_addr, PHY_ID1);
    id2 = mdio_read(phy_addr, PHY_ID2);
    if (id1 != LAN8720_ID1) {
        for (u32 a = 0; a < 32; a++) {
            u16 i1 = mdio_read(a, PHY_ID1);
            u16 i2 = mdio_read(a, PHY_ID2);
            if (i1 == LAN8720_ID1 && (i2 & LAN8720_ID2_MSK) == LAN8720_ID2) {
                phy_addr = a;
                id1 = i1;
                id2 = i2;
                break;
            }
        }
    }
    xil_printf("NET: PHY id %04x %04x @%u\r\n",      /* E1            */
               id1, id2, (unsigned)phy_addr);
    if (id1 != LAN8720_ID1 || (id2 & LAN8720_ID2_MSK) != LAN8720_ID2)
        return -1;
    up |= NET_UP_MDIO;

    /* 4. PHY internal loopback self-test (gate G2), one retry.       */
    mdio_write(phy_addr, PHY_BMCR, BMCR_LOOP_100FD);
    vTaskDelay(pdMS_TO_TICKS(10));
    matched = loopback_try();
    if (matched != ETH_MIN)
        matched = loopback_try();          /* first-frame flakiness   */
    xil_printf("NET: loopback %u/%u bytes %s\r\n",   /* E2            */
               (unsigned)matched, (unsigned)ETH_MIN,
               matched == ETH_MIN ? "OK" : "FAIL");
    if (matched == ETH_MIN)
        up |= NET_UP_LOOP;
    /* On failure continue anyway: cable sessions can still diagnose. */

    /* 5. Normal mode: advertise 100BASE-TX only (shim is 100 Mb
     *    fixed), then enable + restart autoneg. Link-up is announced
     *    asynchronously by net_poll.                                 */
    mdio_write(phy_addr, PHY_ANAR, ANAR_100_ONLY);
    mdio_write(phy_addr, PHY_BMCR, BMCR_AN_RESTART);

    /* Verify loopback really cleared: a PHY left in internal loopback
     * swallows every transmit (observed on hardware - our own frames
     * came back and nothing reached the wire). Retry until bit 14 is 0. */
    for (int i = 0; i < 10; i++) {
        u16 bmcr = mdio_read(phy_addr, PHY_BMCR);
        if (!(bmcr & 0x4000u))
            break;
        xil_printf("NET: BMCR %04x still in loopback, clearing\r\n", bmcr);
        mdio_write(phy_addr, PHY_BMCR, BMCR_AN_RESTART);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    xil_printf("NET: BMCR %04x (bit14 loopback must be 0)\r\n",
               mdio_read(phy_addr, PHY_BMCR));
    /* Wait for autoneg to complete, then report the negotiated mode:
     * LAN8720A reg 31 bits[4:2] = 001 10HD, 101 10FD, 010 100HD, 110 100FD */
    for (int i = 0; i < 40; i++) {
        if (mdio_read(phy_addr, PHY_BMSR) & 0x0020u)   /* AN complete */
            break;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    xil_printf("NET: BMSR %04x ANLPAR %04x SPECIAL %04x\r\n",
               mdio_read(phy_addr, PHY_BMSR),
               mdio_read(phy_addr, 5),
               mdio_read(phy_addr, 31));

    return up;
}

/* ---- FreeRTOS task (spec §8.5) -------------------------------------- */

void net_task(void *arg)
{
    unsigned n = 0;
    int up;

    (void)arg;
    up = net_init();                       /* prints E1/E2            */
    if (up < 0)
        xil_printf("NET: init hard failure (%d)\r\n", up);
    else
        xil_printf("NET: init gates=%s%s\r\n",
                   (up & NET_UP_MDIO) ? "MDIO " : "",
                   (up & NET_UP_LOOP) ? "LOOP" : "");
    /* Run on regardless: cable-attached tests may still work.        */

    for (;;) {
        if (up >= 0) {
            net_poll();
            {   /* switch events: emit one frame per new sequence */
                static unsigned int evt_seen = 0;
                unsigned int es = ui_live.evt_seq;
                if (es != evt_seen) {
                    evt_seen = es;
                    net_send_event(es, ui_live.evt_sw, ui_live.evt_mask);
                }
            }
            if (++n >= 100) {              /* once per second         */
                n = 0;
                static unsigned dhcp_tick = 0;
                if (!have_ip && link_up_cached && (dhcp_tick++ % 3 == 0)) {
                    /* fresh DISCOVER every 3 s until an ACK lands   */
                    dhcp_start();
                }
                net_send_telemetry();   /* also a TX heartbeat pre-lease */
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
