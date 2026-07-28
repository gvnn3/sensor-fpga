# Tricorder Ethernet Telemetry — Engineering Specification

Status: **approved for implementation** — branch `gnn-tricorder-ethernet`,
27 Jul 2026. Targets the running system described in `docs/plan.md` §3/§5
(Nexys A7-100T, classic MicroBlaze + FreeRTOS, Vivado/Vitis 2025.2,
standalone QSPI boot, switch-driven DFX personalities).

This document is the single source of truth for three independent
implementers:

* **Implementer A (hardware)** — §3 block-design additions, §4 RMII shim,
  §5 pins/XDC.
* **Implementer B (software)** — §6 EmacLite programming model, §7 frame
  formats, §8 `net.c`/`net.h` module, §9 PHY bring-up.
* **Implementer C (test)** — §10 test plan; also owns the UART evidence
  strings in §10.3, which A and B must not change.

Every cross-implementer interface (shim port list, register offsets, MMIO
base addresses, frame bytes, UART strings, telemetry keys) is **frozen** by
this spec. Deviations require a spec revision, not a side agreement.

---

## 1. Overview, goals, non-goals

### 1.1 Goal

Get live telemetry and other data **out** of the tricorder over the
Nexys A7's on-board Ethernet: a host on the same LAN sees a 1 Hz
human-readable UDP broadcast of the instrument's state (`nc -ul 5001`),
and can `ping` the board. The design must be provable **without an
Ethernet cable** on the bench (MDIO + PHY-loopback self-test), because
cable-attached testing happens later.

### 1.2 Fixed architecture (decided, do not relitigate)

| Item | Decision |
|---|---|
| PHY | On-board SMSC **LAN8720A**, RMII, 10/100. FPGA supplies the 50 MHz REFCLK on pin **D5**. |
| MAC | **axi_ethernetlite** (MII, 100 Mb) in the block design + a hand-written **MII↔RMII shim** at the top level (`mii_to_rmii` IP no longer ships). Shim clocked at 50 MHz; **100 Mbps fixed**, no 10 Mb support. |
| Software | **No lwIP.** Self-contained raw-Ethernet module `net.c`/`net.h` driving EmacLite registers directly (same raw-MMIO style as the DFX/QSPI code): minimal ARP responder, ICMP echo responder, 1 Hz UDP telemetry broadcast. |
| Addressing | Static IPv4 **192.168.1.44/24**; MAC **02:18:3E:CF:44:01** (locally administered); UDP src/dst port **5001**; telemetry destination **255.255.255.255** (broadcast — no ARP resolution needed to transmit). |
| Payload | ASCII `key=value` lines from `struct ui_live` + `xTaskGetTickCount()`. |
| EmacLite | **Polled** (no interrupt wiring in v1), **ping-pong disabled** (single TX buffer, single RX buffer), base address **0x40E00000**. |

### 1.3 Non-goals (v1)

* No lwIP, TCP, DHCP, DNS, IGMP, IPv6, TFTP (TFTP-to-ICAP is a later
  plan-§3 item and builds on this module).
* No interrupts — `net_poll()` is called from a FreeRTOS task.
* No 10 Mbps support anywhere (shim is 100 Mb fixed; ANAR advertises
  100BASE-TX only).
* No inbound control channel: received UDP is ignored. RX handling is
  exactly ARP + ICMP echo.
* No checksum offload, no jumbo frames (max frame 1518 B), no VLAN.
* No half-duplex correctness guarantees (COL is tied 0; use a switched
  full-duplex LAN, which auto-negotiation will select).

---

## 2. System picture

```
                 100 MHz AXI                     50 MHz domain          board
 ┌────────────┐  ┌────────────────┐  MII (25 MHz) ┌───────────┐  RMII  ┌─────────┐
 │ MicroBlaze │──│ axi_ethernetlite│◄────────────►│ rmii_shim │◄──────►│ LAN8720A│═▶ RJ45
 │  FreeRTOS  │  │  @ 0x40E00000  │  4-bit nibbles│ (top RTL) │ 2-bit  │  PHY    │
 │  net_task  │  └────────────────┘               └───────────┘ dibits └─────────┘
 └────────────┘        │  MDC/MDIO ─────────────────────────────────────────┘
                       └─ phy managed by EmacLite MDIO engine
   clk_wiz: new 50 MHz output ──► shim clk50 ──► ODDR ──► eth_refclk (D5) ──► PHY REFCLK
   axi_gpio_eth @ 0x40E10000 bit0 ──► eth_rstn (B3)
```

Everything above the shim is stock IP configured in the BD; the shim and
the pin plumbing live in `hw/rtl/` and the top wrapper
`hw/rtl/tricorder_top_nexys.v`, exactly like the existing 7-seg/audio
peripherals.

---

## 3. Hardware: block-design additions (Implementer A)

### 3.1 New BD content

1. **`axi_ethernetlite_0`**
   * Base address **0x40E00000**, 8 KB used (map 64 KB, 0x40E00000–0x40E0FFFF).
   * Configuration: `C_TX_PING_PONG = 0`, `C_RX_PING_PONG = 0`
     (ping-pong **disabled** — single-buffer mode), internal loopback
     disabled, MDIO **included**.
   * `s_axi_aclk` = existing 100 MHz AXI clock; connect to the existing
     periph interconnect and reset like the other AXI-Lite slaves.
   * Interrupt output **left unconnected** (polled v1).
   * The **MII interface and MDIO interface are made external** on the BD
     so the top wrapper can wire them to the shim and pins.
2. **`clk_wiz`**: add one new output, **`clk_out_50` = 50.000 MHz**, from
   the existing MMCM (same VCO, phase-aligned with the other outputs).
   This clock drives (a) the shim, (b) the REFCLK ODDR. No other new
   clocks: the 25 MHz MII clock is generated *inside the shim* (§4.3).
