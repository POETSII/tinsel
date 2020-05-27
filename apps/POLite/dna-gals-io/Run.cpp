// SPDX-License-Identifier: BSD-2-Clause
#include "matrixmult.h"
#include "matrices.h"

#include <HostLink.h>
#include <POLite.h>
#include <sys/time.h>

/*****************************************************
 * Matrix Multiplier - Synchronous - IO Stream
 * ***************************************************
 * This code multiplies the matrices specificfied in matrices.cpp.
 * USAGE:
 * 1. Matrix A and Matrix B to be defined in matrices.cpp
 * 2. Matrix A and Matrix B dimensions to be defined in matrices.h.
 * 
 * PLEASE NOTE:
 * Implementation checks whether mulitplication is possible.
 * (Matrix A Cols == Matrix B Rows)
 * ****************************************************/

int main() {
        
    // Start timer for mesh creation and mapping
    struct timeval start_map, finish_map, diff_map;
    gettimeofday(&start_map, NULL);


    // Connection to tinsel machine
    HostLink *host_link = new HostLink(1, 1);


    // Create POETS graph
    PGraph <MatDevice, MatState, None, MatMessage>*graph = new PGraph<MatDevice, MatState, None, MatMessage>(1, 1);


    // Create 2D mesh of devices
    static PDeviceId mesh[NOOFSTATES][NOOFOBS];
    for (uint32_t y = 0; y < NOOFSTATES; y++) {
        for (uint32_t x = 0; x < NOOFOBS; x++) {
                
                mesh[y][x] = graph->newDevice();
                
        }
    }
    
    // Add edges
    for (uint32_t x = 0; x < NOOFOBS-1; x++) {
        for (uint32_t y = 0; y < NOOFSTATES; y++) {
            
            for (uint32_t z = 0; z < NOOFSTATES; z++) {
                graph->addEdge(mesh[y][x], z, mesh[z][x+1]);
            }

        }
    } 

    // Prepare mapping from graph to hardware
    graph->mapVerticesToDRAM = true;
    graph->map();
    
    // Record map time
    gettimeofday(&finish_map, NULL);
    timersub(&finish_map, &start_map, &diff_map);
    double map_duration = (double) diff_map.tv_sec + (double) diff_map.tv_usec / 1000000.0;
    
    // Start timer for mesh init
    struct timeval start_init, finish_init, diff_init;
    gettimeofday(&start_init, NULL);

    // Initialise device coordinates/dimensions
    for (uint32_t y = 0; y < NOOFSTATES; y++) {
        for (uint32_t x = 0; x < NOOFOBS; x++) {
                
            // Initialise device IDs
            graph->devices[mesh[y][x]]->state.id = mesh[y][x];
            
            // Initialise Mesh coordinates on devices
            graph->devices[mesh[y][x]]->state.x = x;
            graph->devices[mesh[y][x]]->state.y = y;

            //Inform each device of matrix size for message passing decisions
            graph->devices[mesh[y][x]]->state.xmax = NOOFOBS;
            graph->devices[mesh[y][x]]->state.ymax = NOOFSTATES;
            
            //Initialise Message Pointer and Matrix Elements
            //graph->devices[mesh[y][x]]->state.inflags = 0;
            //graph->devices[mesh[y][x]]->state.sentflags = 0;
            
        }
    }

    // Write graph down to tinsel machine via HostLink
    graph->write(host_link);

    // Load code and trigger execution
    host_link->boot("code.v", "data.v");
    host_link->go();
    //printf("Starting\n");
    
    // Record init time
    gettimeofday(&finish_init, NULL);
    timersub(&finish_init, &start_init, &diff_init);
    double init_duration = (double) diff_init.tv_sec + (double) diff_init.tv_usec / 1000000.0;

    // Start timer for sending
    struct timeval start_send, finish_send, diff_send;
    gettimeofday(&start_send, NULL);

    // Start timer for overall processing
    struct timeval start_proc, finish_proc, diff_proc;
    gettimeofday(&start_proc, NULL);

    int deviceAddr = 0;
    
    for (uint32_t y = 0; y < NOOFSTATES; y++) {

        // Construct messages -> One same element from each matrix
        PMessage<None, MatMessage> sendMsg;

        // From maxtrix A
        deviceAddr = graph->toDeviceAddr[mesh[y][0]];
        sendMsg.devId = getLocalDeviceId(deviceAddr);
        sendMsg.payload.dir = EXTERNALX;
        sendMsg.payload.val = matrixA[w][h];
        host_link->send(getThreadId(deviceAddr), 2, &sendMsg);
        //printf("Sent %d to node [0][%d][%d]\n", sendMsg.payload.element1, w, h);

    }
    
    // Record send time
    gettimeofday(&finish_send, NULL);
    timersub(&finish_send, &start_send, &diff_send);
    double send_duration = (double) diff_send.tv_sec + (double) diff_send.tv_usec / 1000000.0;

    // Allocate array to contain final value of each device
    uint32_t result[MESHLEN][MESHWID] {};

    // Receive final value of each device
    for (uint32_t i = 0; i < RETMATSIZE; i++) {
        
        // Receive message
        PMessage<None, MatMessage> msg;
        host_link->recvMsg(&msg, sizeof(msg));
        if (i == 0) gettimeofday(&finish_proc, NULL);

        // Save final value
        result[graph->devices[msg.payload.dir]->state.x][graph->devices[msg.payload.dir]->state.y] = msg.payload.val;
        
    }

    // Display time
    timersub(&finish_proc, &start_proc, &diff_proc);
    double proc_duration = (double) diff_proc.tv_sec + (double) diff_proc.tv_usec / 1000000.0;

    for (uint32_t y = 0; y < MESHWID; y++) {
        for (uint32_t x = 0; x < MESHLEN; x++) {
            
            printf("%d ", result[x][y]);
            
        }
        
        printf("\n");
        
    }
    
    printf("%lf,%lf,%lf,%lf", map_duration, init_duration, send_duration, proc_duration);
    
    return 0;

}
