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
    input  wire [15:0] sw,
    output wire [7:0]  seg,
    output wire [7:0]  an,
    output wire        aud_pwm,
    output wire        aud_sd,
    input  wire        uart_rxd,
    output wire        uart_txd,
    inout  wire        iic_ext_scl_io,
    inout  wire        iic_ext_sda_io,
    inout  wire        iic_tmp_scl_io,
    inout  wire        iic_tmp_sda_io,
    inout  wire        qspi_dq0,
    inout  wire        qspi_dq1,
    inout  wire        qspi_dq2,
    inout  wire        qspi_dq3,
    inout  wire        qspi_cs,
    // qspi sck has no user pin: it drives the dedicated CCLK via STARTUPE2
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
    wire        qspi_sck;
    wire [31:0] disp_lo, disp_hi;
    wire [1:0]  audio_ctl;
    wire qspi_io0_i, qspi_io0_o, qspi_io0_t;
    wire qspi_io1_i, qspi_io1_o, qspi_io1_t;
    wire qspi_io2_i, qspi_io2_o, qspi_io2_t;
    wire qspi_io3_i, qspi_io3_o, qspi_io3_t;
    wire qspi_ss_i,  qspi_ss_o,  qspi_ss_t;
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
        .qspi_io0_i   (qspi_io0_i),
        .qspi_io0_o   (qspi_io0_o),
        .qspi_io0_t   (qspi_io0_t),
        .qspi_io1_i   (qspi_io1_i),
        .qspi_io1_o   (qspi_io1_o),
        .qspi_io1_t   (qspi_io1_t),
        .qspi_io2_i   (qspi_io2_i),
        .qspi_io2_o   (qspi_io2_o),
        .qspi_io2_t   (qspi_io2_t),
        .qspi_io3_i   (qspi_io3_i),
        .qspi_io3_o   (qspi_io3_o),
        .qspi_io3_t   (qspi_io3_t),
        .qspi_ss_i    (qspi_ss_i),
        .qspi_ss_o    (qspi_ss_o),
        .qspi_ss_t    (qspi_ss_t),
        .qspi_sck_o   (qspi_sck),
        .qspi_sck_i   (1'b0),
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
        .sw           (sw),
        .disp_lo      (disp_lo),
        .disp_hi      (disp_hi),
        .audio_ctl    (audio_ctl),
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

    sevenseg_scan u_sevenseg (
        .clk  (rm_clk),
        .disp ({disp_hi, disp_lo}),
        .seg  (seg),
        .an   (an)
    );

    audio_player u_audio (
        .clk     (rm_clk),
        .ctl     (audio_ctl),
        .aud_pwm (aud_pwm),
        .aud_sd  (aud_sd)
    );

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

    IOBUF u_qspi_dq0 (.IO(qspi_dq0), .I(qspi_io0_o), .O(qspi_io0_i), .T(qspi_io0_t));
    IOBUF u_qspi_dq1 (.IO(qspi_dq1), .I(qspi_io1_o), .O(qspi_io1_i), .T(qspi_io1_t));
    IOBUF u_qspi_dq2 (.IO(qspi_dq2), .I(qspi_io2_o), .O(qspi_io2_i), .T(qspi_io2_t));
    IOBUF u_qspi_dq3 (.IO(qspi_dq3), .I(qspi_io3_o), .O(qspi_io3_i), .T(qspi_io3_t));
    IOBUF u_qspi_cs  (.IO(qspi_cs),  .I(qspi_ss_o),  .O(qspi_ss_i),  .T(qspi_ss_t));

    STARTUPE2 #(.PROG_USR("FALSE"), .SIM_CCLK_FREQ(0.0)) u_startup (
        .CFGCLK    (),
        .CFGMCLK   (),
        .EOS       (),
        .PREQ      (),
        .CLK       (1'b0),
        .GSR       (1'b0),
        .GTS       (1'b0),
        .KEYCLEARB (1'b1),
        .PACK      (1'b0),
        .USRCCLKO  (qspi_sck),
        .USRCCLKTS (1'b0),
        .USRDONEO  (1'b1),
        .USRDONETS (1'b1)
    );

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
