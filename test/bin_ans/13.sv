`include "rom.svh"
`include "machine.svh"

module rom_sv(
    input logic clk,
    rom_read_if.slave rom_read
    );
    import machine_p::*;

    localparam integer ROM_SIZE = 4;

    (* rom_style = "block" *) machine_t machines[0:ROM_SIZE - 1] = {
        mov(4'hf, 0, 0, 33'h1_0000_0000 + 32'hffffffff),
        mov(4'hf, 0, 1, 33'h1_0000_0000 + 32'hfffffffb),
        mov(4'hf, 0, 2, 33'h1_0000_0000 + 0),
        jmp(0, 33'h1_0000_0000 + 3)
    };

    // BRAMへ推論させるため，クロック同期の読み出しにする(rom_read.pcを出した次のサイクルで
    // rom_read.machine/rom_read.validが確定する．リセット分岐を持たない定型にすることで
    // BRAM推論を妨げないようにしている点に注意)
    always_ff @(posedge clk) begin
        // pcがROMの命令数に収まっているかを上位へ伝える
        rom_read.valid <= (rom_read.pc < ROM_SIZE);

        if (rom_read.pc < ROM_SIZE) begin
            rom_read.machine <= machines[rom_read.pc];
        end else begin
            rom_read.machine <= nop();
        end
    end

endmodule
