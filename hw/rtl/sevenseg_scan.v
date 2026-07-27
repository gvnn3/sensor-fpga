// Eight-digit seven-segment multiplexer for the Nexys A7-100T.
//
// disp[63:0] holds one byte per digit: byte i is the segment pattern for
// digit i, digit 0 = rightmost (AN0), digit 7 = leftmost (AN7). Within a
// byte, bit0=CA .. bit6=CG, bit7=DP, ACTIVE-HIGH. The board's cathode and
// anode pins are active-low, so both outputs are inverted here.
//
// Scan: each digit is driven for exactly 1 ms (100_000 cycles at 100 MHz),
// giving a full-display refresh of 8 ms (125 Hz) - well above the flicker
// threshold and within Digilent's recommended 1-16 ms per-digit window.

`timescale 1ns / 1ps

module sevenseg_scan (
    input  wire        clk,   // 100 MHz
    input  wire [63:0] disp,  // 8 x 8-bit segment patterns, active-high
    output reg  [7:0]  seg,   // {DP,CG,CF,CE,CD,CC,CB,CA}, active-low to pins
    output reg  [7:0]  an     // digit enables, active-low to pins
);

    localparam DIGIT_CYCLES = 100_000; // 1 ms per digit at 100 MHz

    reg [16:0] div   = 17'd0; // counts one digit period
    reg [2:0]  digit = 3'd0;  // currently lit digit, 0 = rightmost

    // Power-on: everything off (active-low), advances on the first tick.
    initial begin
        seg = 8'hff;
        an  = 8'hff;
    end

    always @(posedge clk) begin
        if (div == DIGIT_CYCLES - 1) begin
            div   <= 17'd0;
            digit <= digit + 3'd1;
        end else begin
            div <= div + 17'd1;
        end
        // Registered outputs: invert active-high register byte to the
        // board's active-low cathodes, one-hot low anode for this digit.
        seg <= ~disp[digit*8 +: 8];
        an  <= ~(8'h01 << digit);
    end

endmodule
