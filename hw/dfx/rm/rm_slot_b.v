// DFX hello-swap reconfigurable module B. Identical boundary to rm_slot_a:
// AXI4-Lite slave, one clock, one reset. reg0 = 0x55550000 | counter[15:0]
// (counter decrements each read); reg4 = build signature 0x5B5B5B5B.

`timescale 1ns / 1ps

module rm_slot (
    input  wire        clk,
    input  wire        aresetn,

    // PDM boundary (unused in the hello RMs)
    output wire        pdm_clk_o,
    input  wire        pdm_data_i,

    output wire        sonar_tx_a,
    output wire        sonar_tx_b,
    output wire        afe_gain_pwm,
    output wire        afe_hilo,
    input  wire [11:0] adc_data_i,
    output wire        adc_clk_o,

    // AXI4-Lite slave
    input  wire [31:0] s_axi_awaddr,
    input  wire        s_axi_awvalid,
    output reg         s_axi_awready,
    input  wire [31:0] s_axi_wdata,
    input  wire [3:0]  s_axi_wstrb,
    input  wire        s_axi_wvalid,
    output reg         s_axi_wready,
    output reg [1:0]   s_axi_bresp,
    output reg         s_axi_bvalid,
    input  wire        s_axi_bready,
    input  wire [31:0] s_axi_araddr,
    input  wire        s_axi_arvalid,
    output reg         s_axi_arready,
    output reg [31:0]  s_axi_rdata,
    output reg [1:0]   s_axi_rresp,
    output reg         s_axi_rvalid,
    input  wire        s_axi_rready
);

    assign pdm_clk_o = 1'b0;
    assign sonar_tx_a   = 1'b0;
    assign sonar_tx_b   = 1'b0;
    assign afe_gain_pwm = 1'b0;
    assign afe_hilo     = 1'b0;
    assign adc_clk_o    = 1'b0;

    localparam [15:0] SIG  = 16'h5555;
    localparam [31:0] SIG2 = 32'h5B5B5B5B;
    localparam        UP   = 1'b0;

    reg [15:0] count = 16'd0;
    reg [3:0]  raddr;

    // Write channel: accept and discard (OKAY)
    always @(posedge clk) begin
        if (!aresetn) begin
            s_axi_awready <= 1'b0;
            s_axi_wready  <= 1'b0;
            s_axi_bvalid  <= 1'b0;
            s_axi_bresp   <= 2'b00;
        end else begin
            s_axi_awready <= s_axi_awvalid && s_axi_wvalid && !s_axi_bvalid;
            s_axi_wready  <= s_axi_awvalid && s_axi_wvalid && !s_axi_bvalid;
            if (s_axi_awready && s_axi_awvalid && s_axi_wvalid)
                s_axi_bvalid <= 1'b1;
            else if (s_axi_bvalid && s_axi_bready)
                s_axi_bvalid <= 1'b0;
        end
    end

    // Read channel
    always @(posedge clk) begin
        if (!aresetn) begin
            s_axi_arready <= 1'b0;
            s_axi_rvalid  <= 1'b0;
            s_axi_rresp   <= 2'b00;
            count         <= 16'd0;
        end else begin
            s_axi_arready <= s_axi_arvalid && !s_axi_rvalid;
            if (s_axi_arready && s_axi_arvalid) begin
                raddr        <= s_axi_araddr[3:0];
                s_axi_rvalid <= 1'b1;
                s_axi_rdata  <= (s_axi_araddr[3:0] == 4'h4) ? SIG2
                                                            : {SIG, count};
                if (s_axi_araddr[3:0] == 4'h0)
                    count <= UP ? count + 16'd1 : count - 16'd1;
            end else if (s_axi_rvalid && s_axi_rready) begin
                s_axi_rvalid <= 1'b0;
            end
        end
    end

endmodule
