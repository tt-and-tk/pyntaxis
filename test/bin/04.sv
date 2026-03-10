`include "machine.svh"

module rom(
    input  logic clk,
    input  logic resetn,

    input  logic [31:0] pc,
    output logic [31:0] machine,
    output logic [31:0] imm
    );

    logic [63:0] machines[0:255] = {
        machine::jmp(0, 33'h1_0000_0000 + 3),
        machine::jmp(0, 33'h1_0000_0000 + 1),
        machine::jmp(0, 33'h1_0000_0000 + 2),
        machine::jmp(0, 33'h1_0000_0000 + 2),
        machine::jmp(0, 33'h1_0000_0000 + 4),
    };

    always_comb begin
        if (pc >= 32'h100) begin
            machine <= 32'b0;
            imm <= 32'b0;
        end else begin
            machine <= machines[pc][63:32];
            imm <= machines[pc][31:0];
        end
    end

endmodule
