#include <HostLink.h>
#include "latency.h"
#include <cassert>
#include <vector>

uint32_t tinselHostId()
{
  return 1 << (1 + TinselMeshYBits + TinselMeshXBits +
                     TinselLogCoresPerBoard + TinselLogThreadsPerCore);
}

struct hop_info_t
{
  int inter_mailbox_hops;
  int inter_fpga_hops;
};

hop_info_t calc_distance(HostLink &hl, uint32_t t1, uint32_t t2)
{

  uint32_t t1_meshx=(t1>>TinselLogThreadsPerBoard) & ((1<<TinselMeshXBits)-1);
  uint32_t t1_meshy=(t1>>(TinselLogThreadsPerBoard+TinselMeshXBits)) & ((1<<TinselMeshYBits)-1);
  uint32_t t1_mboxx=(t1>>TinselLogThreadsPerMailbox) & ((1<<TinselMeshXBitsWithinBox)-1);
  uint32_t t1_mboxy=(t1>>(TinselLogThreadsPerMailbox + TinselMeshXBitsWithinBox)) & ((1<<TinselMeshYBitsWithinBox)-1);
  uint32_t t2_meshx=(t2>>TinselLogThreadsPerBoard) & ((1<<TinselMeshXBits)-1);
  uint32_t t2_meshy=(t2>>(TinselLogThreadsPerBoard+TinselMeshXBits)) & ((1<<TinselMeshYBits)-1);
  uint32_t t2_mboxx=(t2>>TinselLogThreadsPerMailbox) & ((1<<TinselMeshXBitsWithinBox)-1);
  uint32_t t2_mboxy=(t2>>(TinselLogThreadsPerMailbox + TinselMeshXBitsWithinBox)) & ((1<<TinselMeshYBitsWithinBox)-1);


  hop_info_t hops{0,0};

  //fprintf(stderr, "t1=%u -> meshy=%u, meshx=%u, mbox=%u, mboxy=%u\n", t1, t1_meshy, t1_meshx, t1_mboxy, t1_mboxx);
  //printf(stderr, "t2=%u -> meshy=%u, meshx=%u, mbox=%u, mboxy=%u\n", t2, t2_meshy, t2_meshx, t2_mboxy, t2_mboxx);

  if(t1_meshx==t2_meshx && t1_meshy==t2_meshy){
    //fprintf(stderr, "Local\n");
    while(t1_mboxx!=t2_mboxx){
      //fprintf(stderr, "  x\n");
      hops.inter_mailbox_hops++;
      if(t1_mboxx<t2_mboxx){
        t1_mboxx++;
      }else{
        t1_mboxx--;
      }
    }
    while(t1_mboxy!=t2_mboxy){
      //fprintf(stderr, "  y\n");
      hops.inter_mailbox_hops++;
      if(t1_mboxy<t2_mboxy){
        t1_mboxy++;
      }else{
        t1_mboxy--;
      }
    }
  }else{
    // Count the router->grid and grid->router as one hop
    hops.inter_mailbox_hops+=2;

    while(t1_meshx!=t2_meshx || t1_meshy!=t2_meshy){
      if(t1_mboxy!=0){
        assert(t1_mboxy>0);
        t1_mboxy--;
        hops.inter_mailbox_hops++;
      }else if(t1_meshx != t2_meshx){
        hops.inter_fpga_hops++;
        if(t1_meshx > t2_meshx){
          t1_meshx--;
        }else{
          t1_meshx++;
        }
      }else{
        hops.inter_fpga_hops++;
        if(t1_meshy > t2_meshy){
          t1_meshy--;
        }else{
          t1_meshy++;
        }
      }
    }

    while(t1_mboxy!=t2_mboxy){
      assert(t1_mboxy<t2_mboxy);
      t1_mboxy++;
      hops.inter_mailbox_hops++;
    }
  }

  //fprintf(stderr, "   hops={%u,%u}\n", hops.inter_fpga_hops, hops.inter_mailbox_hops);

  return hops;
}


void run_spot(
  HostLink &hostlink,
  uint32_t controller,
  uint32_t responder,
  unsigned reps
){
  job_msg_full_t msgIn, msgOut;
  msgIn.dest_mbox = responder>>TinselLogThreadsPerMailbox;
  msgIn.source_mbox = tinselHostId()>>TinselLogThreadsPerMailbox;
  assert(reps>0);
  msgIn.reps = reps;
  msgIn.cycle_count=0;

  hostlink.send(controller, TinselMaxFlitsPerMsg, &msgIn);

  auto hops=calc_distance(hostlink, controller, responder);

  hostlink.recv( &msgOut );
  //fprintf(stderr, "dest=%u, source=%u, reps=%u, cycle_count=%u\n",
  //  msgOut.dest_mbox, msgOut.source_mbox, msgOut.reps, msgOut.cycle_count);
  
  assert(msgOut.source_mbox==controller>>TinselLogThreadsPerMailbox);
  assert(msgOut.reps==reps);
  //printf("Got response, reps = %u, cycles = %u\n", reps, msgOut.cycle_count);

  printf("%u,%u, %u,%u, %u, %u,%g\n",
      controller, responder, hops.inter_fpga_hops, hops.inter_mailbox_hops,  reps, msgOut.cycle_count, msgOut.cycle_count / double(reps) / double(TinselClockFreq*1000000)      
  );
}

int main()
{
  fprintf(stderr, "Aquiring host-link\n");
  HostLink hostLink;

  fprintf(stderr, "Booting\n");
  hostLink.boot("code.v", "data.v");

  std::vector<uint32_t> threads;
  for(unsigned meshY=0; meshY<hostLink.meshYLen; meshY++){
    for(unsigned meshX=0; meshX<hostLink.meshXLen; meshX++){
      for(unsigned mbox=0; mbox<TinselMailboxesPerBoard; mbox++){
        uint32_t tid=((((meshY<<TinselMeshXBits)|meshX)<<TinselLogMailboxesPerBoard)|mbox)<<TinselLogThreadsPerMailbox;
        threads.push_back(tid);
      }
    }
  }

  fprintf(stderr, "Starting\n");
  hostLink.go();

  printf("src_thread,dst_thread,inter_fpga_hops,inter_mbox_hops,repeats,total_cycles_for_all_repeats,time_per_roundtrip\n");
  for(unsigned i=0; i+1 < threads.size(); i++){
    fprintf(stderr, "Outer thread index = %u of %u\n", i, (unsigned)threads.size());
    fflush(stdout);
    for(unsigned j=i+1; j<threads.size(); j++){
      for(unsigned reps=1; reps<=256; reps*=16){
        run_spot(hostLink,  threads[i],threads[j], reps);
      }
    }
  }

  return 0;
}
