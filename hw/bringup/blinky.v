// M0 bring-up smoke test for the Nexys A7-100T.
// Proves: Vivado 2025.2 builds for xc7a100tcsg324-1, JTAG cable works,
// board programs. led[3:0] show counter bits (~0.75 Hz on led[0]).

`timescale 1ns / 1ps
`default_nettype none

module blinky (
    input  wire       clk100mhz,
    input  wire       cpu_resetn,   // active-low pushbutton reset
    output wire [3:0] led
);

    reg [26:0] count = 27'd0;

    always @(posedge clk100mhz) begin
        if (!cpu_resetn)
            count <= 27'd0;
        else
            count <= count + 27'd1;
    end

    assign led = count[26:23];

endmodule

`default_nettype wire
