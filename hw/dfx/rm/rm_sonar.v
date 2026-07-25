// M7 reconfigurable module: pulse-echo sonar front end (AD9226 + piezo).
// Same boundary as the other RMs (rm_slot, boundary v3). Without the
// external harness the ADC bus floats — burst/capture logic still runs
// and self-reports, which is the swap-demo gate; echo ranging needs
// hardware.
//
// Register map (AXI4-Lite):
//   0x0  R  0x50A4 | burst counter[15:0]
//   0x4  R  build signature 0x50A40001
//   0x8  W  write 1 = fire one 8-cycle 40 kHz burst, then capture
//        R  busy flag (bit0)
//   0xC  R  peak |sample-2048| [27:16] | peak time in adc clocks [15:0]

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

    localparam [31:0] SIG2 = 32'h50A40001;

    assign pdm_clk_o = 1'b0;

    // AFE: mid gain PWM (50%), low-gain range
    reg [7:0] pwm = 8'd0;
    always @(posedge clk) pwm <= pwm + 8'd1;
    assign afe_gain_pwm = pwm[7];
    assign afe_hilo     = 1'b0;

    // ADC clock: 100/5 = 20 MHz (derated per plan; 200-ohm Pmod path)
    reg [2:0] adiv = 3'd0;
    reg       aclk = 1'b0;
    always @(posedge clk) begin
        if (adiv == 3'd4) begin
            adiv <= 3'd0;
            aclk <= ~aclk;
        end else begin
            adiv <= adiv + 3'd1;
        end
    end
    assign adc_clk_o = aclk;
    wire adc_tick = (adiv == 3'd4) & ~aclk; // once per aclk rising edge

    // 40 kHz burst: 100 MHz / 2500; 8 cycles per shot
    reg        busy = 1'b0;
    reg        fire = 1'b0;
    reg [11:0] tdiv = 12'd0;
    reg [3:0]  tcyc = 4'd0;
    reg        tx   = 1'b0;
    reg        txen = 1'b0;
    reg [15:0] bursts = 16'd0;

    // capture window: 4096 adc clocks (~205 us @ 20 MHz)
    reg [15:0] cap_t = 16'd0;
    reg [11:0] peak  = 12'd0;
    reg [15:0] peak_t = 16'd0;
    wire [11:0] mag = adc_data_i[11] ? (adc_data_i - 12'd2048)
                                     : (12'd2048 - adc_data_i);

    always @(posedge clk) begin
        if (!aresetn) begin
            busy <= 1'b0;
            txen <= 1'b0;
        end else begin
            if (fire && !busy) begin
                busy   <= 1'b1;
                txen   <= 1'b1;
                tdiv   <= 12'd0;
                tcyc   <= 4'd0;
                tx     <= 1'b0;
                cap_t  <= 16'd0;
                peak   <= 12'd0;
                peak_t <= 16'd0;
                bursts <= bursts + 16'd1;
            end
            if (busy && txen) begin
                if (tdiv == 12'd1249) begin
                    tdiv <= 12'd0;
                    tx   <= ~tx;
                    if (tx) begin
                        if (tcyc == 4'd7)
                            txen <= 1'b0;
                        else
                            tcyc <= tcyc + 4'd1;
                    end
                end else begin
                    tdiv <= tdiv + 12'd1;
                end
            end
            if (busy && !txen && adc_tick) begin
                if (mag > peak) begin
                    peak   <= mag;
                    peak_t <= cap_t;
                end
                if (cap_t == 16'd4095)
                    busy <= 1'b0;
                else
                    cap_t <= cap_t + 16'd1;
            end
        end
    end

    assign sonar_tx_a = txen ? tx : 1'b0;
    assign sonar_tx_b = txen ? ~tx : 1'b0;

    // --- AXI4-Lite -----------------------------------------------------------
    always @(posedge clk) begin
        fire <= 1'b0;
        if (!aresetn) begin
            s_axi_awready <= 1'b0;
            s_axi_wready  <= 1'b0;
            s_axi_bvalid  <= 1'b0;
            s_axi_bresp   <= 2'b00;
        end else begin
            s_axi_awready <= s_axi_awvalid && s_axi_wvalid && !s_axi_bvalid;
            s_axi_wready  <= s_axi_awvalid && s_axi_wvalid && !s_axi_bvalid;
            if (s_axi_awready && s_axi_awvalid && s_axi_wvalid) begin
                s_axi_bvalid <= 1'b1;
                if (s_axi_awaddr[3:0] == 4'h8 && s_axi_wdata[0])
                    fire <= 1'b1;
            end else if (s_axi_bvalid && s_axi_bready) begin
                s_axi_bvalid <= 1'b0;
            end
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
                    4'h8:    s_axi_rdata <= {31'd0, busy};
                    4'hC:    s_axi_rdata <= {4'd0, peak, peak_t};
                    default: s_axi_rdata <= {16'h50A4, bursts};
                endcase
            end else if (s_axi_rvalid && s_axi_rready) begin
                s_axi_rvalid <= 1'b0;
            end
        end
    end

endmodule
