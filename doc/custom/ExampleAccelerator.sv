// Example accelerator which consumes a flit, and then sends it to the
// address specified in the first word of the flit's payload.

typedef struct packed {
  logic acc;
  logic host;
  logic hostDir;
  logic [`TinselMeshYBits-1:0] boardY;
  logic [`TinselMeshXBits-1:0] boardX;
  logic [`TinselMailboxMeshYBits-1:0] tileY;
  logic [`TinselMailboxMeshYBits-1:0] tileX;
  logic [`TinselLogCoresPerMailbox-1:0] coreId;
  logic [`TinselLogThreadsPerCore-1:0] threadId;
} NetAddr;

typedef struct {
  // Destination address
  NetAddr dest;
  // Payload
  logic [`TinselBitsPerFlit-1:0] payload;
  // Is this the final flit in the message?
  logic notFinalFlit;
  // Is this a special packet for idle-detection?
  logic isIdleToken;
} Flit;

module ExternalTinselAccelerator
  ( // Clock and reset
    // By default, BSV synchronises on negedge(clock) and uses negative reset
    input wire clk
  , input wire rst_n

    // Coordinates of this FPGA board in the board mesh
  , input wire [`TinselMeshXBits-1:0] board_x
  , input wire [`TinselMeshYBits-1:0] board_y

    // Stream of flits coming in
  , input Flit in_data
  , input wire in_valid
  , output wire in_ready

    // Stream of flits going out
  , output Flit out_data
  , output wire out_valid
  , input wire out_ready
  );

  // Compile-time NoC coordinates of this accelerator
  parameter TILE_X;
  parameter TILE_Y;

  // Input flit queue
  reg inQueueFull = 0;
  reg Flit inQueueData;

  assign in_ready = !inQueueFull;

  // Output flit queue
  reg outQueueFull = 0;
  reg Flit outQueueData;

  assign out_valid = outQueueFull;

  // State machine
  always @(negedge clk) begin
    if (rst_n == 1) begin
      inQueueFull <= 0;
      outQueueFull <= 0;
    end else begin
      if (in_valid && !inQueueFull) begin
        inQueueFull <= 1;
        inQueueData <= in_data;
      end

      if (outQueueFull && out_ready) begin
        outQueueFull <= 0;
      end

      if (inQueueFull && !outQueueFull) begin
        outQueueFull <= 1;
        outQueueData <= '{
            dest: inQueueData.payload[31:0]
          , payload: inQueueData.payload
          , notFinalFlit: 0
          , isIdleToken: 0
        };
        inQueueFull <= 0;
      end
    end
  end

endmodule
