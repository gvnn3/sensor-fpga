// M6 reconfigurable module: single-channel PDM capture + CIC decimator.
// Same boundary as the hello RMs (rm_slot). Drives the on-board ADMP421:
// ~2.38 MHz PDM clock, CIC N=4 R=64 -> ~37.2 kHz PCM, DC removed with a
// leaky estimator, per-window (1024 samples, ~27 ms) energy accumulator.
//
// Register map (AXI4-Lite, offsets):
//   0x0  R  0x9D3A | free-running window counter[15:0]
//   0x4  R  build signature 0x9D3A0001
//   0x8  R  latest PCM sample (sign-extended 32-bit, DC-removed)
//   0xC  R  energy of last completed window (sum sample^2 >> 10)

`timescale 1ns / 1ps

module rm_slot (
    input  wire        clk,
    input  wire        aresetn,

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

    assign sonar_tx_a   = 1'b0;
    assign sonar_tx_b   = 1'b0;
    assign afe_gain_pwm = 1'b0;
    assign afe_hilo     = 1'b0;
    assign adc_clk_o    = 1'b0;

    localparam [31:0] SIG2 = 32'h9D3A0001;

    // --- PDM clock: 100 MHz / 42 = 2.381 MHz --------------------------------
    reg [5:0] div = 6'd0;
    reg       pdm_clk = 1'b0;
    wire      half = (div == 6'd20);
    always @(posedge clk) begin
        if (half) begin
            div     <= 6'd0;
            pdm_clk <= ~pdm_clk;
        end else begin
            div <= div + 6'd1;
        end
    end
    assign pdm_clk_o = pdm_clk;

    // ADMP421 with LRSEL=0 drives data while clock is LOW; sample just
    // before the rising edge.
    wire pdm_sample_en = half & ~pdm_clk;

    // --- CIC N=4, R=64, M=1 (bit growth 4*log2(64)=24 -> 25 bits) ----------
    reg signed [24:0] i0, i1, i2, i3;
    reg signed [24:0] c0, c1, c2, c3;
    reg signed [24:0] d0, d1, d2, d3;
    reg [5:0]  rcnt = 6'd0;
    reg        dec_en = 1'b0;

    wire signed [24:0] pdm_pm = pdm_data_i ? 25'sd1 : -25'sd1;

    always @(posedge clk) begin
        dec_en <= 1'b0;
        if (pdm_sample_en) begin
            i0 <= i0 + pdm_pm;
            i1 <= i1 + i0;
            i2 <= i2 + i1;
            i3 <= i3 + i2;
            if (rcnt == 6'd63) begin
                rcnt   <= 6'd0;
                dec_en <= 1'b1;
            end else begin
                rcnt <= rcnt + 6'd1;
            end
        end
        if (dec_en) begin
            d0 <= i3;      c0 <= i3 - d0;
            d1 <= c0;      c1 <= c0 - d1;
            d2 <= c1;      c2 <= c1 - d2;
            d3 <= c2;      c3 <= c2 - d3;
        end
    end

    // --- DC removal (leaky average) + windowed energy -----------------------
    reg signed [24:0] dc = 25'sd0;
    reg signed [24:0] ac = 25'sd0;
    reg        [9:0]  wcnt = 10'd0;
    reg        [47:0] eacc = 48'd0;
    reg        [31:0] energy = 32'd0;
    reg        [15:0] windows = 16'd0;
    reg               dec_d = 1'b0;

    wire signed [15:0] ac16 = ac[24:9]; // scale to 16-bit-ish
    wire signed [31:0] sq   = ac16 * ac16;

    always @(posedge clk) begin
        dec_d <= dec_en;
        if (dec_d) begin
            dc <= dc + ((c3 - dc) >>> 8);
            ac <= c3 - dc;
            eacc <= eacc + {16'd0, sq};
            if (wcnt == 10'd1023) begin
                wcnt    <= 10'd0;
                energy  <= eacc[41:10];
                eacc    <= 48'd0;
                windows <= windows + 16'd1;
            end else begin
                wcnt <= wcnt + 10'd1;
            end
        end
    end

    // --- AXI4-Lite -----------------------------------------------------------
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

    always @(posedge clk) begin
        if (!aresetn) begin
            s_axi_arready <= 1'b0;
            s_axi_rvalid  <= 1'b0;
            s_axi_rresp   <= 2'b00;
        end else begin
            s_axi_arready <= s_axi_arvalid && !s_axi_rvalid;
            if (s_axi_arready && s_axi_arvalid) begin
                s_axi_rvalid <= 1'b1;
                case (s_axi_araddr[3:0])
                    4'h4:    s_axi_rdata <= SIG2;
                    4'h8:    s_axi_rdata <= {{16{ac16[15]}}, ac16};
                    4'hC:    s_axi_rdata <= energy;
                    default: s_axi_rdata <= {16'h9D3A, windows};
                endcase
            end else if (s_axi_rvalid && s_axi_rready) begin
                s_axi_rvalid <= 1'b0;
            end
        end
    end

endmodule
