// M5 DFX top: static_region BD wrapper + the reconfigurable rm_slot.
// rm_slot is a black box during static synthesis; the DFX flow fills it
// with rm_slot_a / rm_slot_b checkpoints.

`timescale 1ns / 1ps

module tricorder_m5_top (
    input  wire        clk100mhz,
    output wire        pdm_m_clk,
    input  wire        pdm_m_data,
    output wire        pdm_m_lrsel,
    output wire        sonar_tx_a,
    output wire        sonar_tx_b,
    output wire        afe_gain_pwm,
    output wire        afe_hilo,
    input  wire [11:0] adc_data,
    output wire        adc_clk,
    input  wire        cpu_resetn,
    input  wire        uart_rxd,
    output wire        uart_txd,
    inout  wire        iic_ext_scl_io,
    inout  wire        iic_ext_sda_io,
    inout  wire        iic_tmp_scl_io,
    inout  wire        iic_tmp_sda_io,
    output wire [3:0]  led_tri_o,
    // DDR2 (wired straight through to the MIG)
    output wire [12:0] ddr2_addr,
    output wire [2:0]  ddr2_ba,
    output wire        ddr2_cas_n,
    output wire [0:0]  ddr2_ck_n,
    output wire [0:0]  ddr2_ck_p,
    output wire [0:0]  ddr2_cke,
    output wire [0:0]  ddr2_cs_n,
    output wire [1:0]  ddr2_dm,
    inout  wire [15:0] ddr2_dq,
    inout  wire [1:0]  ddr2_dqs_n,
    inout  wire [1:0]  ddr2_dqs_p,
    output wire [0:0]  ddr2_odt,
    output wire        ddr2_ras_n,
    output wire        ddr2_we_n
);

    wire        rm_clk;
    wire [31:0] icapw_data;
    wire [0:0]  icapw_go;
    wire [31:0] icap_axis_tdata;
    wire        icap_axis_tvalid;
    wire [3:0]  icap_axis_tkeep;
    wire        icap_axis_tlast;
    wire [0:0]  rm_aresetn;
    wire [31:0] rm_awaddr, rm_wdata, rm_araddr, rm_rdata;
    wire [3:0]  rm_wstrb;
    wire [1:0]  rm_bresp, rm_rresp;
    wire        rm_awvalid, rm_awready, rm_wvalid, rm_wready;
    wire        rm_bvalid, rm_bready, rm_arvalid, rm_arready;
    wire        rm_rvalid, rm_rready;

    static_region_wrapper u_static (
        .clk100mhz    (clk100mhz),
        .cpu_resetn   (cpu_resetn),
        .uart_rxd     (uart_rxd),
        .uart_txd     (uart_txd),
        .iic_ext_scl_io (iic_ext_scl_io),
        .iic_ext_sda_io (iic_ext_sda_io),
        .iic_tmp_scl_io (iic_tmp_scl_io),
        .iic_tmp_sda_io (iic_tmp_sda_io),
        .led_tri_o    (led_tri_o),
        .ddr2_addr    (ddr2_addr),
        .ddr2_ba      (ddr2_ba),
        .ddr2_cas_n   (ddr2_cas_n),
        .ddr2_ck_n    (ddr2_ck_n),
        .ddr2_ck_p    (ddr2_ck_p),
        .ddr2_cke     (ddr2_cke),
        .ddr2_cs_n    (ddr2_cs_n),
        .ddr2_dm      (ddr2_dm),
        .ddr2_dq      (ddr2_dq),
        .ddr2_dqs_n   (ddr2_dqs_n),
        .ddr2_dqs_p   (ddr2_dqs_p),
        .ddr2_odt     (ddr2_odt),
        .ddr2_ras_n   (ddr2_ras_n),
        .ddr2_we_n    (ddr2_we_n),
        .icapw_data   (icapw_data),
        .icapw_go     (icapw_go),
        .icap_axis_tdata  (icap_axis_tdata),
        .icap_axis_tvalid (icap_axis_tvalid),
        .icap_axis_tready (1'b1),
        .icap_axis_tkeep  (icap_axis_tkeep),
        .icap_axis_tlast  (icap_axis_tlast),
        .rm_clk       (rm_clk),
        .rm_aresetn   (rm_aresetn),
        .rm_axi_awaddr (rm_awaddr),
        .rm_axi_awvalid(rm_awvalid),
        .rm_axi_awready(rm_awready),
        .rm_axi_wdata  (rm_wdata),
        .rm_axi_wstrb  (rm_wstrb),
        .rm_axi_wvalid (rm_wvalid),
        .rm_axi_wready (rm_wready),
        .rm_axi_bresp  (rm_bresp),
        .rm_axi_bvalid (rm_bvalid),
        .rm_axi_bready (rm_bready),
        .rm_axi_araddr (rm_araddr),
        .rm_axi_arvalid(rm_arvalid),
        .rm_axi_arready(rm_arready),
        .rm_axi_rdata  (rm_rdata),
        .rm_axi_rresp  (rm_rresp),
        .rm_axi_rvalid (rm_rvalid),
        .rm_axi_rready (rm_rready)
    );

    assign pdm_m_lrsel = 1'b0; // ADMP421: data valid on clk low

    // Raw ICAPE2 writer (second ICAP site; the HWICAP occupies the other).
    // One config word per go-toggle: CSIB low for exactly one 100 MHz cycle.
    // Bit ordering per UG470: bit-reverse within each byte for the ICAP I
    // port; software supplies plain 0xAA995566-style words.
    reg  [2:0]  icapw_sync = 3'b000;
    reg  [31:0] icapw_lat  = 32'h0;
    reg         icapw_csn  = 1'b1;

    always @(posedge rm_clk) begin
        icapw_sync <= {icapw_sync[1:0], icapw_go[0]};
        if (icapw_sync[2] ^ icapw_sync[1]) begin
            icapw_lat <= icapw_data;
            icapw_csn <= 1'b0;
        end else begin
            icapw_csn <= 1'b1;
        end
    end

    function [7:0] bitrev8(input [7:0] b);
        integer k;
        for (k = 0; k < 8; k = k + 1)
            bitrev8[k] = b[7 - k];
    endfunction

    wire [31:0] icapw_i = { bitrev8(icapw_lat[31:24]), bitrev8(icapw_lat[23:16]),
                            bitrev8(icapw_lat[15:8]),  bitrev8(icapw_lat[7:0]) };

    // AXIS fast path: one config word per beat. Full 32-bit reversal =
    // byteswap (file LE -> config BE) composed with UG470 per-byte bitrev,
    // so the DMA streams raw .bin words with no software preprocessing.
    reg  [31:0] axis_lat;
    reg         axis_csn = 1'b1;
    integer     m;
    always @(posedge rm_clk) begin
        axis_csn <= ~icap_axis_tvalid;
        for (m = 0; m < 32; m = m + 1)
            axis_lat[m] <= icap_axis_tdata[31 - m];
    end

    wire        icap_csn_mux = axis_csn & icapw_csn;
    wire [31:0] icap_i_mux   = icapw_csn ? axis_lat : icapw_i;

    ICAPE2 #(.ICAP_WIDTH("X32")) u_icap_writer (
        .CLK   (rm_clk),
        .CSIB  (icap_csn_mux),
        .RDWRB (1'b0),
        .I     (icap_i_mux),
        .O     ()
    );


    rm_slot u_rm (
        .clk           (rm_clk),
        .aresetn       (rm_aresetn[0]),
        .pdm_clk_o     (pdm_m_clk),
        .pdm_data_i    (pdm_m_data),
        .sonar_tx_a    (sonar_tx_a),
        .sonar_tx_b    (sonar_tx_b),
        .afe_gain_pwm  (afe_gain_pwm),
        .afe_hilo      (afe_hilo),
        .adc_data_i    (adc_data),
        .adc_clk_o     (adc_clk),
        .s_axi_awaddr  (rm_awaddr),
        .s_axi_awvalid (rm_awvalid),
        .s_axi_awready (rm_awready),
        .s_axi_wdata   (rm_wdata),
        .s_axi_wstrb   (rm_wstrb),
        .s_axi_wvalid  (rm_wvalid),
        .s_axi_wready  (rm_wready),
        .s_axi_bresp   (rm_bresp),
        .s_axi_bvalid  (rm_bvalid),
        .s_axi_bready  (rm_bready),
        .s_axi_araddr  (rm_araddr),
        .s_axi_arvalid (rm_arvalid),
        .s_axi_arready (rm_arready),
        .s_axi_rdata   (rm_rdata),
        .s_axi_rresp   (rm_rresp),
        .s_axi_rvalid  (rm_rvalid),
        .s_axi_rready  (rm_rready)
    );

endmodule
