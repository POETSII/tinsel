# Custom accelerators

Tinsel supports external custom accelerators written in Verilog or
SystemVerilog when the `UseCustomAccelerators` configuration option is
set.  With this option, each mailbox in the standard Tinsel design
becomes a *mailbox plus accelerator*, with the mailbox and accelerator
sharing the connections to the mesh router.

## Custom accelerator interface

The accelerator interface is currently quite simple:

```sv
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

  // Module body here
  // ...
endmodule
```

Note the use of Verilog macros such as `TinselMeshXBits` and
`TinselMeshXBits`.  These can be generated automatically by running
[config.py](https://github.com/POETSII/tinsel/blob/master/config.py)
with the `vpp` option (which stands for Verilog pre-processor).

## Flit format

Here is the flit format, as a SystemVerilog structure:

```sv
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
```

## Address format

Here is the address format for the `dest` field in a flit.  Note the
`acc` field, which determines whether a packet is destined for a
custom accelerator or a mailbox.

```sv
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
```

## Full example

