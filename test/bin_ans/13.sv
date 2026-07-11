`include "rom.svh"
`include "machine.svh"

module rom_sv(
    rom_read_if.slave rom_read
    );
    import machine_p::*;

    localparam integer ROM_SIZE = 4;

    machine_t machines[0:ROM_SIZE - 1] = {
        mov(4'hf, 0, 0, 33'h1_0000_0000 + 32'hffffffff),
        mov(4'hf, 0, 1, 33'h1_0000_0000 + 32'hfffffffb),
        mov(4'hf, 0, 2, 33'h1_0000_0000 + 0),
        jmp(0, 33'h1_0000_0000 + 3)
    };

    always_comb begin
        if (rom_read.pc >= ROM_SIZE) begin
            rom_read.machine = nop();
        end else begin
            rom_read.machine = machines[rom_read.pc];
        end
    end

endmodule
