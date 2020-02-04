// SPDX-License-Identifier: BSD-2-Clause
package ProgRouting;

// Functions and data types for programmable routers

// =============================================================================
// Routing keys and beats
// =============================================================================

// A routing record is either 40 bits or 80 bits in size (aligned on a
// 40-bit or 80-bit boundary respectively). Multiple records are
// packed into a 256-bit DRAM beat (aligned on a 256-bit boundary).
// The most significant 16 bits of the beat contain a count of the
// number of records in the beat (in the range 1 to 6 inclusive). The
// remaining 240 bits contain records. The first record lies in the
// least-significant bits of the beat. The size portion of the routing
// key contains the number of contiguous DRAM beats holding all
// records for the key.

// 256-bit routing beat
typedef struct {
  // Number of 40-bit record chunks present
  Bit#(16) size;
  // The 40-bit record chunks
  Vector#(6, Bit#(40)) chunks;
} RoutingBeat deriving (Bits);

// 32-bit routing key
typedef struct {
  // Pointer to array of routing beats containing routing records
  Bit#(26) ptr;
  // Number of beats in the array
  Bit#(6) numBeats;
} RoutingKey deriving (Bits);

// =============================================================================
// Types of routing record
// =============================================================================

typedef enum {
  URM1 = 3'd0, // 40-bit Unicast Router-to-Mailbox
  URM2 = 3'd1, // 80-bit Unicast Router-to-Mailbox
  RR   = 3'd2, // 40-bit Router-to-Router
  MRM  = 3'd3, // 80-bit Multicast Router-to-Mailbox
  IND  = 3'd4  // 40-bit Indirection
} RoutingRecordTag;

// 40-bit Unicast Router-to-Mailbox (URM1) record
typedef struct {
  // Record type
  RoutingRecordTag tag;
  // Mailbox destination
  Bit#(4) mbox;
  // Mailbox-local thread identifier
  Bit#(6) thread;
  // Local key. The first word of the message
  // payload is overwritten with this.
  Bit#(27) localKey;
} URM1Record deriving (Bits);

// 80-bit Unicast Router-to-Mailbox (URM2) record
typedef struct {
  // Record type
  RoutingRecordTag tag;
  // Mailbox destination
  Bit#(4) mbox;
  // Mailbox-local thread identifier
  Bit#(6) thread;
  // Currently unused
  Bit#(3) unused;
  // Local key. The first two words of the message
  // payload is overwritten with this.
  Bit#(64) localKey;
} URM2Record deriving (Bits);

// 40-bit Router-to-Router (RR) record
typedef struct {
  // Record type
  RoutingRecordTag tag;
  // Direction (N, S, E, or W)
  Bit#(2) dir;
  // Currently unused
  Bit#(3) unused;
  // New 32-bit routing key that will replace the one in the
  // current message for the next hop of the message's journey
  Bit#(32) newKey;
} RRRecord deriving (Bits);

// 80-bit Multicast Router-to-Mailbox (MRM) record
typedef struct {
  // Record type
  RoutingRecordTag tag;
  // Mailbox destination
  Bit#(4) mbox;
  // Currently unused
  Bit#(9) unused;
  // Mailbox-local destination mask
  Bit#(64) destMask;
} MRMRecord deriving (Bits);

// 40-bit Indirection (IND) record:
typedef struct {
  // Record type
  RoutingRecordTag tag;
  // Currently unused
  Bit#(5) unused;
  // New 32-bit routing key for new set of records on current router
  Bit#(32) newKey;
} MRMRecord deriving (Bits);

endpackage
