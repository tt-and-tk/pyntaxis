`include "rom.svh"
`include "machine.svh"

module rom_sv(
    input logic clk,
    rom_read_if.slave rom_read
    );
    import machine_p::*;

    localparam integer ROM_SIZE = 7;

    // BRAMへ推論させるため配列自体にも明示する
    (* rom_style = "block" *) machine_t machines[0:ROM_SIZE - 1] = {
        scan(1),
        print(1, 33'h1_0000_0000 + 0),
        rm(4'hf, 1, 2, 0),
        wm(4'hf, 1, 2, 0),
        brm(4'hf, 1, 2, 3, 0),
        bwm(4'hf, 1, 2, 3, 0),
        jmp(0, 33'h1_0000_0000 + 6)
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
