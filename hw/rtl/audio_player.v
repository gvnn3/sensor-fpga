// Audio effect player for the Nexys A7 mono PWM output (AUD_PWM -> 4th
// order LPF -> jack). Two sounds baked into block RAM from
// hw/rtl/audio_rom.mem (regenerate with tools/mp3_to_mem.py):
//   scan  = ROM [0     .. 32767], 4.096 s @ 8 kHz
//   alert = ROM [32768 .. 42559], 1.224 s @ 8 kHz
// Triggers are toggle-signaling (any edge starts playback, matching the
// ICAPE2 writer convention): ctl[0] = scan, ctl[1] = alert. A new trigger
// preempts the current sound. aud_sd enables the on-board amp while playing.

`timescale 1ns / 1ps

module audio_player (
    input  wire       clk,          // 100 MHz
    input  wire [1:0] ctl,          // toggle-to-play: bit0 scan, bit1 alert
    output reg        aud_pwm,
    output reg        aud_sd
);

    localparam [15:0] SCAN_START  = 16'd0;
    localparam [15:0] SCAN_END    = 16'd32767;
    localparam [15:0] ALERT_START = 16'd32768;
    localparam [15:0] ALERT_END   = 16'd42559;

    reg [7:0] rom [0:49151];
    initial $readmemh("/home/gnn/Repos/Yale/Sandbox/sensor-fpga/hw/rtl/audio_rom.mem", rom);

    reg [2:0]  scan_sync  = 3'b000;
    reg [2:0]  alert_sync = 3'b000;
    reg        playing = 1'b0;
    reg [15:0] addr = 16'd0;
    reg [15:0] addr_end = 16'd0;
    reg [13:0] srate = 14'd0;         // 100 MHz / 12500 = 8 kHz
    reg [7:0]  sample = 8'd128;
    reg [8:0]  pwm_acc = 9'd0;        // 1st-order sigma-delta

    always @(posedge clk) begin
        scan_sync  <= {scan_sync[1:0], ctl[0]};
        alert_sync <= {alert_sync[1:0], ctl[1]};

        if (scan_sync[2] ^ scan_sync[1]) begin
            playing  <= 1'b1;
            addr     <= SCAN_START;
            addr_end <= SCAN_END;
            srate    <= 14'd0;
        end else if (alert_sync[2] ^ alert_sync[1]) begin
            playing  <= 1'b1;
            addr     <= ALERT_START;
            addr_end <= ALERT_END;
            srate    <= 14'd0;
        end else if (playing) begin
            if (srate == 14'd12499) begin
                srate <= 14'd0;
                if (addr == addr_end)
                    playing <= 1'b0;
                else
                    addr <= addr + 16'd1;
            end else begin
                srate <= srate + 14'd1;
            end
        end

        sample <= playing ? rom[addr] : 8'd128;

        // Sigma-delta: carry-out of an 8-bit accumulator is the PWM bit
        pwm_acc <= {1'b0, pwm_acc[7:0]} + {1'b0, sample};
        aud_pwm <= playing ? pwm_acc[8] : 1'b0;
        aud_sd  <= playing;
    end

endmodule
