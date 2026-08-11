`include "rom.svh"
`include "machine.svh"

module rom_sv(
    rom_read_if.slave rom_read
    );
    import machine_p::*;

    localparam integer ROM_SIZE = 10;

    machine_t machines[0:ROM_SIZE - 1] = {
        mov(0, 0, 0, 33'h1_0000_0000 + 0),
        call(0, 33'h1_0000_0000 + 4),
        mov(0, 0, 0, 33'h1_0000_0000 + 1),
        jmp(0, 33'h1_0000_0000 + 3),
        mov(0, 0, 0, 33'h1_0000_0000 + 2),
        call(0, 33'h1_0000_0000 + 8),
        mov(0, 0, 0, 33'h1_0000_0000 + 3),
        ret(),
        mov(0, 0, 0, 33'h1_0000_0000 + 4),
        ret()
    };

    always_comb begin
        rom_read.valid = (rom_read.pc < ROM_SIZE);
        if (rom_read.valid) begin
            rom_read.machine = machines[rom_read.pc];
        end else begin
            rom_read.machine = nop();
        end
    end

endmodule
