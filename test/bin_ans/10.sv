`include "rom.svh"
`include "machine.svh"

module rom_sv(
    rom_read_if.slave rom_read
    );
    import machine_p::*;

    localparam integer ROM_SIZE = 9;

    machine_t machines[0:ROM_SIZE - 1] = {
        and_(1, 2, 3),
        or_(1, 2, 3),
        xor_(1, 2, 3),
        not_(1, 2),
        nand_(1, 2, 3),
        sub(1, 2, 3),
        mul(1, 2, 3),
        div(1, 2, 3, 0),
        jmp(0, 33'h1_0000_0000 + 8)
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
