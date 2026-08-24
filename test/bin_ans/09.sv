`include "rom.svh"
`include "machine.svh"

module rom_sv(
    input logic clk,
    rom_read_if.slave rom_read
    );
    import machine_p::*;

    localparam integer ROM_SIZE = 24;

    (* rom_style = "block" *) machine_t machines[0:ROM_SIZE - 1] = {
        call(0, 33'h1_0000_0000 + 2),
        jmp(0, 33'h1_0000_0000 + 1),
        call(0, 33'h1_0000_0000 + 4),
        ret(),
        call(0, 33'h1_0000_0000 + 6),
        ret(),
        call(0, 33'h1_0000_0000 + 8),
        ret(),
        call(0, 33'h1_0000_0000 + 10),
        ret(),
        call(0, 33'h1_0000_0000 + 12),
        ret(),
        call(0, 33'h1_0000_0000 + 14),
        ret(),
        call(0, 33'h1_0000_0000 + 16),
        ret(),
        call(0, 33'h1_0000_0000 + 18),
        ret(),
        call(0, 33'h1_0000_0000 + 20),
        ret(),
        call(0, 33'h1_0000_0000 + 22),
        ret(),
        nop(),
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
