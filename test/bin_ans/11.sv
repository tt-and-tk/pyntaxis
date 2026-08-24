`include "rom.svh"
`include "machine.svh"

module rom_sv(
    input logic clk,
    rom_read_if.slave rom_read
    );
    import machine_p::*;

    localparam integer ROM_SIZE = 13;

    (* rom_style = "block" *) machine_t machines[0:ROM_SIZE - 1] = {
        sll(1, 2, 3, 0),
        srl(1, 2, 3, 0),
        sla(1, 2, 3, 0),
        sra(1, 2, 3, 0),
        eq(1, 2, 33'h1_0000_0000 + 8),
        ne(1, 2, 33'h1_0000_0000 + 7),
        lt(1, 2, 33'h1_0000_0000 + 6),
        gt(1, 2, 33'h1_0000_0000 + 5),
        elt(1, 2, 33'h1_0000_0000 + 4),
        egt(1, 2, 33'h1_0000_0000 + 32'hfffffff7),
        jmp(0, 33'h1_0000_0000 + 12),
        jmp(0, 33'h1_0000_0000 + 0),
        jmp(0, 33'h1_0000_0000 + 12)
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