3. **`axi_gpio_eth`** (new `axi_gpio`): base address **0x40E10000**,
   single channel, **1-bit all-output**, `GPIO_DEFAULT_TRI = 0` (driven),
   `GPIO_DEFAULT_VAL = 0`. Bit 0 = `eth_rstn` (active-low PHY reset).
   Default 0 means the PHY is **held in reset from configuration until
   software releases it** — the bring-up sequence (§9) owns all timing.

Nothing else in the BD changes; the DFX partition, MIG, and all existing
peripherals are untouched. Re-export the BD Tcl (`write_bd_tcl`) per repo
convention.

### 3.2 Address map additions (frozen — software depends on these)

| Base | Size | Peripheral |
|---|---|---|
| `0x40E00000` | 8 KB (64 KB mapped) | axi_ethernetlite_0 |
| `0x40E10000` | 64 KB | axi_gpio_eth (bit 0 = eth_rstn) |

### 3.3 Top-wrapper wiring (`tricorder_top_nexys.v`)

New ports (names frozen; they match the XDC in §5):

```verilog
    // Ethernet: LAN8720A RMII + management
    output wire        eth_refclk,   // 50 MHz to PHY (D5)
    output wire        eth_rstn,     // PHY reset, active low (B3)
    output wire        eth_mdc,      // (C9)
    inout  wire        eth_mdio,     // (A9)
    input  wire        eth_crsdv,    // (D9)
    input  wire        eth_rxerr,    // (C10)
    input  wire [1:0]  eth_rxd,      // (C11, D10)
    output wire        eth_txen,     // (B9)
    output wire [1:0]  eth_txd,      // (A10, A8)
    input  wire        eth_intn      // (B8) — unused in v1, keep wired
```

Wrapper responsibilities:

* Instantiate `rmii_shim` (§4) between the BD's external MII port and the
  `eth_*` RMII pins.
* **REFCLK forwarding**: drive `eth_refclk` from `clk_out_50` through an
  `ODDR` (`D1=1, D2=0, SRTYPE="SYNC"`) — never route a clock to a pin
  through fabric directly.
* **MDIO**: instantiate an `IOBUF` — `I = phy_mdio_o`, `T = phy_mdio_t`,
  `O = phy_mdio_i`, `IO = eth_mdio` (EmacLite exposes the three-wire
  form on the external MDIO interface). `eth_mdc` = EmacLite `phy_mdc`.
* `eth_rstn` = axi_gpio_eth bit 0 (routed out of the BD as an external
  GPIO port).
* `eth_intn` — input, terminate into the unused-input heartbeat fold
  (same trick the wrapper already uses) so it isn't pruned/warned.
* EmacLite MII inputs `phy_crs`, `phy_col`, `phy_rx_er`, `phy_dv`,
  `phy_rx_data[3:0]`, `phy_tx_clk`, `phy_rx_clk` all come from the shim;
  `phy_tx_en`, `phy_tx_data[3:0]` go to the shim.

---

## 4. The MII↔RMII shim (`hw/rtl/rmii_shim.v`)

One module, one clock (`clk50`), roughly 100 lines. 100 Mbps only.

### 4.1 Port list (frozen)

```verilog
module rmii_shim #(
    parameter RX_SAMPLE_FALLING = 0   // §4.6 timing fallback, default off
)(
    input  wire        clk50,        // clk_wiz clk_out_50; same clock the
                                     // ODDR forwards to the PHY as REFCLK
    input  wire        rst,          // active-high, synchronous to clk50
                                     // (tie to BD peripheral reset, resynced)

    // ---- MII side: toward axi_ethernetlite (all 25 MHz timing) ----
    output wire        mii_tx_clk,   // 25 MHz, BUFG-driven (§4.3)
    output wire        mii_rx_clk,   // same 25 MHz net as mii_tx_clk
    input  wire        mii_tx_en,    // from EmacLite, launched on mii_tx_clk↑
    input  wire [3:0]  mii_txd,      //   nibble, bit0 = first bit on wire
    output reg         mii_rx_dv,    // to EmacLite, sampled on mii_rx_clk↑
    output reg  [3:0]  mii_rxd,
    output reg         mii_rx_er,
    output wire        mii_crs,      // = rx frame activity (§4.5)
    output wire        mii_col,      // constant 1'b0 (full duplex only)

    // ---- RMII side: toward LAN8720A (all 50 MHz timing) ----
    output reg         rmii_tx_en,
    output reg  [1:0]  rmii_txd,     // bit0 = first bit on wire
    input  wire        rmii_crs_dv,
    input  wire [1:0]  rmii_rxd,
    input  wire        rmii_rx_er
);
```

### 4.2 Rate contract

At 100 Mbps: MII moves **one nibble per 25 MHz cycle** (4 bits × 25 MHz);
RMII moves **one dibit per 50 MHz cycle** (2 bits × 50 MHz). Both equal
100 Mb/s — the shim is a pure width/clock converter with **no FIFOs beyond
one nibble of elasticity** (§4.5) and no rate adaptation. Bit order
everywhere is **LSB-first on the wire**: MII nibble bit 0 ↔ first RMII
dibit bit 0.

Mapping of one byte `b[7:0]`:
* MII: nibble 1 = `b[3:0]`, nibble 2 = `b[7:4]`.
* RMII: dibit 1 = `b[1:0]`, dibit 2 = `b[3:2]`, dibit 3 = `b[5:4]`,
  dibit 4 = `b[7:6]`.
* Shim: MII nibble `n[3:0]` = two consecutive dibits, **first dibit →
  `n[1:0]`**, second → `n[3:2]`.

### 4.3 Clock generation (25 MHz by division, enables in the 50 MHz domain)

* One flip-flop `clk25_r` toggles on every `clk50` rising edge
  (reset to 0). `mii_tx_clk = mii_rx_clk =` `clk25_r` **through a BUFG**.
