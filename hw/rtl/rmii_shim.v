// MII <-> RMII shim for the Nexys A7-100T on-board LAN8720A PHY.
//
// Sits between the block design's external axi_ethernetlite MII port and
// the RMII pins, per docs/ethernet-spec.md section 4. 100 Mbps only: MII
// moves one nibble per 25 MHz cycle, RMII moves one dibit per 50 MHz
// cycle - a pure width/clock converter with one nibble of elasticity and
// no FIFOs. Bit order is LSB-first on the wire everywhere: an MII nibble
// n[3:0] is two consecutive RMII dibits, first dibit -> n[1:0], second
// dibit -> n[3:2].
//
// Everything runs in the one 50 MHz domain (clk50, the same clock the top
// wrapper forwards to the PHY as REFCLK through an ODDR). The 25 MHz MII
// clock is a divide-by-2 register (clk25_r) driven out through a BUFG, so
// all shim<->EmacLite paths are synchronous generated-clock paths - no
// true CDC. The XDC declares the matching create_generated_clock on
// clk25_r_reg (spec section 5.3). Phase bookkeeping uses clk25_r itself:
//   clk25_r == 0 in a clk50 cycle => mii clocks RISE at the end of it;
//   clk25_r == 1 in a clk50 cycle => mii clocks FALL at the end of it.
//
// RX recovers MII RX_DV semantics from RMII CRS_DV framing: hunt past
// leading 00 dibits, lock dibit-pair phase on the first non-00 dibit
// (first preamble dibit), and treat a pair with CRS_DV low in BOTH cycles
// as end of frame (the PHY may toggle CRS_DV at 25 MHz near frame end).
// RX_SAMPLE_FALLING = 1 moves the RMII input capture flop to the falling
// edge of clk50 - the sanctioned timing fallback of spec section 4.6.

