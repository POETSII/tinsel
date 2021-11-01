#include "jtag/UART.h"
#include "BoardCtrl.h"
#include "config.h"
#include "DebugLink.h"
#include "HostLink.h"
#include <unistd.h>

#include <iostream>
#include <cassert>
#include <iomanip>

#ifdef SIMULATE
#define UARTID 0
#else
#define UARTID 1
#endif

// #include "JtagAtlantic.h"
//
// // https://github.com/tomverbeure/jtag_uart_example/blob/master/c_client/common.cpp
// // DEL SOON
// void show_info(JTAGATLANTIC *atlantic) {
//     char const *cable;
//     int device, instance;
//     jtagatlantic_get_info(atlantic, &cable, &device, &instance);
//     fprintf(stderr, "Connected to cable '%s', device %d, instance %d\n", cable, device, instance);
// }
//
// static const char *err_msgs[] = {
//     "No error",
//     "Unable to connect to local JTAG server",
//     "More than one cable available, provide more specific cable name",
//     "Cable not available",
//     "Selected cable is not plugged",
//     "JTAG not connected to board, or board powered down",
//     "Another program is already using the UART",
//     "More than one UART available, specify device/instance",
//     "No UART matching the specified device/instance",
//     "Selected UART is not compatible with this version of the library"
// };
// void show_err() {
//     char const *progname = NULL;
//     int err = jtagatlantic_get_error(&progname);
//     if(err >= -9 && err <= 0)
//         fprintf(stderr, "%s\n", err_msgs[-err]);
//     if(progname != NULL && progname[0])
//         fprintf(stderr, "progname: '%s'\n", progname);
// }

void drain(UART* uart) {
  int recv;
  long excess;
  char outbuf[256];

  excess = 0;
  recv = 0;
  int recv_retval;

  do {
    recv_retval=uart->read(outbuf, 255);
    if (recv_retval == -1) {
      perror("drain called read()");
      exit(1);
    }
    if (recv_retval > 0) excess += recv;
    outbuf[255] = 0;
    printf("> ");
    for (int i =0; i<recv; i++) printf("%i, ", outbuf[i]);
    printf("\n");
  } while (recv != 0);
  std::cout << "\ngot "<<excess<<" extra bytes." << std::endl;
}

// void atlatic_POST() {
// char chain[256];
// int instId = 1;
// JTAGATLANTIC* jtag;
// snprintf(chain, sizeof(chain), "%i", instId);
// jtag = jtagatlantic_open(chain, 0, 0, "hostlink");
// if (jtag == NULL) {
//   fprintf(stderr, "Error opening JTAG UART %i\n", instId);
//   exit(EXIT_FAILURE);
// }
//
// if (!jtagatlantic_is_setup_done(jtag)) {
//   std::cout << "setup not yet done." << std::endl;
// } else {
//   std::cout << "setup done." << std::endl;
// }
//
// jtagatlantic_wait_open(jtag);
//
// if (!jtagatlantic_is_setup_done(jtag)) {
//   std::cout << "setup not yet done." << std::endl;
// } else {
//   std::cout << "setup done." << std::endl;
// }
//
// show_err();
// show_info(jtag);
//
// std::cout << "bytes avail to read: " << jtagatlantic_bytes_available(jtag) << std::endl;
// show_err();
//
// char buf[16];
// while(1) {
//   int ret = jtagatlantic_read(jtag, buf, sizeof(buf));
//   if(ret < 0)
//       break;
//   fwrite(buf, ret, 1, stdout);
// }

// char const** cable_id;
// int* device_id;
// int* instance_id;
// jtagatlantic_get_info(jtag, cable_id, device_id, instance_id);
//
// std::cout << "cable ID: " << cable_id << " dev id: " << device_id << " instanceId: " << instance_id << std::endl;
//

