`include "rom.svh"
`include "machine.svh"

module rom_sv(
    input logic clk,
    rom_read_if.slave rom_read1,
    rom_read_if.slave rom_read2
    );
    import machine_p::*;

    localparam integer ROM_SIZE = 4;

    (* rom_style = "block" *) machine_t machines[0:ROM_SIZE - 1] = {
        mov(4'hf, 6'h2, 0, 33'h1_0000_0000 + 15),
        mov(15, 33, 0, 33'h1_0000_0000 + 4),
        add(34, 32, 33),
        jmp(0, 33'h1_0000_0000 + 3)
    };

    always_ff @(posedge clk) begin
        rom_read1.valid <= (rom_read1.pc < ROM_SIZE);

        if (rom_read1.pc < ROM_SIZE) begin
            rom_read1.machine <= machines[rom_read1.pc];
        end else begin
            rom_read1.machine <= nop();
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
