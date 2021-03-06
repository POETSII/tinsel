// SPDX-License-Identifier: BSD-2-Clause
package Globals;

import Util :: *;

// Core-local thread id
typedef Bit#(`LogThreadsPerCore) ThreadId;

// Board-local core id
typedef Bit#(`LogCoresPerBoard) CoreId;

// Board id
typedef struct {
  Bit#(`MeshYBits) y;
  Bit#(`MeshXBits) x;
} BoardId deriving (Eq, Bits, FShow);

// Network address
// Note: If 'host' bit is valid, then once the message reaches the
// destination board, it is routed either left or right depending
// the contents of the host bit.  This is to support bridge boards
// connected at the east/west rims of the FPGA mesh.
// The 'isKey' bit means that the destination is a routing key, held
// in the botom 32 bits of the 'NetAddr'.
// The 'acc' bit means message is routed to a custom accelerator rather
// than a mailbox.
typedef struct {
  Bool acc;
  Bool isKey;
  Option#(Bit#(1)) host;
  BoardId board;
  MailboxId mbox;
} MailboxNetAddr deriving (Bits, FShow);

typedef struct {
  MailboxNetAddr addr;
  Bit#(`ThreadsPerMailbox) threads;
} NetAddr deriving (Bits, FShow);

// Mailbox id
typedef struct {
  Bit#(`MailboxMeshYBits) y;
  Bit#(`MailboxMeshXBits) x;
} MailboxId deriving (Bits, Eq, FShow);

function MailboxId getMailboxId(NetAddr addr) = addr.addr.mbox;

// Extract routing key from network address
function Bit#(32) getRoutingKeyRaw(NetAddr addr) = truncate(pack(addr));

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
