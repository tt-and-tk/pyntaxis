`include "rom.svh"
`include "machine.svh"

module rom_sv(
    input logic clk,
    rom_read_if.slave rom_read
    );
    import machine_p::*;

    localparam integer ROM_SIZE = 10;

    (* rom_style = "block" *) machine_t machines[0:ROM_SIZE - 1] = {
        mov(4'hf, 0, 1, 33'h1_0000_0000 + 8),
        call(1, 0),
        mov(4'hf, 0, 2, 33'h1_0000_0000 + 9),
        call(6'h2, 0),
        wm(4'hf, 0, 1, 33'h1_0000_0000 + 9),
        rmr(4'hf, 16, 3, 33'h1_0000_0000 + 8),
        call(0, 33'h1_0000_0000 + 9),
        jmp(0, 33'h1_0000_0000 + 7),
        ret(),
        ret()
    };

    always_ff @(posedge clk) begin
        rom_read.valid <= (rom_read.pc < ROM_SIZE);

        if (rom_read.pc < ROM_SIZE) begin
            rom_read.machine <= machines[rom_read.pc];
        end else begin
            rom_read.machine <= nop();
        end
    end

endmodule