* Because `clk25` is a generated clock derived from `clk50` in the same
  clock group, all shim↔EmacLite paths are timed synchronously by Vivado
  — there is no true CDC anywhere in the shim. XDC (§5.3) declares
  `create_generated_clock -divide_by 2`.
* Phase bookkeeping inside the shim uses `clk25_r` itself as the enable:
  * `clk25_r == 0` on a given `clk50` cycle ⇒ `mii_*_clk` **rises** at
    the end of that cycle ("pre-rise" phase).
  * `clk25_r == 1` ⇒ `mii_*_clk` **falls** at the end of that cycle
    ("post-rise" phase).

### 4.4 TX path (MAC → PHY): 50 MHz domain, MII sampled by enable

EmacLite launches `mii_tx_en`/`mii_txd` on `mii_tx_clk` rising edges.
The shim, running on `clk50`:

1. **Sample** `mii_tx_en`/`mii_txd` on the `clk50` edge where
   `clk25_r == 1` (i.e., one 50 MHz cycle after the MII rising edge —
   the data has had a half 25 MHz period to settle; trivially met, same
   clock group).
2. **Serialize** the captured nibble over the next two `clk50` cycles:
   first `rmii_txd <= nib[1:0]`, then `rmii_txd <= nib[3:2]`, with
   `rmii_tx_en` asserted for both cycles. When the sampled `mii_tx_en`
   is 0, drive `rmii_tx_en = 0`, `rmii_txd = 2'b00`.
3. Register `rmii_tx_en`/`rmii_txd` in **IOB output flops** on `clk50`
   rising edges. Fixed pipeline latency (2–3 clk50 cycles) is irrelevant.

The preamble/SFD comes from EmacLite (it transmits the full 7+1 preamble
bytes and appends the FCS); the shim converts them like any other data —
it never inspects frame contents on TX.

RMII has no TX_ER; EmacLite's MII has none either. Nothing to map.

### 4.5 RX path (PHY → MAC): CRS_DV framing → MII RX_DV + RX_CLK

RMII folds carrier-sense and data-valid into one signal, `CRS_DV`, and
the PHY may deliver `00` dibits between carrier assertion and the start
of preamble. The shim recovers MII semantics as follows (all on `clk50`
rising edges, after the input capture stage of §4.6):

1. **Idle → hunting:** when `rmii_crs_dv` is first sampled high, enter
   HUNT. In HUNT, discard `00` dibits.
2. **Dibit-pair alignment:** the first **non-`00`** dibit while
   `rmii_crs_dv` is high is by definition the first preamble dibit
   (`01`); it fixes the pair phase — that dibit is the *low* half of the
   first nibble, the next dibit is its *high* half, and so on for the
   whole frame. (Preamble `0x55` = dibits `01,01,01,01`; SFD `0xD5` =
   `01,01,01,11` — EmacLite performs its own SFD hunt on the nibble
   stream, so the shim passes preamble and SFD through untouched.)
3. **Nibble assembly:** every two dibits form one nibble
   (first → `[1:0]`, second → `[3:2]`), written with a "ready" flag into
   a **one-nibble elasticity register**. Both sides run at exactly
   25 MHz nibble rate, so one nibble of skew absorption is sufficient by
   construction — no FIFO.
