// SPDX-License-Identifier: BSD-2-Clause
#include "matrixmult.h"
#include "matrices.h"

#include <HostLink.h>
#include <POLite.h>
#include <sys/time.h>

/*****************************************************
 * Matrix Multiplier - Asynchronous - Batch - Isotropic Messages
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
    //HostLink::HostLink(2, 2);
    HostLink *host_link = new HostLink(1, 1);
    //Sampleclass *qs = new Sampleclass()

    // Create POETS graph
    //PGraph<MatDevice, MatState, None, MatMessage>(2, 2) graph;
    
    PGraph <MatDevice, MatState, None, MatMessage>*graph = new PGraph<MatDevice, MatState, None, MatMessage>(1, 1);

    // Create 2D mesh of devices
    static PDeviceId mesh[NOOFSTATES][NOOFOBS];
    for (uint32_t y = 0; y < NOOFSTATES; y++) {
        for (uint32_t x = 0; x < NOOFOBS; x++) {
                
                mesh[y][x] = graph->newDevice();
                
        }
    }
    
    // Add induction edges
    for (uint32_t x = 0; x < NOOFOBS-1; x++) {
        for (uint32_t y = 0; y < NOOFSTATES; y++) {
            
            for (uint32_t z = 0; z < NOOFSTATES; z++) {
                graph->addEdge(mesh[y][x], z, mesh[z][x+1]);
            }

        }
    }
    
    // Add termination edges
    for (uint32_t y = 0; y < NOOFSTATES-1; y++) {
        graph->addEdge(mesh[y][NOOFOBS-1], 0, mesh[NOOFSTATES-1][NOOFOBS-1])
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
            
            // Initialise Sentflags
            graph->devices[mesh[y][x]]->state.sentflags = 0;
            
            // Initialise Counters
            graph->devices[mesh[y][x]]->state.indereccount = 0;
            graph->devices[mesh[y][x]]->state.finreccount = 0;
            graph->devices[mesh[y][x]]->state.sendcount = 0;
            
            //Initialise Message Pointer and Matrix Elements
            if (x == 0) {
                graph->devices[mesh[y][x]]->state.initprob = init_prob[y];
            }
            
            graph->devices[mesh[y][x]]->state.transprob = 1 / NOOFSTATES;
            graph->devices[mesh[y][x]]->state.emisprob = 0.9999;

            // Initialise Values
            graph->devices[mesh[y][x]]->state.alpha = 0;
            graph->devices[mesh[y][x]]->state.answer = 0;
            
        }
    }

    // Write graph down to tinsel machine via HostLink
    graph->write(host_link);

    // Load code and trigger execution
    host_link->boot("code.v", "data.v");
    host_link->go();
    // printf("Starting\n");
    
    // Record init time
    gettimeofday(&finish_init, NULL);
    timersub(&finish_init, &start_init, &diff_init);
    double init_duration = (double) diff_init.tv_sec + (double) diff_init.tv_usec / 1000000.0;
    
    // Start timer for overall processing
    struct timeval start_proc, finish_proc, diff_proc;
    gettimeofday(&start_proc, NULL);        

    // Allocate array to contain final value of each device
    float result = 0.0f;

    // Receive message
    PMessage<None, MatMessage> msg;
    host_link->recvMsg(&msg, sizeof(msg));
    gettimeofday(&finish_proc, NULL);
    
    // Save final value
    result = msg.payload.val;

    timersub(&finish_proc, &start_proc, &diff_proc);
    double proc_duration = (double) diff_proc.tv_sec + (double) diff_proc.tv_usec / 1000000.0;
       
    printf("%f\n", result);
    
    printf("%lf,%lf,%lf", map_duration, init_duration, proc_duration);
    
    return 0;

}
