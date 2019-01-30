#ifndef _BOARD_CTRL_D_H_
#define _BOARD_CTRL_D_H_

// Board control channel
enum BoardCtrlChannel {
  CtrlChannel = 0,     // FPGA board initialisation
  UartChannel = 1      // Send payload over debug link
};

// Board control packet
struct BoardCtrlPkt {
  uint8_t linkId;
  BoardCtrlChannel channel;
  uint8_t payload;
};

#endif