// UART card_uart = UART();
// card_uart.open(1); // fist JTAG device on the chain
// char outbuf[2];
// char rpybuf[2];
// int written;
// int recv;
// outbuf[1] = 0;
// for (int i=0; i<120; i++) {
//   outbuf[0] = i;
//   written = card_uart.write(outbuf, 1);
//   std::cout << "Written " << written << " bytes." << std::endl;
//   recv = card_uart.read(rpybuf, 1); // queryout
//
//   std::cout << "recv'd " << recv << " bytes. " << std::hex << std::setfill('0') <<
//   " byte: " << std::setw(2) << (uint8_t)(rpybuf[0]) << std::endl;
// }
// }


void sendDebugLinkPkt(UART* card_uart, BoardCtrlPkt* pkt) {
  int written = 0;
  int size = toDebugLinkSize(pkt->payload[0]);
  int count = 1000;
  int write_retval;
  while (written < size && count > 0) {
    write_retval = card_uart->write((char *)(pkt->payload+written), size-written);
    if (write_retval > 0) written+=write_retval;
    if (write_retval == -1) {
      perror("sendDebugLinkPkt called write()");
      exit(1);
    }
    // std::cout << "Written " << written << " bytes." << std::endl;
    usleep(10);
    count--;
  }
}

int recvDebugLinkPkt(UART* card_uart, BoardCtrlPkt* pkt, int expected, int count = 2000000) {
  int recv = 0;
  int required = fromDebugLinkSize(expected);
  // std::cout << "waiting for " << required << " bytes" << std::endl;
  do {
    recv += card_uart->read((char *)(pkt->payload+recv), std::max(required-recv, 0));
    if (recv > 0) required = fromDebugLinkSize(pkt->payload[0]); // recv the entire packet from debuglink, no matter the expected type
    usleep(1000);
    count--;
    if (count == 0) { return 0; }; //{std::cout << "exiting due to no data" << std::endl; exit(0); }
    // std::cout << "got " << recv << " bytes" << std::endl;
  } while (recv < required);

  if (pkt->payload[0] != expected) printf("Expected a %i packet, got a %i\n", expected, pkt->payload[0]);

  if (pkt->payload[recv-1] != 0 && pkt->payload[0] == 0x02) {
    printf("%c", pkt->payload[recv-1]);
    std::cout << std::flush;
  } else if (pkt->payload[0] != 0x02) {
    std::cout << "recv'd " << recv << " bytes. msg:";
    for (int i=0; i<recv; i++) printf("0x#%02x (%c); ", pkt->payload[i], pkt->payload[i]);
    std::cout << std::endl;
  } else {
    printf(".");
  }
  return recv;
}


