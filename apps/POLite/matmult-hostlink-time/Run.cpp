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
    
    if (!mult_possible) {
        printf("Multiplication not possible with supplied matrices!\n");
    }
    else {
        
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

        // Create 3D mesh of devices
        static PDeviceId mesh;
     
        mesh = graph->newDevice();
        
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
                    
        // Initialise device IDs
        graph->devices[mesh]->state.id = mesh;
        
        // Initialise Mesh coordinates on devices
        graph->devices[mesh]->state.x = 0;
        graph->devices[mesh]->state.y = 0;
        graph->devices[mesh]->state.z = 0;

        //Inform each device of matrix size for message passing decisions
        graph->devices[mesh]->state.xmax = 0;
        graph->devices[mesh]->state.ymax = 0;
        graph->devices[mesh]->state.zmax = 0;
        
        //Clear message reception flags
        graph->devices[mesh]->state.inflags = 0;
        
        graph->devices[mesh]->state.element1 = 1;
        graph->devices[mesh]->state.element2 = 1;
                    

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
        
        // Allocate array to contain final value of each device
        uint32_t result {};
        
        // Start timer for overall processing
        struct timeval start_proc, finish_proc, diff_proc;
        gettimeofday(&start_proc, NULL);        

        // Receive message
        PMessage<None, MatMessage> msg;
        host_link->recvMsg(&msg, sizeof(msg));
        gettimeofday(&finish_proc, NULL);
        
        // Save final value
        result = msg.payload.val;

        timersub(&finish_proc, &start_proc, &diff_proc);
        double proc_duration = (double) diff_proc.tv_sec + (double) diff_proc.tv_usec / 1000000.0;
        
        printf("%lf,%lf,%lf", map_duration, init_duration, proc_duration);
    
    }
    
    return 0;

}
