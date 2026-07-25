// Tricorder top level — port-complete stub for the Nexys A7-100T.
// Matches hw/constraints/tricorder-nexys-a7-100t.xdc one-for-one; used to
// validate the XDC (LOC legality, clock-capable pins) until the real
// static-region design replaces the stub body.
//
// Differences from the Arty top (docs/plan.md §2):
//  - no i2c_*_pup (Arty-only pull-up-enable hardware; resistors on harness)
//  - no cam_rst_n / cam_pwdn (strapped on module), no cam_sioc/siod
//    (SCCB rides the shared i2c bus)
//  - no adc_otr (rail-code detection in fabric), no afe_enable (strapped)
//  - AD9226 and AD9708 time-share one 12-bit bus through IOBUFs:
//    adcdac_data + adcdac_clk (ADC personality: 12 in; DAC: 8 out)

`timescale 1ns / 1ps

module tricorder_top_nexys (
    // Board infrastructure
    input  wire        clk100mhz,
    output wire        uart_rxd_out,
    input  wire        uart_txd_in,
    input  wire [3:0]  btn,
    input  wire [3:0]  sw,
    output wire [3:0]  led,
    output wire        dfx_status_r,
    output wire        dfx_status_g,
    output wire        dfx_status_b,

    // Shared sensor I2C bus (5 low-rate sensors + OV7670 SCCB), Pmod JA
    inout  wire        i2c_scl,
    inout  wire        i2c_sda,

    // SiPM front end (pulse oversampled in fabric — not a clock), Pmod JA
    input  wire        sipm_pulse,
    output wire        sipm_thresh_pwm,

    // AD8332 ultrasound AFE control, Pmod JA
    output wire        afe_gain_pwm,
    output wire        afe_hilo,

    // Piezo sonar TX, Pmod JA
    output wire        sonar_tx_a,
    output wire        sonar_tx_b,

    // OV7670 camera, Pmods JB + JC (PCLK on JB10/H16, sole P-side MRCC)
    input  wire        cam_pclk,
    output wire        cam_xclk,
    input  wire        cam_href,
    input  wire        cam_vsync,
    input  wire [7:0]  cam_data,

    // AD9226 / AD9708 shared bus, Pmods JC + JD, clk on JXADC
    inout  wire [11:0] adcdac_data,
    output wire        adcdac_clk,

    // PDM microphone array, JXADC used digitally
    output wire        pdm_clk,
    input  wire [5:0]  pdm_data
);

    // Stub: fold every input into a heartbeat so no port is pruned; drive
    // the shared bus from sw[3] so both IOBUF directions exist in the netlist.
    reg [26:0] beat = 27'd0;
    reg        pulse = 1'b0;

    always @(posedge clk100mhz) begin
        beat  <= beat + 27'd1;
        pulse <= ^{btn, sw, uart_txd_in, pdm_data, cam_href, cam_vsync,
                   cam_data, adcdac_data, sipm_pulse};
    end

    assign led             = {3'b000, beat[26] ^ pulse};
    assign uart_rxd_out    = 1'b1;
    assign dfx_status_r    = 1'b0;
    assign dfx_status_g    = 1'b0;
    assign dfx_status_b    = beat[26];

    assign i2c_scl         = 1'bz;
    assign i2c_sda         = 1'bz;

    assign pdm_clk         = beat[5];             // ~1.6 MHz placeholder
    assign cam_xclk        = beat[1];             // 25 MHz placeholder

    assign adcdac_data     = sw[3] ? {4'b0000, beat[10:3]} : 12'bz;
    assign adcdac_clk      = beat[1];

    assign sipm_thresh_pwm = beat[7];
    assign afe_gain_pwm    = beat[8];
    assign afe_hilo        = 1'b0;
    assign sonar_tx_a      = beat[11];
    assign sonar_tx_b      = ~beat[11];

endmodule
