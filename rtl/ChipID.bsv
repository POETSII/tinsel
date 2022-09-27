/*
 * Copyright (c) 2021 Simon W. Moore
 * All rights reserved.
 *
 * @BERI_LICENSE_HEADER_START@
 *
 * Licensed to BERI Open Systems C.I.C. (BERI) under one or more contributor
 * license agreements.  See the NOTICE file distributed with this work for
 * additional information regarding copyright ownership.  BERI licenses this
 * file to you under the BERI Hardware-Software License, Version 1.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at:
 *
 *   http://www.beri-open-systems.org/legal/license-1-0.txt
 *
 * Unless required by applicable law or agreed to in writing, Work distributed
 * under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations under the License.
 *
 * @BERI_LICENSE_HEADER_END@
 *
 * ----------------------------------------------------------------------------
 *
 * Read the Stratix 10 Chip ID
 */

package ChipID;

import GetPut  :: *;
import FIFOF   :: *;
import Clocks  :: *;

`ifdef SIMULATE
import "BDPI" function Bit#(32) getBoardId();
`endif


interface Stratix10ChipID;
  method Action   start;
  method Bit#(64) chip_id;
endinterface

`ifndef SIMULATE

import "BVI" chipid =
  module mkStratix10ChipID(Stratix10ChipID);
    method start() enable (readid);
    method chip_id chip_id() ready (data_valid);
    schedule (chip_id) C  (chip_id);
    schedule (start)   C  (start);
    schedule (start)   SB (chip_id);
    default_clock clk (clk, (*unused*) clk_gate);
    default_reset rst (reset);
  endmodule

module mkChipID(Get#(Bit#(64)));
  FIFOF#(Bit#(64))    idfifo <- mkFIFOF1;
  Reset               invRst <- invertCurrentReset();
  Stratix10ChipID      getid <- mkStratix10ChipID(reset_by invRst);
  Reg#(Bit#(4))  start_timer <- mkReg(0);

  /*
     Note: 'start' triggers 'readid' and the Chip ID docs say:
       "The readid signal is used to read the ID value from the
       device. Every time the signal change value from 1 to 0, the IP
       core triggers the read ID operation. You must drive the signal
       to 0 when unused. To start the read ID operation, drive the signal
       high for at least 3 clock cycles, then pull it low. The IP core
       starts reading the value of the chip ID."
     start_timer achieves this.
   */

  rule trigger (idfifo.notFull && (msb(start_timer)==0));
     getid.start();
     start_timer <= start_timer+1;
  endrule

  rule store (msb(start_timer)==1);
    Bit#(64) id = getid.chip_id();
    idfifo.enq(id);
    start_timer <= 0;
  endrule

  return toGet(idfifo);
endmodule

`endif

`ifdef SIMULATE

module mkChipID(Get#(Bit#(64)));

  Reg#(Bit#(64)) fpgaID <- mkReg(0);

  Reg#(Bool) set <- mkReg(False);

  rule setr (!set);
    fpgaID <= 64'h5555 + extend(getBoardId());
    set <= True;
  endrule

  return toGet(fpgaID);

endmodule

`endif

endpackage: ChipID
