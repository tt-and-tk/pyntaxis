`include "rom.svh"
`include "machine.svh"

module rom_sv(
    input logic clk,
    rom_read_if.slave rom_read,
    rom_read_if.slave rom_read2
    );
    import machine_p::*;

    localparam integer ROM_SIZE = 4;

    (* rom_style = "block" *) machine_t machines[0:ROM_SIZE - 1] = {
        call(0, 33'h1_0000_0000 + 2),
        jmp(0, 33'h1_0000_0000 + 1),
        call(0, 33'h1_0000_0000 + 2),
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

    always_ff @(posedge clk) begin
        rom_read2.valid <= (rom_read2.pc < ROM_SIZE);

        if (rom_read2.pc < ROM_SIZE) begin
            rom_read2.machine <= machines[rom_read2.pc];
        end else begin
            rom_read2.machine <= nop();
        end
    end

endmodule
