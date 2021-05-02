// SPDX-License-Identifier: BSD-2-Clause
#include "HeatRelax.h"

#include <HostLink.h>
#include <POLite.h>
#include <EdgeList.h>
#include <sys/time.h>

int main(int argc, char **argv)
{
  int n=10;
  float thresh=0.01;
  float start_thresh=thresh;
  float scale_thresh=0.1;
  float force=30;
  float omega=1.5;

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
    omega=strtod(argv[4], nullptr);
  }
  if(argc>5){
    start_thresh=strtod(argv[5], nullptr);
  }
  if(argc>6){
    scale_thresh=strtod(argv[6], nullptr);
  }

  fprintf(stderr, "size=%dx%d, threshold=%g, force_temperature=%g, omega=%g\n",
    n, n, thresh, force, omega
    );

  fprintf(stderr, "start_thresh=%g, scale_thresh=%g\n", start_thresh, scale_thresh);

  PGraph<HeatDevice, HeatState, None, HeatMessage> graph;

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

  for(int x=0; x<n; x++){
    for(int y=0; y<n; y++){
      if(x<n-1) connect(ids[x][y], ids[x+1][y]);
      if(y<n-1) connect(ids[x][y], ids[x][y+1]);

      auto &here=graph.devices[ids[x][y]]->state;
      int edges=0;
      edges += x==0;
      edges += x==n-1;
      edges += y==0;
      edges += y==n-1;
      here.sent_heat=0;
      here.generation=0;
      if( edges==2 ){
        here.is_fixed=true;
        if(x==y){
          here.curr_heat = +force;
        }else{
          here.curr_heat = -force;
        }
        fprintf(stderr, "x=%d, y=%d, heat=%f\n", x, y, here.curr_heat);
      }else{
        here.curr_heat=0;
      }
      here.scale= (edges==0) ? 0.25f : (edges==1) ? (1.0f/3.0f) : 0.5f;
      here.tolerance=start_thresh;
      here.tolerance_scale=scale_thresh;
      here.min_tolerance=thresh;
      here.colour=colours[ x%8 ][ y%8 ]-'0';
      here.x=x;
      here.y=y;
      here.omega=omega;
    }
  }

  // Connection to tinsel machine
  HostLink hostLink;

  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");
  hostLink.go();
  printf("Starting\n");

  // Start timer
  struct timeval start, finish, diff;
  gettimeofday(&start, NULL);

  std::vector<std::vector<HeatMessage>> results(n, std::vector<HeatMessage>(n));

  unsigned max_generation=0;

  // Receive final value of each device
  for (uint32_t i = 0; i < n*n; i++) {
    // Receive message
    PMessage<HeatMessage> msg;
    hostLink.recvMsg(&msg, sizeof(msg));
    if (i == 0) gettimeofday(&finish, NULL);
    // Save final value
    results.at(msg.payload.x).at(msg.payload.y) = msg.payload;
    //fprintf(stderr, "x=%d, y=%d, v=%f\n", msg.payload.x, msg.payload.y, msg.payload.val);

    max_generation=std::max<unsigned>(max_generation, msg.payload.generation);
  }

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
      int val = results[x][y].val * scale;
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