int main(void) {
  std::cout << "hello, world" << std::endl;


  // FOR LATENCY TESTER
  if (0) {

    UART card_uart = UART();
    card_uart.open(1); // first JTAG device on the chain; needs to be 0 for sim
    char outbuf[256];
    card_uart.write(outbuf, 1); // write a byte to trigger signaltap
    int recv = 0;
    while (1) {
      recv = card_uart.read(outbuf, 16);
      if (recv == 0) continue;
      outbuf[16] = 0;
      for (int i=0; i<recv-1; i++) printf("%i; ", outbuf[i+1]-outbuf[i]); // printf("%#02X; ", (uint8_t)(outbuf[i])); // std::cout << std::hex << std::setfill('0') << (uint8_t)(outbuf[i]) << "; ";
      std::cout << std::endl;
    }
  }

  // TEST UART LOOPBACK; bare metal
  if (0) {

    UART card_uart = UART();
    card_uart.open(1); // first JTAG device on the chain; needs to be 0 for sim
    char inbuf[256];
    char outbuf[256];

    int recv = 0;
    outbuf[0] = 5;
    card_uart.write(outbuf, 16); // write a byte to trigger signaltap
    while (1) {
      // card_uart.write(outbuf, 1); // write a byte to trigger signaltap
      outbuf[0] = outbuf[0]+1;
      recv = card_uart.read(inbuf, 1);
      for (int i=0; i<recv; i++) {
        printf("%#02X; ", (uint8_t)(inbuf[i]));
      }
      if (recv) std::cout << std::endl;
    }
  }

  // // TEST UART LOOPBACK; tinsel core
  // if (1) {
  //
  //   UART card_uart = UART();
  //   card_uart.open(UARTID); // first JTAG device on the chain; needs to be 0 for sim
  //   BoardCtrlPkt query;
  //   query.linkId = 0;
  //   query.payload[0] = DEBUGLINK_QUERY_IN;
  //   int x = 0;
  //   int y = 0;
  //   int offsetX = x * TinselMeshXLenWithinBox;
  //   int offsetY = y * TinselMeshYLenWithinBox;
  //   query.payload[1] = (offsetY << 4) | offsetX;
  //   sendDebugLinkPkt(&card_uart, &query);
  //   recvDebugLinkPkt(&card_uart, &query, DEBUGLINK_QUERY_OUT);
  //
  //   BoardCtrlPkt req_pkt;
  //   BoardCtrlPkt pkt;
  //
  //   req_pkt.linkId = 0;
  //   req_pkt.payload[0] = DEBUGLINK_SET_DEST;
  //   // Core-local thread id
  //   req_pkt.payload[1] = 0;
  //   // Board-local core id
  //   req_pkt.payload[2] = 0;
  //   sendDebugLinkPkt(&card_uart, &req_pkt); // set the dest core id
  //
  //   req_pkt.payload[0] = DEBUGLINK_STD_IN;
  //
  //   char ctr = 0;
  //   // while (recvDebugLinkPkt(&card_uart, &pkt, DEBUGLINK_STD_OUT, 100000) == 0) {
  //   //   req_pkt.payload[1] = ctr;
  //   //   sendDebugLinkPkt(&card_uart, &req_pkt); // send a byte to stdin
  //   //   ctr++;
  //   //   std::cout << "." << std::flush;
  //   // }
  //
  //
  //   while (1) {
  //     // card_uart.write(outbuf, 1); // write a byte to trigger signaltap
  //     req_pkt.payload[0] = DEBUGLINK_STD_IN;
  //     req_pkt.payload[1] = ctr;
  //
  //     sendDebugLinkPkt(&card_uart, &req_pkt); // send a byte to stdin
  //     std::cout << "sent byte " << (int)ctr << std::endl;
  //     ctr++;
  //     do {
  //       recvDebugLinkPkt(&card_uart, &pkt, DEBUGLINK_STD_OUT); // should get the same pkt back
  //     } while (pkt.payload[3] != '\0');
  //   }
  // }


  // TEST UART LOOPBACK; tinsel core
  if (0) {

    UART card_uart = UART();
    card_uart.open(UARTID); // first JTAG device on the chain; needs to be 0 for sim
    BoardCtrlPkt query;
    query.linkId = 0;
    query.payload[0] = DEBUGLINK_QUERY_IN;
    int x = 0;
    int y = 0;
    int offsetX = x * TinselMeshXLenWithinBox;
    int offsetY = y * TinselMeshYLenWithinBox;
    query.payload[1] = (offsetY << 4) | offsetX;
    sendDebugLinkPkt(&card_uart, &query);
    recvDebugLinkPkt(&card_uart, &query, DEBUGLINK_QUERY_OUT);

    BoardCtrlPkt req_pkt;
    BoardCtrlPkt pkt;



    uint8_t ctr = 0;
    // while (recvDebugLinkPkt(&card_uart, &pkt, DEBUGLINK_STD_OUT, 100000) == 0) {
    //   req_pkt.payload[1] = ctr;
    //   sendDebugLinkPkt(&card_uart, &req_pkt); // send a byte to stdin
    //   ctr++;
    //   std::cout << "." << std::flush;
    // }


    for (int coreid=0; coreid<256; coreid++) {
      req_pkt.linkId = 0;
      req_pkt.payload[0] = DEBUGLINK_SET_DEST;
      // Core-local thread id
      req_pkt.payload[1] = 0;
      // Board-local core id
      req_pkt.payload[2] = coreid;
      sendDebugLinkPkt(&card_uart, &req_pkt); // set the dest core id

      req_pkt.payload[0] = DEBUGLINK_STD_IN;
      req_pkt.payload[1] = coreid+1;

      sendDebugLinkPkt(&card_uart, &req_pkt); // send a byte to stdin
      std::cout << "sent byte " << (int)ctr << " to core " << coreid << std::endl;
      ctr++;

      do {
        recvDebugLinkPkt(&card_uart, &pkt, DEBUGLINK_STD_OUT); // should get the same pkt back
      } while (pkt.payload[3] != '\0');
    }
  }

  if (1) {
    HostLinkParams params;
    params.numBoxesX=1; params.numBoxesY=1; params.useExtraSendSlot=false;
    HostLink hl( params );
    hl.boot("../apps/hello/code.v", "../apps/hello/data.v");
    hl.go();
    hl.dumpStdOut();
  }




  if (0) {
    printf("testing query pkt\n");
    UART card_uart = UART();
    card_uart.open(UARTID); // first JTAG device on the chain; needs to be 0 for sim

    drain(&card_uart);

    BoardCtrlPkt query;
    query.linkId = 0;
    query.payload[0] = DEBUGLINK_QUERY_IN;

    int x = 1;
    int y = 1;
    int offsetX = x * TinselMeshXLenWithinBox;
    int offsetY = y * TinselMeshYLenWithinBox;
    assert(offsetX < 16);
    assert(offsetY < 16);
    query.payload[1] = (offsetY << 4) | offsetX;
    sendDebugLinkPkt(&card_uart, &query);

    // char* buf = (char*)(&query);
    //
    // for (int bytei=0; bytei<toDebugLinkSize(DEBUGLINK_QUERY_IN); bytei++) {
    //   int written = card_uart.write(buf+bytei, 1);
    //   std::cout << "Written " << written << " bytes." << std::endl;
    // }

    BoardCtrlPkt pkt;
    recvDebugLinkPkt(&card_uart, &pkt, DEBUGLINK_QUERY_OUT);
    printf("set debuglink id to %i\n", pkt.payload[1]);

    drain(&card_uart);
  }

  if (0) { // core self-id
    UART card_uart = UART();
    card_uart.open(UARTID); // first JTAG device on the chain; needs to be 0 for sim
    BoardCtrlPkt query;
    query.linkId = 0;
    query.payload[0] = DEBUGLINK_QUERY_IN;
    int x = 1;
    int y = 1;
    int offsetX = x * TinselMeshXLenWithinBox;
    int offsetY = y * TinselMeshYLenWithinBox;
    query.payload[1] = (offsetY << 4) | offsetX;
    // drain(&card_uart);
    // sendDebugLinkPkt(&card_uart, &query);
    // recvDebugLinkPkt(&card_uart, &query, DEBUGLINK_QUERY_OUT);
    // drain(&card_uart);

    BoardCtrlPkt req_pkt;
    BoardCtrlPkt pkt;
    bool seen[TinselCoresPerBoard];
    int BUFSZ = 256;
    char msg[BUFSZ];
    msg[BUFSZ-1] = '\n';
    char get;
    int idx;


    for (int i=0; i<1; i++) { //TinselCoresPerBoard
      req_pkt.linkId = 0;
      req_pkt.payload[0] = DEBUGLINK_SET_DEST;
      // Core-local thread id
      req_pkt.payload[1] = 0;
      // Board-local core id
      req_pkt.payload[2] = i;
      sendDebugLinkPkt(&card_uart, &req_pkt);

      req_pkt.payload[0] = DEBUGLINK_STD_IN;
      req_pkt.payload[1] = 0xFF;
      drain(&card_uart);
      sendDebugLinkPkt(&card_uart, &req_pkt);

      while (1) {
        idx = 0;
        do {
          recvDebugLinkPkt(&card_uart, &pkt, DEBUGLINK_STD_OUT);
          get = pkt.payload[3];
          if (get != 0) {
             msg[idx] = get;
            idx++;
            printf("%c\n", get);
          }
        } while (idx < 16);
        printf("              MSG from core %i: %s \n", i, msg);
        drain(&card_uart);
      }
    }
    drain(&card_uart);

    bool missing = false;
    for (int i=0; i<TinselCoresPerBoard; i++) {
      if (!seen[i]) missing = true;
    }
    if (missing) printf("A core failed to identify\n");
    else printf("All cores sent a message\n");
  }

  if (0) {
    UART card_uart = UART();
    card_uart.open(UARTID); // first JTAG device on the chain; needs to be 0 for sim
    BoardCtrlPkt query;
    query.linkId = 0;
    query.payload[0] = DEBUGLINK_QUERY_IN;
    int x = 1;
    int y = 1;
    int offsetX = x * TinselMeshXLenWithinBox;
    int offsetY = y * TinselMeshYLenWithinBox;
    query.payload[1] = (offsetY << 4) | offsetX;
    drain(&card_uart);

    BoardCtrlPkt req_pkt;
    BoardCtrlPkt pkt;

    for (uint8_t i=0; i<250; i++) {
      req_pkt.linkId = 0;
      req_pkt.payload[0] = DEBUGLINK_SET_DEST;
      // Core-local thread id
      req_pkt.payload[1] = 0;
      // Board-local core id
      req_pkt.payload[2] = i;
      sendDebugLinkPkt(&card_uart, &req_pkt); // set the dest core id

      req_pkt.payload[0] = DEBUGLINK_STD_IN;
      req_pkt.payload[1] = i;
      drain(&card_uart);
      sendDebugLinkPkt(&card_uart, &req_pkt); // send a byte to stdin

      recvDebugLinkPkt(&card_uart, &pkt, DEBUGLINK_STD_OUT); // should get the same pkt back
    }
  }

  if (0) {
    UART card_uart = UART();
    card_uart.open(UARTID); // first JTAG device on the chain; needs to be 0 for sim
    BoardCtrlPkt query;
    BoardCtrlPkt pkt;
    query.linkId = 0;
    query.payload[0] = DEBUGLINK_QUERY_IN;
    int x = 0;
    int y = 0;
    int offsetX = x * TinselMeshXLenWithinBox;
    int offsetY = y * TinselMeshYLenWithinBox;
    query.payload[1] = (offsetY << 4) | offsetX;
    query.payload[2] = 0; // don't enable send slot
    drain(&card_uart);
    sendDebugLinkPkt(&card_uart, &query);
    recvDebugLinkPkt(&card_uart, &pkt, DEBUGLINK_QUERY_OUT); // should get the same pkt back

    uint8_t coreId = 1;
    printf("sending a message to thread 0 core %i\n", coreId);

    // // make a stdin write to thread 0
    // query.linkId = 0;
    // query.payload[0] = DEBUGLINK_SET_DEST;
    // // Core-local thread id
    // query.payload[1] = 0;
    // // Board-local core id
    // query.payload[2] = coreId;
    // sendDebugLinkPkt(&card_uart, &query); // set the dest core id
    //
    // query.payload[0] = DEBUGLINK_STD_IN;
    // query.payload[1] = 5; // unimportant
    // sendDebugLinkPkt(&card_uart, &query); // send a byte to stdin

    while (1) {
      recvDebugLinkPkt(&card_uart, &pkt, DEBUGLINK_STD_OUT); // should get the same pkt back
    }


    // BoardCtrlPkt req_pkt;
    // BoardCtrlPkt pkt;
    //
    // // make a stdin write to thread 0
    // req_pkt.linkId = 0;
    // req_pkt.payload[0] = DEBUGLINK_SET_DEST;
    // // Core-local thread id
    // req_pkt.payload[1] = 0;
    // // Board-local core id
    // req_pkt.payload[2] = 1;
    // sendDebugLinkPkt(&card_uart, &req_pkt); // set the dest core id
    //
    // req_pkt.payload[0] = DEBUGLINK_STD_IN;
    // req_pkt.payload[1] = 254; // unimportant
    // sendDebugLinkPkt(&card_uart, &req_pkt); // send a byte to stdin

  }

  if (0) {
    DebugLinkParams params;
    params.numBoxesX=1;
    params.numBoxesY=1;
    params.useExtraSendSlot=0;
    DebugLink link(params);
  }
}
