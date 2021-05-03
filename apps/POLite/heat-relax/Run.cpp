// SPDX-License-Identifier: BSD-2-Clause
#include "HeatRelax.h"

#include <HostLink.h>
#include <POLite.h>
#include <EdgeList.h>
#include <sys/time.h>

#include <POLite/HostLogger.h>

int main(int argc, char **argv)
{
  int n=10;
  float thresh=0.01;
  float start_thresh=thresh;
  float scale_thresh=0.1;
  float force=30;
  std::string file_base="out";

  if(argc>1){
    n=atoi(argv[1]);
  }
  if(argc>2){
    thresh=strtod(argv[2], nullptr);
    start_thresh=thresh;
  }
  if(argc>3){
    force=strtod(argv[3], nullptr);
  }
  if(argc>4){
    start_thresh=strtod(argv[4], nullptr);
  }
  if(argc>5){
    scale_thresh=strtod(argv[5], nullptr);
  }
  if(argc>6){
    file_base=argv[6];
  }

  fprintf(stderr, "size=%dx%d, threshold=%g, force_temperature=%g, file_base=%s\n",
    n, n, thresh, force, file_base.c_str()
    );

  fprintf(stderr, "start_thresh=%g, scale_thresh=%g\n", start_thresh, scale_thresh);

  HostLogger log(file_base+".log");

  PGraph<HeatDevice, HeatState, None, HeatMessage> graph;
  log.hook_graph(graph);

  std::vector<std::vector<PDeviceId>> ids(n, std::vector<PDeviceId>(n));
  for(int x=0; x<n; x++){
    for(int y=0; y<n; y++){
      ids[x][y]=graph.newDevice();
    }
  }

  auto connect=[&](PDeviceId a, PDeviceId b)
  {
    graph.addEdge(a, 0, b);
    graph.addEdge(b, 0, a);
  };

  for(int x=0; x<n; x++){
    for(int y=0; y<n; y++){
      if(x<n-1) connect(ids[x][y], ids[x+1][y]);
      if(y<n-1) connect(ids[x][y], ids[x][y+1]);
    }
  }

  graph.map();

  const char *colours[8]={
      "01230123",
      "23012301",
      "23012301",
      "01230123",
      "01230123",
      "23012301",
      "23012301",
      "01230123"
  };

  for(int x=1; x<7; x++){
    for(int y=1; y<7; y++){
      assert( colours[x+1][y] != colours[x][y+1] );
      assert( colours[x+1][y] != colours[x-1][y] );
      assert( colours[x+1][y] != colours[x][y-1] );
    }
  }

  std::mt19937_64 rng;
  std::uniform_real_distribution<> urng;

  for(int x=0; x<n; x++){
    for(int y=0; y<n; y++){

      auto &here=graph.devices[ids[x][y]]->state;
      int edges=0;
      edges += x==0;
      edges += x==n-1;
      edges += y==0;
      edges += y==n-1;
      here.sent_heat=to_heat(0);
      here.generation=0;
      if( edges>0 ){
        here.is_fixed=true;
        if(x==0 || y==0){
          here.curr_heat = to_heat(+force);
        }else{
          here.curr_heat = to_heat(-force);
        }
        //fprintf(stderr, "x=%d, y=%d, heat=%f\n", x, y, from_heat(here.curr_heat));
      }else{
        here.is_fixed=false;
        here.curr_heat=to_heat(urng(rng)*force-force/2);
      }
      for(unsigned i=0; i<4; i++){
        here.ghosts[i].generation=0;
        here.ghosts[i].heat=to_heat(0);
      }
      here.scale= to_heat( (edges==0) ? 0.25f : (edges==1) ? (1.0f/3.0f) : 0.5f );
      here.tolerance=to_heat( start_thresh );
      here.tolerance_scale=to_heat(scale_thresh);
      here.min_tolerance=to_heat(thresh);
      here.colour=colours[ x%8 ][ y%8 ]-'0';
      here.x=x;
      here.y=y;
    }
  }

  // Connection to tinsel machine
  log.tag_leaf("open_machine");
  HostLink hostLink;

  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);

  // Load code and trigger execution
  log.tag_leaf("boot");
  hostLink.boot("code.v", "data.v");
  log.tag_leaf("go");
  hostLink.go();

  log.tag_leaf("running");

  // Start timer
  struct timeval start, finish, diff;
  gettimeofday(&start, NULL);

  std::vector<std::vector<HeatMessage>> results(n, std::vector<HeatMessage>(n));

  unsigned max_generation=0;

  volatile bool quit=false;

  std::thread dumper([&](){
    while(!quit){
      while(hostLink.pollStdOut(stderr));
      usleep(1000);
    }
  });

  // Receive final value of each device
  for (uint32_t i = 0; i < n*n; i++) {
    

    // Receive message
    PMessage<HeatMessage> msg;
    hostLink.recvMsg(&msg, sizeof(msg));
    if (i == 0){
      fprintf(stderr, "Got first results\n");
      gettimeofday(&finish, NULL);
      log.tag_leaf("collecting");
    }
    // Save final value
    results.at(msg.payload.x).at(msg.payload.y) = msg.payload;
    /*fprintf(stderr, "x=%d, y=%d, v=%f, sv=%f, g={%f,%f,%f,%f}, sent=%d, recv=%d\n",
      msg.payload.x, msg.payload.y, from_heat(msg.payload.val), from_heat(msg.payload.sent_heat),
      from_heat(msg.payload.ghosts[0]), from_heat(msg.payload.ghosts[1]), from_heat( msg.payload.ghosts[2]), from_heat( msg.payload.ghosts[3]),
      msg.payload.sent, msg.payload.recv
    );
    */
  

    max_generation=std::max<unsigned>(max_generation, msg.payload.generation);
  }

  log.tag_leaf("exporting");

  quit=true;
  dumper.join();

  fprintf(stderr, "max_generation=%d\n", max_generation);

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  #ifndef POLITE_DUMP_STATS
  printf("Time = %lf\n", duration);
  #endif

  // Emit image
  FILE* fp = fopen("heat.ppm", "wt");
  if (fp == NULL) {
    printf("Can't open output file for writing\n");
    return -1;
  }
  fprintf(fp, "P3\n%d %d\n255\n", n, n);

  float scale=255.0f / force;
  for (uint32_t y = 0; y < n; y++)
    for (uint32_t x = 0; x < n; x++) {
      int val = from_heat(results[x][y].val) * scale;
      if(val < 0){
        fprintf(fp, "%d %d %d\n", std::min(255, -val), 0, 0);
      }else{
        fprintf(fp, "%d %d %d\n", 0, std::min(255, val), 0 );
      }
    }
  fclose(fp);



  fp = fopen("generations.pgm", "wt");
  if (fp == NULL) {
    printf("Can't open output file for writing\n");
    return -1;
  }
  fprintf(fp, "P2\n%d %d\n%d\n", n, n, max_generation+1);

  for (uint32_t y = 0; y < n; y++)
    for (uint32_t x = 0; x < n; x++) {
      int val = results[x][y].generation;
      fprintf(fp, "%d\n", val);
    }
  fclose(fp);

  return 0;
}
