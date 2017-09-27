#include <HostLink.h>
#include "PMesh2D.h"
#include "Heat.h"
#include "Colours.h"

int main()
{
  // Parameters
  const uint32_t width  = 16;
  const uint32_t height = 8;
  const uint32_t time   = 2;

  // Connection to tinsel machine
  HostLink hostLink;

  // Create virtual 2D mesh of heat devices
  PMesh2D<HeatDevice, HeatMessage> mesh(width, height);

  // Specify number of time steps to run on each device
  for (uint32_t y = 0; y < mesh.height; y++)
    for (uint32_t x = 0; x < mesh.width; x++)
      mesh.devices[y][x]->t = time;

  // Apply constant heat at north edge
  // Apply constant cool at south edge
  for (uint32_t x = 0; x < mesh.width; x++) {
    mesh.devices[0][x]->val = 255 << 16;
    mesh.devices[0][x]->isConstant = true;
    mesh.devices[mesh.height-1][x]->val = 40 << 16;
    mesh.devices[mesh.height-1][x]->isConstant = true;
  }

  // Apply constant heat at west edge
  // Apply constant cool at east edge
  for (uint32_t y = 0; y < mesh.height; y++) {
    mesh.devices[y][0]->val = 255 << 16;
    mesh.devices[y][0]->isConstant = true;
    mesh.devices[y][mesh.width-1]->val = 40 << 16;
    mesh.devices[y][mesh.width-1]->isConstant = true;
  }

  // Map mesh down to tinsel machine via HostLink
  mesh.map(&hostLink);
 
  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");
  hostLink.go();

  // Allocate 2D mesh to contain final value of each device
  uint32_t image[mesh.height][mesh.width];

  // Receive final value of each device
  uint32_t numDevices = mesh.width * mesh.height;
  for (uint32_t i = 0; i < numDevices; i++) {
    // Receive message
    HeatMessage msg;
    hostLink.recv(&msg);
    // Determine x and y coords of sending device
    PMesh2DId coords =
      mesh.fromThreadId[msg.from.threadId][msg.from.localId];
    // Save final value
    image[coords.y][coords.x] = msg.val;
  }

  // Emit image
  printf("P3\n%d %d\n255\n", mesh.width, mesh.height);
  for (uint32_t y = 0; y < mesh.height; y++)
    for (uint32_t x = 0; x < mesh.width; x++) {
      uint32_t val = (image[y][x] >> 16) & 0xff;
      printf("%d %d %d\n",
        colours[val*3], colours[val*3+1], colours[val*3+2]);
    }

  return 0;
}