`timescale 1ns / 1ps

module rmii_shim #(
    parameter RX_SAMPLE_FALLING = 0,  // spec 4.6 timing fallback, default off
    parameter TX_LOOPBACK       = 0   // bring-up bisect: fold TX back into RX
)(
    input  wire        clk50,        // clk_wiz clk_out_50; same clock the
                                     // ODDR forwards to the PHY as REFCLK
    input  wire        rst,          // active-high, synchronous to clk50

    // ---- MII side: toward axi_ethernetlite (all 25 MHz timing) ----
    output wire        mii_tx_clk,   // 25 MHz, BUFG-driven
    output wire        mii_rx_clk,   // same 25 MHz net as mii_tx_clk
    (* mark_debug = "true" *) input  wire        mii_tx_en,    // from EmacLite, launched on mii_tx_clk^
    input  wire [3:0]  mii_txd,      //   nibble, bit0 = first bit on wire
    (* mark_debug = "true" *) output reg         mii_rx_dv,    // to EmacLite, sampled on mii_rx_clk^
    output reg  [3:0]  mii_rxd,
    output reg         mii_rx_er,
    output wire        mii_crs,      // = rx frame activity
    output wire        mii_col,      // constant 1'b0 (full duplex only)

    // ---- RMII side: toward LAN8720A (all 50 MHz timing) ----
    (* IOB = "TRUE", mark_debug = "true" *) output reg        rmii_tx_en,
    (* IOB = "TRUE", mark_debug = "true" *) output reg [1:0]  rmii_txd,  // bit0 = first bit on wire
    input  wire        rmii_crs_dv,
    input  wire [1:0]  rmii_rxd,
    input  wire        rmii_rx_er
);

    // ------------------------------------------------------------------
    // 25 MHz MII clock: divide-by-2 register through a BUFG. Toggles
    // unconditionally so the MII clocks keep running through reset
    // (EmacLite wants live PHY clocks); initial 0 covers configuration.
    // ------------------------------------------------------------------
    reg clk25_r = 1'b0;

    always @(posedge clk50)
        clk25_r <= ~clk25_r;

    wire clk25_bufg;
    BUFG mii_clk_bufg (.I(clk25_r), .O(clk25_bufg));

    assign mii_tx_clk = clk25_bufg;
    assign mii_rx_clk = clk25_bufg;

    assign mii_col = 1'b0;

    // ------------------------------------------------------------------
    // TX path (MAC -> PHY). EmacLite launches mii_tx_en/mii_txd on the
    // mii_tx_clk rising edge (end of a clk25_r==0 cycle); we sample one
    // clk50 cycle later, at the end of the clk25_r==1 cycle, giving the
    // data a half 25 MHz period to settle. The captured nibble then goes
    // out as two dibits on the next two clk50 edges: low half while
    // clk25_r==0 at the edge, high half while clk25_r==1. Output flops
    // are IOB-packed on clk50 rising edges; the fixed pipeline latency
    // is irrelevant.
    // ------------------------------------------------------------------
    reg       tx_en_s = 1'b0;   // sampled mii_tx_en
    reg [3:0] tx_nib  = 4'h0;   // sampled mii_txd

    always @(posedge clk50) begin
        if (rst) begin
            tx_en_s <= 1'b0;
        end else if (clk25_r) begin
            tx_en_s <= mii_tx_en;
            tx_nib  <= mii_txd;
        end
    end

    always @(posedge clk50) begin
        if (rst) begin
            rmii_tx_en <= 1'b0;
            rmii_txd   <= 2'b00;
        end else begin
            rmii_tx_en <= tx_en_s;
            if (!tx_en_s)
                rmii_txd <= 2'b00;
            else if (!clk25_r)
                rmii_txd <= tx_nib[1:0];   // first dibit on the wire
            else
                rmii_txd <= tx_nib[3:2];   // second dibit
        end
    end

    // ------------------------------------------------------------------
    // RMII input capture: IOB flop on clk50 (rising edge by default,
    // falling edge when RX_SAMPLE_FALLING=1), then one more clk50 rising
    // register before use. The PHY is frequency-locked to our own REFCLK,
    // so this 2-flop pipeline is skew tolerance, not true CDC.
    // ------------------------------------------------------------------
    (* IOB = "TRUE" *) reg       crsdv_c = 1'b0;
    (* IOB = "TRUE" *) reg [1:0] rxd_c   = 2'b00;
    (* IOB = "TRUE" *) reg       rxer_c  = 1'b0;

    generate
        if (RX_SAMPLE_FALLING) begin : g_cap_fall
            always @(negedge clk50) begin
                crsdv_c <= rmii_crs_dv;
                rxd_c   <= rmii_rxd;
                rxer_c  <= rmii_rx_er;
            end
        end else if (TX_LOOPBACK) begin : g_cap_loop
            /* fabric loopback: the PHY is cut out of the path entirely */
            always @(posedge clk50) begin
                crsdv_c <= rmii_tx_en;
                rxd_c   <= rmii_txd;
                rxer_c  <= 1'b0;
            end
        end else begin : g_cap_rise
            always @(posedge clk50) begin
                crsdv_c <= rmii_crs_dv;
                rxd_c   <= rmii_rxd;
                rxer_c  <= rmii_rx_er;
            end
        end
    endgenerate

    (* mark_debug = "true" *) reg crsdv_q = 1'b0;
    (* mark_debug = "true" *) reg [1:0] rxd_q   = 2'b00;
    reg       rxer_q  = 1'b0;

    always @(posedge clk50) begin
        crsdv_q <= crsdv_c;
        rxd_q   <= rxd_c;
        rxer_q  <= rxer_c;
    end

    // ------------------------------------------------------------------
    // RX path (PHY -> MAC): CRS_DV framing -> MII RX_DV.
    //
    // Dibit-pair FSM (one dibit per clk50 cycle once locked):
    //   IDLE  - no carrier; CRS_DV high starts hunting.
    //   HUNT  - carrier seen, discard 00 dibits; the first non-00 dibit
    //           is the first preamble dibit and fixes pair phase (it is
    //           the LOW half of the first nibble).
    //   PAIR_LO - capture the low half of a nibble.
    //   PAIR_HI - capture the high half, evaluate the end-of-frame rule:
    //           CRS_DV high in either cycle of the pair => nibble valid;
    //           low in both => frame over, pair discarded (frames end on
    //           pair boundaries at 100 Mb, so nothing real is lost).
    //
    // Assembled nibbles land in a one-nibble elasticity register
    // (nib_valid/nib_data/nib_er). Both sides run at exactly the 25 MHz
    // nibble rate, so one nibble of skew absorption is sufficient by
    // construction. The MII outputs update only on clk50 edges where
    // clk25_r==1 - i.e. coincident with the mii_rx_clk falling edge - so
    // they are stable a full clk50 cycle plus setup before the next
    // mii_rx_clk rising edge, where EmacLite samples them.
    //
    // The consumer block precedes the producer FSM so that when both
    // touch nib_valid on the same edge (consume old nibble, produce new
    // one) the producer's nonblocking write wins and no nibble is lost.
    // ------------------------------------------------------------------
    localparam [1:0] RX_IDLE    = 2'd0,
                     RX_HUNT    = 2'd1,
                     RX_PAIR_LO = 2'd2,
                     RX_PAIR_HI = 2'd3;

    (* mark_debug = "true" *) reg [1:0] rx_state  = RX_IDLE;
    reg [1:0] nib_lo    = 2'b00;   // low half of the nibble in flight
    reg       er_lo     = 1'b0;    // RX_ER seen in the pair's first cycle
    reg       crsdv_lo  = 1'b0;    // CRS_DV seen in the pair's first cycle
    reg [3:0] nib_data  = 4'h0;    // elasticity register
    reg       nib_er    = 1'b0;
    (* mark_debug = "true" *) reg       nib_valid = 1'b0;
    reg       frame_end = 1'b0;    // deassert mii_rx_dv at next update slot

    always @(posedge clk50) begin
        if (rst) begin
            rx_state  <= RX_IDLE;
            nib_valid <= 1'b0;
            frame_end <= 1'b0;
            mii_rx_dv <= 1'b0;
            mii_rx_er <= 1'b0;
            mii_rxd   <= 4'h0;
        end else begin
            // MII output slot: just after the mii_rx_clk falling edge.
            if (clk25_r) begin
                if (nib_valid) begin
                    mii_rxd   <= nib_data;
                    mii_rx_dv <= 1'b1;
                    mii_rx_er <= nib_er;
                    nib_valid <= 1'b0;
                end else begin
                    mii_rx_er <= 1'b0;
                    if (frame_end) begin
                        mii_rx_dv <= 1'b0;
                        frame_end <= 1'b0;
                    end
                end
            end

            // Dibit-pair FSM: every clk50 cycle.
            case (rx_state)
                RX_IDLE, RX_HUNT: begin
                    if (crsdv_q && rxd_q != 2'b00) begin
                        nib_lo   <= rxd_q;      // first preamble dibit:
                        er_lo    <= rxer_q;     // low half of first nibble
                        crsdv_lo <= 1'b1;
                        rx_state <= RX_PAIR_HI;
                    end else if (crsdv_q) begin
                        rx_state <= RX_HUNT;    // discard leading 00 dibits
                    end else begin
                        rx_state <= RX_IDLE;
                    end
                end
                RX_PAIR_LO: begin
                    nib_lo   <= rxd_q;
                    er_lo    <= rxer_q;
                    crsdv_lo <= crsdv_q;
                    rx_state <= RX_PAIR_HI;
                end
                RX_PAIR_HI: begin
                    if (crsdv_lo || crsdv_q) begin
                        nib_data  <= {rxd_q, nib_lo};
                        nib_er    <= er_lo | rxer_q;
                        nib_valid <= 1'b1;
                        rx_state  <= RX_PAIR_LO;
                    end else begin
                        frame_end <= 1'b1;      // CRS_DV low both cycles:
                        rx_state  <= RX_IDLE;   // frame over, pair discarded
                    end
                end
            endcase
        end
    end

    // Carrier sense: from CRS_DV detection until end of frame on the MII
    // side. COL is tied 0 above - full duplex only.
    assign mii_crs = crsdv_q || (rx_state != RX_IDLE) || mii_rx_dv;

endmodule
