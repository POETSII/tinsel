// SPDX-License-Identifier: BSD-2-Clause
package Globals;

import Util :: *;



// ============================================================================
// Messages
// ============================================================================

// Message length in flits
// (A length of N corresponds to N+1 flits)
typedef Bit#(`LogMaxFlitsPerMsg) MsgLen;

// Flit payload
typedef Bit#(TMul#(`WordsPerFlit, 32)) FlitPayload;

// Flit type
typedef struct {
  // Destination address
  NetAddr dest;
  // Payload
  FlitPayload payload;
  // Is this the final flit in the message?
  Bool notFinalFlit;
  // Is this a special packet for idle-detection?
  Bool isIdleToken;
} Flit deriving (Bits, FShow);

// A padded flit is a multiple of 64 bits
// (i.e. the data width of the 10G MAC interface)
typedef TMul#(TDiv#(SizeOf#(Flit), 64), 64) PaddedFlitNumBits;
typedef Bit#(PaddedFlitNumBits) PaddedFlit;

// Padding functions
function PaddedFlit padFlit(Flit flit) = {?, pack(flit)};
function Flit unpadFlit(PaddedFlit flit) = unpack(truncate(pack(flit)));

endpackage
