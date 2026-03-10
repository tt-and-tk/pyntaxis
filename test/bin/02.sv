`include "machine.svh"

module rom(
    input  logic clk,
    input  logic resetn,

    input  logic [31:0] pc,
    output logic [31:0] machine,
    output logic [31:0] imm
    );

    logic [63:0] machines[0:255] = {
