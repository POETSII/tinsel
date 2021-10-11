import Interface    :: *;
import Globals    :: *;
import Mailbox    :: *;

interface HostLinkPCIeAdaptorIfc;

  interface BOut#(Bit#(128)) streamToHost; // from us to PCIe/host
  interface In#(Bit#(128)) streamFromHost; // from PCIe/host to us
  interface MailboxNet mbox;

endinterface


module mkHostLink(HostLinkPCIeAdaptorIfc);

  BOut#(Bit#(128)) out_pcie <- mkNullBOut();
  In#(Bit#(128)) in_pcie <- mkNullIn();

  BOut#(Flit) out_noc <- mkNullBOut();
  In#(Flit) in_noc <- mkNullIn();


  interface streamToHost = out_pcie;
  interface streamFromHost = in_pcie;

  interface MailboxNet mbox;
    interface flitIn = in_noc;
    interface flitOut = out_noc;
  endinterface

endmodule
