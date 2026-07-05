// Tricorder top level — port-complete stub for the Arty A7-100T.
// Matches hw/constraints/tricorder-arty-a7-100t.xdc one-for-one; used to
// validate the XDC (LOC legality, clock-capable pins) until the real
// static-region design replaces the stub body.

`timescale 1ns / 1ps

module tricorder_top (
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

    // Low-rate sensor I2C bus
    inout  wire        i2c_scl,
    inout  wire        i2c_sda,
    output wire        i2c_scl_pup,
    output wire        i2c_sda_pup,

    // PDM microphone array
    output wire        pdm_clk,
    input  wire [5:0]  pdm_data,

    // OV7670 camera
    input  wire        cam_pclk,
    output wire        cam_xclk,
    input  wire        cam_href,
    input  wire        cam_vsync,
    output wire        cam_rst_n,
    output wire        cam_pwdn,
    output wire        cam_sioc,
    inout  wire        cam_siod,
    input  wire [7:0]  cam_data,

    // AD9226 ADC
    input  wire [11:0] adc_data,
    input  wire        adc_otr,
    output wire        adc_clk,

    // AD9708 DAC
    output wire [7:0]  dac_data,
    output wire        dac_clk,

    // SiPM front end
    input  wire        sipm_pulse,
    output wire        sipm_thresh_pwm,

    // AD8332 ultrasound AFE control
    output wire        afe_gain_pwm,
    output wire        afe_hilo,
    output wire        afe_enable,

    // Piezo sonar TX
    output wire        sonar_tx_a,
    output wire        sonar_tx_b
);

    // Stub: fold every input into a heartbeat so no port is pruned.
    reg [26:0] beat = 27'd0;
    reg        pulse = 1'b0;

    always @(posedge clk100mhz) begin
        beat  <= beat + 27'd1;
        pulse <= ^{btn, sw, uart_txd_in, pdm_data, cam_href, cam_vsync,
                   cam_data, adc_data, adc_otr, sipm_pulse};
    end

    assign led             = {3'b000, beat[26] ^ pulse};
    assign uart_rxd_out    = 1'b1;
    assign dfx_status_r    = 1'b0;
    assign dfx_status_g    = 1'b0;
    assign dfx_status_b    = beat[26];

    assign i2c_scl         = 1'bz;
    assign i2c_sda         = 1'bz;
    assign i2c_scl_pup     = 1'b1;
    assign i2c_sda_pup     = 1'b1;

    assign pdm_clk         = beat[5];       // ~1.6 MHz placeholder
    assign cam_xclk        = beat[1];       // 25 MHz placeholder
    assign cam_rst_n       = 1'b1;
    assign cam_pwdn        = 1'b0;
    assign cam_sioc        = 1'b1;
    assign cam_siod        = 1'bz;

    assign adc_clk         = beat[1];
    assign dac_data        = beat[10:3];
    assign dac_clk         = beat[2];
    assign sipm_thresh_pwm = beat[7];
    assign afe_gain_pwm    = beat[8];
    assign afe_hilo        = 1'b0;
    assign afe_enable      = 1'b0;
    assign sonar_tx_a      = beat[11];
    assign sonar_tx_b      = ~beat[11];

endmodule