4. **MII output phasing:** `mii_rxd`/`mii_rx_dv`/`mii_rx_er` update
   **only on `clk50` cycles where `clk25_r == 1`** (i.e., just after the
   MII clock's falling edge), so they are stable for a full setup window
   before the next `mii_rx_clk` rising edge, where EmacLite samples them.
   `mii_rx_dv` asserts with the first preamble nibble and stays asserted
   through the last frame nibble.
5. **End of frame / CRS_DV toggling:** per the RMII spec, at end of
   carrier the PHY may **toggle CRS_DV at 25 MHz** (high in one dibit
   slot, low in the other) to signal "carrier gone, data still valid".
   Rule (evaluated per aligned dibit *pair*):
   * CRS_DV high in **either** cycle of the pair ⇒ frame continues, the
     pair's dibits are valid.
   * CRS_DV low in **both** cycles of the pair ⇒ frame is over:
     deassert `mii_rx_dv` at the next MII update slot, return to idle.
   Frames are whole bytes at 100 Mb, so a frame always ends on a pair
   boundary; a trailing half-nibble cannot occur and need not be handled.
6. **RX_ER:** `rmii_rx_er` sampled high in either cycle of a pair ⇒
   assert `mii_rx_er` for that nibble slot. (EmacLite will drop the
   frame on FCS anyway; this is belt-and-suspenders.)
7. **CRS / COL:** `mii_crs` = "RX frame active" (asserted from CRS_DV
   detection until end-of-frame); `mii_col = 1'b0`. We only operate
   full-duplex (§1.3); EmacLite may defer a pending TX while CRS is
   high, which is harmless at our traffic levels and does not affect the
   loopback self-test (TX has already started before our own frame
   returns).

### 4.6 Input capture, reset, and error handling

* **Capture stage:** all three RMII inputs (`rmii_crs_dv`, `rmii_rxd`,
  `rmii_rx_er`) are captured in **IOB flip-flops on `clk50` rising
  edges**, then pass through one more `clk50` register before use
  (2-flop pipeline; the PHY is frequency-locked to our own REFCLK, so
  this is skew tolerance, not true CDC).
  Parameter `RX_SAMPLE_FALLING = 1` moves the first capture flop to the
  **falling** edge of `clk50` (then resynchronized to rising) — the
  documented fallback if §5.3's input-timing analysis fails; default 0.
* **Reset (`rst` high):** `rmii_tx_en = 0`, `rmii_txd = 00`,
  `mii_rx_dv = 0`, `mii_rx_er = 0`, RX FSM to idle, `clk25_r = 0`.
  `mii_tx_clk`/`mii_rx_clk` keep running through reset (EmacLite wants
  live PHY clocks).
* **Malformed input handling:** if CRS_DV drops mid-pair per rule §4.5.5,
  the partial nibble is discarded silently. If CRS_DV asserts for < 1
  pair, nothing reaches the MAC. The shim never hangs: every state has a
  CRS_DV-low exit.

### 4.7 Self-checking simulation (deliverable with the RTL)

`hw/rtl/rmii_shim_tb.v` (xsim, hooks into the existing `make sim`):
back-to-back shim pair (MII→RMII→MII) plus directed tests — preamble
alignment with 0–3 leading `00` dibits, CRS_DV end-of-frame toggling,
RX_ER injection, min (64 B) and max (1518 B) frames. Exit nonzero on
mismatch, per repo CI convention.

---

## 5. Pins and XDC (Implementer A)

### 5.1 Pin table (verified against the Digilent Nexys A7 master XDC — frozen)

| Signal (port) | Package pin | Dir | Standard | Notes |
|---|---|---|---|---|
| `eth_mdc` | C9 | out | LVCMOS33 | management clock |
| `eth_mdio` | A9 | inout | LVCMOS33 | IOBUF in top wrapper |
| `eth_rstn` | B3 | out | LVCMOS33 | active-low; GPIO-driven |
| `eth_crsdv` | D9 | in | LVCMOS33 | RMII CRS_DV |
| `eth_rxerr` | C10 | in | LVCMOS33 | RMII RX_ER |
| `eth_rxd[0]` | C11 | in | LVCMOS33 | |
| `eth_rxd[1]` | D10 | in | LVCMOS33 | |
| `eth_txen` | B9 | out | LVCMOS33 | |
| `eth_txd[0]` | A10 | out | LVCMOS33 | |
| `eth_txd[1]` | A8 | out | LVCMOS33 | |
| `eth_refclk` | D5 | out | LVCMOS33 | 50 MHz, ODDR-forwarded |
| `eth_intn` | B8 | in | LVCMOS33 | unused v1 |

### 5.2 XDC additions (`hw/constraints/tricorder-nexys-a7-100t.xdc`)

```tcl
## Ethernet — LAN8720A RMII (FPGA sources the 50 MHz REFCLK)
set_property -dict {PACKAGE_PIN C9  IOSTANDARD LVCMOS33} [get_ports eth_mdc]
set_property -dict {PACKAGE_PIN A9  IOSTANDARD LVCMOS33} [get_ports eth_mdio]
set_property -dict {PACKAGE_PIN B3  IOSTANDARD LVCMOS33} [get_ports eth_rstn]
set_property -dict {PACKAGE_PIN D9  IOSTANDARD LVCMOS33} [get_ports eth_crsdv]
set_property -dict {PACKAGE_PIN C10 IOSTANDARD LVCMOS33} [get_ports eth_rxerr]
set_property -dict {PACKAGE_PIN C11 IOSTANDARD LVCMOS33} [get_ports {eth_rxd[0]}]
set_property -dict {PACKAGE_PIN D10 IOSTANDARD LVCMOS33} [get_ports {eth_rxd[1]}]
set_property -dict {PACKAGE_PIN B9  IOSTANDARD LVCMOS33} [get_ports eth_txen]
set_property -dict {PACKAGE_PIN A10 IOSTANDARD LVCMOS33} [get_ports {eth_txd[0]}]
set_property -dict {PACKAGE_PIN A8  IOSTANDARD LVCMOS33} [get_ports {eth_txd[1]}]
set_property -dict {PACKAGE_PIN D5  IOSTANDARD LVCMOS33 SLEW FAST} [get_ports eth_refclk]
set_property -dict {PACKAGE_PIN B8  IOSTANDARD LVCMOS33} [get_ports eth_intn]
```

Remember to keep `validate_xdc.tcl -tclargs nexys-a7` passing: the port
list grows by the 12 ports above (58 → 70).

### 5.3 Timing constraints

```tcl
## 25 MHz MII clock generated in the shim by divide-by-2
create_generated_clock -name mii_clk25 -divide_by 2 \
    -source [get_pins */rmii_shim_i/clk25_r_reg/C] \
    [get_pins */rmii_shim_i/clk25_r_reg/Q]

## RMII inputs: PHY launches on the REFCLK it receives back from D5.
## LAN8720A tco 2–14 ns; the REFCLK forwarding path (ODDR + trace + PHY
## input) adds a few ns more, so arrivals can approach one full 20 ns
## period — data is then captured one clk50 cycle late, which is a
## uniform pipeline delay and functionally harmless.
set_input_delay -clock [get_clocks -of_objects [get_pins */clk_wiz_*/clk_out_50]] \
    -max 14.0 [get_ports {eth_crsdv eth_rxerr eth_rxd[*]}]
set_input_delay -clock [get_clocks -of_objects [get_pins */clk_wiz_*/clk_out_50]] \
    -min  2.0 [get_ports {eth_crsdv eth_rxerr eth_rxd[*]}]

## RMII outputs: LAN8720A needs 4 ns setup / 2 ns hold at the PHY.
set_output_delay -clock [get_clocks -of_objects [get_pins */clk_wiz_*/clk_out_50]] \
    -max 4.0 [get_ports {eth_txen eth_txd[*]}]
set_output_delay -clock [get_clocks -of_objects [get_pins */clk_wiz_*/clk_out_50]] \
    -min -2.0 [get_ports {eth_txen eth_txd[*]}]
```

If RX setup fails timing or G2 (§10) shows corrupt loopback data, set
`RX_SAMPLE_FALLING = 1` (§4.6) and adjust the input delays to
`-clock_fall`. This is the *only* sanctioned knob; do not add MMCM phase
shifts in v1.

---

## 6. EmacLite programming model (Implementer B)

Register offsets from base `0x40E00000`, per PG135 (axi_ethernetlite
v3.0), **ping-pong disabled** so only the ping halves exist. All access
is 32-bit AXI reads/writes.

### 6.1 Register map (frozen)

| Offset | Name | Function |
|---|---|---|
| `0x0000`–`0x05FF` | **TX buffer** | Frame bytes, dest MAC first. (Buffer region extends to `0x07E0`; a max frame needs `0x5EE`.) |
| `0x07E4` | **MDIOADDR** | bit 10 = OP (1 = read, 0 = write), bits [9:5] = PHY address, bits [4:0] = register address |
| `0x07E8` | **MDIOWR** | bits [15:0] = write data |
| `0x07EC` | **MDIORD** | bits [15:0] = read data (valid when status clears) |
| `0x07F0` | **MDIOCTRL** | bit 3 = MDIO enable (set once at init), bit 0 = status/go (write 1 to start, polls to 0 when done) |
| `0x07F4` | **TX_LEN** | bits [15:0] = frame length in bytes (no FCS) |
| `0x07F8` | **GIE** | bit 31 = global interrupt enable — leave 0 (polled) |
| `0x07FC` | **TX_CTRL** | bit 0 = status (1 = busy/go), bit 1 = program MAC, bit 3 = internal loopback (leave 0), bit 4 = int enable (leave 0) |
| `0x1000`–`0x15FF` | **RX buffer** | Received frame, dest MAC first |
| `0x17FC` | **RX_CTRL** | bit 0 = status (1 = frame ready; write bit back to 0 to release the buffer), bit 3 = int enable (leave 0) |

Pong registers (`0x0800`, `0x0FF4`, `0x0FFC`, `0x1800`, `0x1FFC`) do not
exist in this configuration and must never be touched.

**Verification note:** these offsets are stated from memory of PG135.
First implementation step for B: confirm against the PG135 copy in the
Vivado 2025.2 install (`docs/pg135*` under the IP directory) and against
`xemaclite_l.h` in the Vitis embeddedsw tree, and correct this table in a
spec-revision commit if any offset differs. The software must use named
constants in `net.c`, so a correction is a one-line change.

### 6.2 Buffer byte order

The buffers are natural byte-addressable memory behind 32-bit AXI:
**frame byte N lives at buffer_base + N, with byte 0 in bits [7:0] of
word 0** (little-endian lane mapping, matching classic MicroBlaze on AXI
and the Xilinx driver's `XEmacLite_AlignedWrite`). Build frames in a
`u8[]` in DDR, then copy with word writes:

```c
static void buf_copy_to(volatile u32 *dst, const u8 *src, u32 len);   /* word writes */
static void buf_copy_from(u8 *dst, const volatile u32 *src, u32 len); /* word reads  */
```

Only 32-bit accesses are permitted on the buffers (no byte enables in
EmacLite); round `len` up to a word.

### 6.3 Operational sequences (frozen behavior)

**Program the station MAC** (once, in `net_init`, before PHY release):
1. Write the 6 MAC bytes `02 18 3E CF 44 01` to TX buffer offsets 0–5.
2. Write `TX_CTRL = 0x3` (Program | Status).
3. Poll `TX_CTRL` bit 0 until clear. The core now filters RX to this
   unicast MAC + broadcast — exactly what ARP/ICMP/telemetry need.

**Transmit:**
1. Poll `TX_CTRL` bit 0 == 0 (previous TX done). Timeout 10 ms → return
   error (should never happen at 100 Mb).
2. Copy frame to TX buffer (§6.2). Frames **must be ≥ 60 bytes**; pad
   with `0x00` in software — do not rely on the core to pad.
3. `TX_LEN = length` (padded length, ≥ 60, ≤ 1514; excludes FCS — the
   core computes and appends FCS).
4. `TX_CTRL = 0x1`. Frame is on the wire when bit 0 clears.

**Receive:**
1. Read `RX_CTRL`; if bit 0 == 0, no frame — return.
2. Copy the frame header from the RX buffer. EmacLite gives **no length
   register**; derive length from content (it has already validated FCS
   and discarded bad frames):
   * EtherType `0x0806` (ARP): frame is 14 + 28 = 42 bytes.
   * EtherType `0x0800` (IPv4): frame is 14 + IP total-length
     (bytes 16–17), clamp to ≤ 1514.
   * Anything else: ignore.
3. Process (§8.4), then release the buffer: write `RX_CTRL` with bit 0
   cleared (write `0x0`).

**MDIO read** (`mdio_read(phy, reg)`):
1. (Init-time, once) `MDIOCTRL = 0x8` (enable).
2. `MDIOADDR = (1<<10) | (phy<<5) | reg`.
3. `MDIOCTRL = 0x9` (enable | go); poll bit 0 → 0 (timeout 1 ms).
4. Return `MDIORD & 0xFFFF`.

**MDIO write** (`mdio_write(phy, reg, val)`): as above with OP = 0 and
`MDIOWR = val` before the go. MDC frequency is derived by the core from
the 100 MHz AXI clock and is compliant (< 2.5 MHz); no setup needed
beyond the enable bit.

---

## 7. Frame formats — byte-exact (Implementer B; C uses these as oracles)

Network constants (frozen):

```
OUR_MAC = 02:18:3E:CF:44:01          OUR_IP  = 192.168.1.44  (C0 A8 01 2C)
BCAST_MAC = FF:FF:FF:FF:FF:FF        BCAST_IP = 255.255.255.255
UDP_PORT = 5001 (0x1389), both src and dst
```

All multi-byte protocol fields are big-endian (network order) on the
wire. Offsets below are from frame byte 0 (first byte of destination
MAC). FCS is never counted — EmacLite adds/strips-checks it.

### 7.1 ARP reply (sent in response to a matching request)

Accept an RX frame iff: EtherType `0x0806`; HTYPE=1, PTYPE=0x0800,
HLEN=6, PLEN=4, OPER=1 (request); **TPA == 192.168.1.44**. Everything
else: drop silently.

Reply (42 bytes built, **padded to 60** with zeros):

| Off | Len | Field | Value |
|---|---|---|---|
| 0 | 6 | Eth dst | requester's SHA (bytes 22–27 of the request) |
| 6 | 6 | Eth src | `02 18 3E CF 44 01` |
| 12 | 2 | EtherType | `08 06` |
| 14 | 2 | HTYPE | `00 01` |
| 16 | 2 | PTYPE | `08 00` |
| 18 | 1 | HLEN | `06` |
| 19 | 1 | PLEN | `04` |
| 20 | 2 | OPER | `00 02` (reply) |
| 22 | 6 | SHA | `02 18 3E CF 44 01` |
| 28 | 4 | SPA | `C0 A8 01 2C` |
| 32 | 6 | THA | requester's SHA |
| 38 | 4 | TPA | requester's SPA |
| 42 | 18 | pad | `00` × 18 |

Optionally (SHOULD): one gratuitous ARP request (OPER=1, SHA=THA-target
ourselves, dst broadcast) immediately after link-up, to prime switch/ARP
tables.

### 7.2 ICMP echo reply

Accept iff: EtherType `0x0800`; IP version 4, **IHL == 5** (drop frames
with IP options — ping never uses them); dst IP == 192.168.1.44;
protocol == 1; ICMP type 8, code 0. Verify nothing else (FCS already
verified by hardware; checksum verification of the request is optional
and NOT required in v1).

Reply — same total length as the request:

| Off | Len | Field | Value |
|---|---|---|---|
| 0 | 6 | Eth dst | request's Eth src |
| 6 | 6 | Eth src | OUR_MAC |
| 12 | 2 | EtherType | `08 00` |
| 14 | 1 | Ver/IHL | `45` |
| 15 | 1 | TOS | `00` |
| 16 | 2 | Total length | copied from request |
| 18 | 2 | ID | copied from request |
| 20 | 2 | Flags/frag | `00 00` |
| 22 | 1 | TTL | `40` (64) |
| 23 | 1 | Protocol | `01` |
| 24 | 2 | Header checksum | recomputed (§7.4) over bytes 14–33 |
| 26 | 4 | Src IP | `C0 A8 01 2C` |
| 30 | 4 | Dst IP | request's src IP |
| 34 | 1 | ICMP type | `00` |
| 35 | 1 | ICMP code | `00` |
| 36 | 2 | ICMP checksum | recomputed (§7.4) over ICMP part, length = total_length − 20 |
| 38 | … | ICMP rest | ID, seq, payload copied verbatim from request |

Pad to 60 if total frame < 60 (a default `ping` is 98 bytes, so padding
only triggers for `ping -s` small sizes).

### 7.3 UDP telemetry broadcast (1 Hz)

| Off | Len | Field | Value |
|---|---|---|---|
| 0 | 6 | Eth dst | `FF FF FF FF FF FF` |
| 6 | 6 | Eth src | OUR_MAC |
| 12 | 2 | EtherType | `08 00` |
| 14 | 1 | Ver/IHL | `45` |
| 15 | 1 | TOS | `00` |
| 16 | 2 | Total length | 28 + payload_len |
| 18 | 2 | ID | incrementing u16, starts 0 |
| 20 | 2 | Flags/frag | `00 00` |
| 22 | 1 | TTL | `40` |
| 23 | 1 | Protocol | `11` (17, UDP) |
| 24 | 2 | Header checksum | computed (§7.4) |
| 26 | 4 | Src IP | `C0 A8 01 2C` |
| 30 | 4 | Dst IP | `FF FF FF FF` |
| 34 | 2 | UDP src port | `13 89` (5001) |
| 36 | 2 | UDP dst port | `13 89` (5001) |
| 38 | 2 | UDP length | 8 + payload_len |
| 40 | 2 | UDP checksum | `00 00` (legal for IPv4 UDP; **no pseudo-header math in v1**) |
| 42 | n | payload | §7.3.1 |

#### 7.3.1 Telemetry payload (frozen keys, order, and formats)

ASCII, lines terminated `\n` (0x0A, no CR), `key=value`, no spaces,
values printed with `xil_printf`-compatible decimal formatting (signed
where noted). Exactly these keys in exactly this order:

```
seq=<u32>        packet sequence number, starts 0, wraps
uptime=<u32>     seconds since boot: xTaskGetTickCount()/configTICK_RATE_HZ
temp_cc=<i32>    ui_live.temp_centi   (ADT7420, centi-degC)
i2c=<i32>        ui_live.i2c_devices  (external bus scan count)
rm=<i32>         ui_live.cur_rm       (-1 swapping, 0 PDM, 1 SONAR, 2 B, 3 A)
swaps=<u32>      ui_live.swaps        (completed DFX swaps)
swap_us=<u32>    ui_live.last_swap_us (last swap duration, microseconds)
pcm=<i32>        ui_live.pcm          (last PDM PCM sample)
energy=<u32>     ui_live.energy       (last PDM energy window)
peak=<u32>       ui_live.peak         (last sonar peak)
```

Example datagram payload (what `nc -ul 5001` displays):

```
seq=42
uptime=42
temp_cc=3128
i2c=0
rm=3
swaps=7
swap_us=1042
pcm=-4326
energy=5678
peak=0
```

Payload is ≤ ~130 bytes; total frame ≤ ~172 bytes — always single,
never fragmented. New keys may be **appended** later without a spec rev;
existing keys/order are frozen (host-side parsers may rely on them).

### 7.4 Internet checksum (one algorithm, used for IP header and ICMP)

Standard RFC 1071: sum 16-bit big-endian words (pad odd trailing byte
with 0x00 low byte), fold carries (`sum = (sum & 0xFFFF) + (sum >> 16)`
until no carry), complement, store big-endian. Compute with the checksum
field zeroed. One shared helper:

```c
static u16 inet_csum(const u8 *p, u32 len);   /* returns value in host order;
                                                 store as p[hi]=v>>8, p[lo]=v */
```

---

## 8. Software module `net.c` / `net.h` (Implementer B)

Location: `sw/tricorder-rtos/src/net.c`, `net.h`. Style: raw MMIO
`#define` register access like `main.c`'s SPI/DFX code; no BSP driver,
no dynamic allocation, no printf in the datapath (UART only for the §10.3
evidence lines and errors).

### 8.1 Public API (frozen)

```c
/* net.h */
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
#endif
```

### 8.2 Concurrency contract

Single-threaded by design: **all four net functions are called from one
FreeRTOS task only** (`net_task`, §8.5). No locks. `ui_live` fields are
single-writer volatiles (existing convention); `net_send_telemetry` only
reads them — a torn read across fields is acceptable for telemetry.
`net.c` owns the EmacLite exclusively; nobody else touches `0x40E0xxxx`
or `0x40E1xxxx`.

### 8.3 MMIO constants (must match §3.2/§6.1 exactly)

```c
#define EMAC_BASE    0x40E00000u
#define EMAC_TXBUF   (EMAC_BASE + 0x0000)
#define EMAC_MDIOADDR (EMAC_BASE + 0x07E4)
#define EMAC_MDIOWR  (EMAC_BASE + 0x07E8)
#define EMAC_MDIORD  (EMAC_BASE + 0x07EC)
#define EMAC_MDIOCTRL (EMAC_BASE + 0x07F0)
#define EMAC_TXLEN   (EMAC_BASE + 0x07F4)
#define EMAC_GIE     (EMAC_BASE + 0x07F8)
#define EMAC_TXCTRL  (EMAC_BASE + 0x07FC)
#define EMAC_RXBUF   (EMAC_BASE + 0x1000)
#define EMAC_RXCTRL  (EMAC_BASE + 0x17FC)
#define ETH_RSTN_GPIO 0x40E10000u    /* axi_gpio_eth ch1 data reg, bit0 */
```

### 8.4 `net_poll` dispatch

For each ready RX frame (max 8 per call, then yield):

```
ethertype 0x0806 → ARP:  match per §7.1 → build reply → emac_send
ethertype 0x0800 → IPv4: dst==OUR_IP && IHL==5 && proto==1 && type==8
                          → echo reply per §7.2 → emac_send
anything else    → drop (count in a stats struct: rx_frames, rx_arp,
                   rx_icmp, rx_dropped, tx_frames — for debugging)
```

Once per second (tick-derived), refresh the cached BMSR link bit and, on
a 0→1 link transition, emit `NET: link up` and send the gratuitous ARP
(§7.1). Frame scratch buffers are two static 1536-byte `u8` arrays (one
RX, one TX) in `net.c` — no stack frames of that size.

### 8.5 Integration into `main.c` (small, mechanical)

```c
static void net_task(void *arg)
{
    (void)arg;
    int up = net_init();                     /* prints §10.3 evidence lines */
    (void)up;                                /* run on even if loopback failed:
                                                cable tests may still work */
    unsigned n = 0;
    for (;;) {
        net_poll();
        if (++n >= 100) { n = 0; net_send_telemetry(); }  /* 1 Hz */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
/* in main(): */
xTaskCreate(net_task, "net", 1024, NULL, tskIDLE_PRIORITY + 1, NULL);
```

Priority +1 (same as heartbeat/ui, **below** sensors and dfx): telemetry
must never delay a personality swap. At 1 Hz/172 B the load is
negligible. No changes to `ui.h`/`ui.c` are needed — all payload fields
already exist.

---

## 9. PHY bring-up sequence (inside `net_init`)

LAN8720A on the Nexys A7; the FPGA's REFCLK must be running before and
during reset (it is — clk_wiz output, alive from configuration).
Expected PHY address **1** (Digilent strap), but step 3 scans to be
robust.

1. **MAC program** (§6.3) — do this first, while the PHY is still held
   in reset by the GPIO default.
2. **Reset pulse:** `ETH_RSTN = 0` (already 0 from config); hold ≥ 25 ms
   (`vTaskDelay`; datasheet minimum is 100 µs — margin is free). Then
   `ETH_RSTN = 1`; wait another 25 ms for strap latch + internal init
   before the first MDIO transaction.
3. **MDIO enable + ID check:** set MDIOCTRL enable. Read registers 2/3
   (PHY ID1/ID2) at address 1; if ID1 != `0x0007`, scan addresses 0–31.
   Pass criteria: **ID1 == 0x0007 and (ID2 & 0xFFF0) == 0xC0F0**
   (SMSC LAN8720A). Print evidence line E1. Fail → return −1
   (this is gate **G1**).
4. **Loopback self-test** (no cable needed — proves MAC↔shim↔PHY both
   directions):
   a. `mdio_write(phy, 0, 0x6100)` — BMCR: Loopback | 100 Mb | Full
      duplex, autoneg disabled. Wait 10 ms.
   b. Build a 60-byte test frame (64 on the wire with FCS): dst = OUR_MAC, src = OUR_MAC,
      EtherType `0x88B5` (IEEE local-experimental), payload bytes
      `i & 0xFF` for i = 0…45. Transmit via §6.3.
   c. Poll RX up to 100 ms. Pass: a frame arrives and bytes 0…59 match
      what was sent. Retry once on failure (first-frame flakiness after
      loopback entry is known PHY behavior).
   d. Print evidence line E2. This is gate **G2**; on failure continue
      to step 5 anyway (a cable-attached session can still diagnose).
5. **Normal mode:**
   a. `mdio_write(phy, 4, 0x0181)` — ANAR: 100BASE-TX FD + 100BASE-TX HD
      + IEEE 802.3 selector. **Deliberately no 10 Mb bits** — the shim
      is 100 Mb only, so 10 Mb must be un-advertised, not merely
      unpreferred.
   b. `mdio_write(phy, 0, 0x1200)` — BMCR: autoneg enable | restart.
   c. Do **not** wait for link here; `net_poll` watches BMSR bit 2 and
      announces link-up asynchronously (§8.4).
6. Return `NET_UP_MDIO | NET_UP_LOOP` bits as earned.

---

## 10. Test plan (Implementer C)

### 10.1 Simulation gates (pre-hardware, CI `make sim`)

| Gate | What | Pass |
|---|---|---|
| S1 | `rmii_shim_tb`: MII→RMII→MII loop, 64 B and 1518 B frames | byte-exact, exit 0 |
| S2 | Preamble alignment with 0–3 leading `00` dibits; CRS_DV end-toggle | frame recovered intact |
| S3 | RX_ER injection mid-frame | `mii_rx_er` asserted during that nibble |

### 10.2 No-cable gates (bench, board only — the machine gate)

Run the RTOS build, watch `/dev/ttyUSB1`:

| Gate | Proves | Pass criteria |
|---|---|---|
| G1 | MDC/MDIO pins, EmacLite MDIO engine, PHY alive after reset sequencing | Evidence line E1 with ID1 `0007`, ID2 `c0fx` |
| G2 | Full TX datapath (EmacLite → shim serialize → PHY) **and** full RX datapath (PHY → CRS_DV framing → shim nibble assembly → EmacLite address filter + FCS check) | Evidence line E2 `OK`; 60/60 bytes match |
| G3 | Non-interference | Existing behavior unchanged with net_task running: 7-seg UI responsive, DFX swap via SW0–SW3 still passes signature check, heartbeat ticks don't stutter over 10 min |

G1+G2 green = hardware and shim are **done and provable without any
cable**; remaining risk is confined to the RJ45/magnetics/autoneg, which
no shim bug can cause.

### 10.3 UART evidence strings (frozen — A and B must not alter)

```
E1: "NET: PHY id %04x %04x @%u\r\n"     (id1, id2, phy_addr)
E2: "NET: loopback %u/%u bytes %s\r\n"  (matched, 60, "OK"/"FAIL")
E3: "NET: link up\r\n"
E4: "NET: telemetry seq=%u\r\n"          (printed every 60th packet only)
```

### 10.4 With-cable gates (later bench session, host on 192.168.1.0/24)

| Gate | Command (host) | Pass criteria |
|---|---|---|
| C1 link | — | E3 within 5 s of cable insert; BMSR shows 100FD (partner reg 5 has 0x0100) |
| C2 ping | `ping -c 100 192.168.1.44` | 0% loss, RTT < 2 ms; `arp -a` shows `02:18:3e:cf:44:01` |
| C3 telemetry | `nc -ul 5001` | one §7.3.1 block per second; `seq` strictly increments; values agree with the 7-seg readouts (temp within 0.5 °C, swaps exact) |
| C4 correctness | `sudo tcpdump -xx -i <if> host 192.168.1.44` | ARP reply and ICMP reply byte-fields match §7.1/§7.2 tables; no runts, no bad-FCS counts on the host NIC |
| C5 stress | `ping -f -c 10000 192.168.1.44` (flood) concurrent with `nc -ul 5001` | < 1% flood loss (polled 10 ms loop *will* drop some — that is acceptable and expected), telemetry cadence stays 1 Hz ± 0.1, no RTOS stall, UI responsive |
| C6 soak | 1 h: 1 Hz ping + telemetry capture + one DFX swap per minute (scripted via existing switch procedure or left to operator) | zero missed telemetry seconds > 2 s gap, board never resets, swap signature never mismatches |
| C7 standalone | JP1 = QSPI boot, no JTAG/USB host, power cycle | C2 + C3 pass identically from flash boot |

C7 requires rebuilding the flash image (`tools/mkbootimg.py` flow per
plan §5) with the new bitstream + ELF; no new mechanism.

### 10.5 Failure triage map

| Symptom | First suspect | Tool |
|---|---|---|
| G1 fails, reads 0xFFFF | RSTN/GPIO wiring, MDIO IOBUF T polarity | scope C9/A9 |
| G1 fails, reads 0x0000 | MDIO stuck low / wrong offsets (§6.1 verify note) | re-check PG135 |
| G2 TX never completes | TX_CTRL busy stuck: shim mii_tx_clk dead → check clk25 BUFG + generated-clock constraint | ILA recipe `hw/scripts/ila_capture.tcl` |
| G2 no RX frame | CRS_DV framing or nibble phase: capture rmii_crs_dv/rxd + mii_rx_dv/rxd with ILA; try `RX_SAMPLE_FALLING=1` | ILA |
| G2 RX data corrupt | dibit order (bit0-first!, §4.2) or pair alignment | ILA + S2 sim |
| C2 fails but G2 passed | magnetics/cable/autoneg only — read PHY reg 5, reg 31 (LAN8720A special modes/status shows actual speed) | mdio_read via debug UART print |

---

## 11. Deliverables checklist

| Who | Files |
|---|---|
| A | `hw/rtl/rmii_shim.v`, `hw/rtl/rmii_shim_tb.v`, `tricorder_top_nexys.v` edits, BD Tcl regen (`hw/bd/static_region.tcl`), XDC edits (§5), timing report clean |
| B | `sw/tricorder-rtos/src/net.c`, `net.h`, `main.c` edits (§8.5) |
| C | `make sim` hookup for the shim TB, bench log of G1–G3 into `docs/notebook.md` (5-section format, per repo convention), later C1–C7 |

Notebook entries and `docs/prompts.md` logging follow the standing repo
conventions; George pushes.
