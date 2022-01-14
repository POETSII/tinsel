import Interface    :: *;
import Globals    :: *;
import Mailbox    :: *;
import Queue    :: *;
import Vector    :: *;
import ConfigReg    :: *;

interface HostLinkPCIeAdaptorIfc;

  interface Out#(Bit#(128)) streamToHost; // from us to PCIe/host
  interface In#(Bit#(128)) streamFromHost; // from PCIe/host to us
  interface MailboxNet mbox;

endinterface


module mkHostLink(HostLinkPCIeAdaptorIfc);

  OutPort#(Bit#(128)) toPCIe <- mkOutPort();
  InPort#(Bit#(128)) fromPCIe <- mkInPort();

  Queue#(Flit) linkOutBuffer <- mkUGQueue;
  InPort#(Flit) fromLink <- mkInPort();

  // We always send a message-sized message to the host
  // When a message is less than that size, we emit padding
  Reg#(Bit#(`LogMaxFlitsPerMsg)) toPCIePadding <- mkReg(0);

  // Count the flits in a message going to the host
  Reg#(Bit#(`LogMaxFlitsPerMsg)) toPCIeFlitCount <- mkReg(0);

  // Connect 10G link to PCIe stream and idle detector
  rule fromLinkRule;
    Flit flit = fromLink.value;
    if (toPCIePadding == 0) begin
      if (fromLink.canGet) begin
        if (flit.isIdleToken) begin
          // if (toDetector.canPut) begin
          //   toDetector.put(flit);
          //   fromLink.get;
          // end
          fromLink.get;
        end else begin
          if (toPCIe.canPut) begin
            toPCIe.put(flit.payload);
            fromLink.get;
            if (flit.notFinalFlit) begin
              toPCIeFlitCount <= toPCIeFlitCount+1;
            end else begin
              toPCIePadding <=
                fromInteger (`MaxFlitsPerMsg-1) - toPCIeFlitCount;
              toPCIeFlitCount <= 0;
              // detector.decCount;
            end
          end
        end
      end
    end else begin
      if (toPCIe.canPut) begin
        toPCIe.put(0);
        toPCIePadding <= toPCIePadding-1;
      end
    end
  endrule


  // all we need to do is copy flits from out_pcie to in_noc, and vice-versa.
  Reg#(Bit#(32)) fromPCIeDA    <- mkConfigRegU;
  Reg#(Bit#(32)) fromPCIeNM    <- mkConfigRegU;
  Reg#(Bit#(8))  fromPCIeFM    <- mkConfigRegU;
  Reg#(Bit#(32))  fromPCIeKey   <- mkConfigRegU;
  Reg#(Bit#(1))  toLinkState   <- mkConfigReg(0);

  Reg#(Bit#(32)) messageCount  <- mkConfigReg(0);
  Reg#(Bit#(8))  flitCount     <- mkConfigReg(0);
  Reg#(Bool)     hostInjectInProgress <- mkConfigReg(False);

  rule toLink0 (toLinkState == 0);
    if (fromPCIe.canGet) begin
      hostInjectInProgress <= True;
      Bit#(128) data = fromPCIe.value;
      fromPCIeDA <= data[31:0];
      fromPCIeNM <= data[63:32];
      fromPCIeFM <= data[95:88];
      fromPCIeKey <= data[127:96];
      toLinkState <= 1;
      fromPCIe.get;
    end
  endrule

  rule toLink1 (toLinkState == 1);

    if (fromPCIe.canGet && linkOutBuffer.notFull) begin
      // Determine flit destination address
      Bit#(6) destThread = fromPCIeDA[`LogThreadsPerMailbox-1:0];
      Vector#(`ThreadsPerMailbox, Bool) destThreads = newVector();
      for (Integer i = 0; i < `ThreadsPerMailbox; i=i+1)
        destThreads[i] = destThread == fromInteger(i);
      // Construct flit
      Flit flit;
      flit.dest.addr = unpack(truncate(fromPCIeDA[31:`LogThreadsPerMailbox]));
      flit.dest.threads = pack(destThreads);
      // If address says to use routing key, then use it
      if (flit.dest.addr.isKey) begin
        flit.dest.threads = zeroExtend(fromPCIeKey);
      end
      flit.payload = fromPCIe.value;
      flit.notFinalFlit = True;
      flit.isIdleToken = False;
      if (flitCount == fromPCIeFM) begin
        flitCount <= 0;
        flit.notFinalFlit = False;
        if (messageCount == fromPCIeNM) begin
          messageCount <= 0;
          toLinkState <= 0;
          hostInjectInProgress <= False;
        end else
          messageCount <= messageCount+1;
      end else
        flitCount <= flitCount+1;
      linkOutBuffer.enq(flit);
      fromPCIe.get;
      // if (flitCount == 0) detector.incCount;
    end

  endrule

  interface streamToHost = toPCIe.out;
  interface streamFromHost = fromPCIe.in;

  interface MailboxNet mbox;
    interface flitIn = fromLink.in;
    interface flitOut = queueToBOut(linkOutBuffer);
  endinterface

endmodule
