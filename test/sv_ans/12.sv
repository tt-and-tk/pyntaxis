`include "rom.svh"
`include "machine.svh"

module rom_sv(
    input logic clk,
    rom_read_if.slave rom_read1,
    rom_read_if.slave rom_read2
    );
    import machine_p::*;

    localparam integer ROM_SIZE = 12;

    (* rom_style = "block" *) machine_t machines[0:ROM_SIZE - 1] = {
        scan(1),
        print(1, 33'h1_0000_0000 + 0),
        rm(4'hf, 1, 2, 0),
        wm(4'hf, 1, 2, 0),
        brm(4'hf, 1, 2, 3, 0),
        bwm(4'hf, 1, 2, 3, 0),
        rmr(4'hf, 16, 2, 33'h1_0000_0000 + 0),
        rmr(4'hf, 1, 2, 33'h1_0000_0000 + 32'hfffffffc),
        wmr(4'hf, 16, 2, 33'h1_0000_0000 + 8),
        wmr(4'hf, 1, 2, 33'h1_0000_0000 + 32'h10),
        rmr(4'h1, 16, 2, 33'h1_0000_0000 + 4),
        jmp(0, 33'h1_0000_0000 + 11)
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
