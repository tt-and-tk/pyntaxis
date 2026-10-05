`include "rom.svh"
`include "machine.svh"

module rom_sv(
    input logic clk,
    rom_read_if.slave rom_read,
    rom_read_if.slave rom_read2
    );
    import machine_p::*;

    localparam integer ROM_SIZE = 12;

    (* rom_style = "block" *) machine_t machines[0:ROM_SIZE - 1] = {
        and_(1, 2, 3),
        or_(1, 2, 3),
        xor_(1, 2, 3),
        not_(1, 2),
        nand_(1, 2, 3),
        sub(1, 2, 3),
        mul(1, 2, 3),
        div(1, 2, 3, 0),
        div(1, 2, 3, 33'h1_0000_0000 + 4),
        divu(1, 2, 3, 0),
        divu(1, 2, 3, 33'h1_0000_0000 + 4),
        jmp(0, 33'h1_0000_0000 + 11)
    };

    always_ff @(posedge clk) begin
        rom_read.valid <= (rom_read.pc < ROM_SIZE);

        if (rom_read.pc < ROM_SIZE) begin
            rom_read.machine <= machines[rom_read.pc];
        end else begin
            rom_read.machine <= nop();
        end
    end

    always_ff @(posedge clk) begin
        rom_read2.valid <= (rom_read2.pc < ROM_SIZE);

        if (rom_read2.pc < ROM_SIZE) begin
            rom_read2.machine <= machines[rom_read2.pc];
        end else begin
            rom_read2.machine <= nop();
        end
    end

endmodule
