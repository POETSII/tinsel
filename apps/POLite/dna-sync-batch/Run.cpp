// SPDX-License-Identifier: BSD-2-Clause
#include "impute.h"
#include "model.h"

#include <HostLink.h>
#include <POLite.h>
#include <EdgeList.h>
#include <sys/time.h>

/*****************************************************
 * Genomic Imputation - Batch Processing Version
 * ***************************************************
 * This code streams target haplotypes from the X86 side
 * USAGE:
 * To Be Completed ...
 * 
 * PLEASE NOTE:
 * To Be Completed ...
 * 
 * ssh jordmorr@ayres.cl.cam.ac.uk
 * scp -r C:\Users\drjor\Documents\tinsel\apps\POLite\dna-sync-batch jordmorr@ayres.cl.cam.ac.uk:~/tinsel/apps/POLite
 * scp jordmorr@ayres.cl.cam.ac.uk:~/tinsel/apps/POLite/dna-sync-batch/build/stats.txt C:\Users\drjor\Documents\tinsel\apps\POLite\dna-sync-batch
 * ****************************************************/

int main(int argc, char **argv)
{
    
    uint32_t lower_count = 1u;
    uint64_t upper_count = 1u;
    double total_time = 0.0f;
    
    // Start timer for mesh creation and mapping
    struct timeval start_map, finish_map, diff_map;
    gettimeofday(&start_map, NULL);
    
    // Connection to tinsel machine
    HostLink hostLink;

    // Create POETS graph
    PGraph<ImpDevice, ImpState, None, ImpMessage> graph;

    // Create 2D mesh of devices
    static PDeviceId mesh[NOOFSTATES][NOOFOBS];
    for (uint32_t y = 0; y < NOOFSTATES; y++) {
        for (uint32_t x = 0; x < NOOFOBS; x++) {
            
                mesh[y][x] = graph.newDevice();
                
        }
    }
    
    

    // Add induction edges
    // Forward Connections
    for (uint32_t x = 0; x < NOOFOBS-1; x++) {
        for (uint32_t y = 0; y < NOOFSTATES; y++) {
            
            for (uint32_t z = 0; z < NOOFSTATES; z++) {
                graph.addEdge(mesh[y][x], 0, mesh[z][x+1]);
            }

        }
    }
    
    // Backward Connections
    for (uint32_t x = 1; x < NOOFOBS; x++) {
        for (uint32_t y = 0; y < NOOFSTATES; y++) {
            
            for (uint32_t z = 0; z < NOOFSTATES; z++) {
                graph.addEdge(mesh[y][x], 1, mesh[z][x-1]);
            }

        }
    }
    
    // Add termination edges
    for (uint32_t y = 0; y < NOOFSTATES-1; y++) {
        graph.addEdge(mesh[y][NOOFOBS-1], 2, mesh[NOOFSTATES-1][NOOFOBS-1]);
    }

    // Prepare mapping from graph to hardware
    graph.mapVerticesToDRAM = true;
    graph.map();
    
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
            graph.devices[mesh[y][x]]->state.id = mesh[y][x];
            
            // Initialise Mesh coordinates on devices
            graph.devices[mesh[y][x]]->state.x = x;
            graph.devices[mesh[y][x]]->state.y = y;

            //Inform each device of matrix size for message passing decisions
            graph.devices[mesh[y][x]]->state.xmax = NOOFOBS;
            graph.devices[mesh[y][x]]->state.ymax = NOOFSTATES;
            
            // Initialise Sentflags
            graph.devices[mesh[y][x]]->state.sentflags = 0;
            
            // Initialise Counters
            graph.devices[mesh[y][x]]->state.aindreccount = 0;
            graph.devices[mesh[y][x]]->state.bindreccount = 0;
            graph.devices[mesh[y][x]]->state.finreccount = 0;
            
            // Initialise known values
            if (x == 0) {
                graph.devices[mesh[y][x]]->state.initprob = init_prob[y];
            }
            if (x == NOOFOBS-1) {
                graph.devices[mesh[y][x]]->state.initprob = 1;
            }
            
            graph.devices[mesh[y][x]]->state.transprob = (1.0 / NOOFSTATES);
            
            graph.devices[mesh[y][x]]->state.emisprob = 0.9999;

            // Initialise Values
            graph.devices[mesh[y][x]]->state.alpha = 0.0;
            graph.devices[mesh[y][x]]->state.beta = 0.0;
            graph.devices[mesh[y][x]]->state.posterior = 1.0;
            graph.devices[mesh[y][x]]->state.answer = 0.0;
            
        }
    }

    // Write graph down to tinsel machine via HostLink
    graph.write(&hostLink);

    // Load code and trigger execution
    hostLink.boot("code.v", "data.v");
    hostLink.go();
    printf("Starting\n");
  
      // Record init time
    gettimeofday(&finish_init, NULL);
    timersub(&finish_init, &start_init, &diff_init);
    double init_duration = (double) diff_init.tv_sec + (double) diff_init.tv_usec / 1000000.0;
    
    // Start timer for overall processing
    struct timeval start_proc, finish_proc, diff_proc;
    gettimeofday(&start_proc, NULL);  
    
    #ifndef IMPDEBUG
        //JPMREAL
        // Allocate array to contain final value of each device
        //float result = 0.0f;

        // Receive message
        PMessage<None, ImpMessage> msg;
        hostLink.recvMsg(&msg, sizeof(msg));
        gettimeofday(&finish_proc, NULL);
        
        // Save final value
        //result = msg.payload.val;
        
        lower_count = msg.payload.val;
        upper_count = msg.payload.msgtype;

        timersub(&finish_proc, &start_proc, &diff_proc);
        double proc_duration = (double) diff_proc.tv_sec + (double) diff_proc.tv_usec / 1000000.0;
        
        //total_time = ((upper_count << 32) + lower_count) / (TinselClockFreq * 1000000.0);
           
        //printf("%f\n", result);
        #ifdef CYCDEBUG
        //JPM CYCLE DEBUG
        printf("%ld,%d,%lf", upper_count, lower_count, total_time);
        #else
        //JPM STANDARD DEBUG
        printf("%lf,%lf,%lf", map_duration, init_duration, total_time);
        #endif

    #endif
    
    #ifdef IMPDEBUG
        //JPMDEBUG
        // Allocate array to contain final value of each device
        float result[NOOFSTATES][NOOFOBS] {};

        // Receive final value of each device
        for (uint32_t i = 0; i < (NOOFSTATES*NOOFOBS); i++) {
            
            //printf("%d received \n", i);
            
            // Receive message
            PMessage<None, ImpMessage> msg;
            hostLink.recvMsg(&msg, sizeof(msg));
            if (i == 0) gettimeofday(&finish_proc, NULL);

            // Save final value
            result[graph.devices[msg.payload.msgtype]->state.y][graph.devices[msg.payload.msgtype]->state.x] = msg.payload.val;
            
        }

        // Display time
        timersub(&finish_proc, &start_proc, &diff_proc);
        double proc_duration = (double) diff_proc.tv_sec + (double) diff_proc.tv_usec / 1000000.0;

        for (uint32_t y = 0; y < NOOFSTATES; y++) {
            for (uint32_t x = 0; x < NOOFOBS; x++) {
                
                printf("%f ", result[y][x]);
                
            }
            
            printf("\n");
            
        }
    #endif

    // Consume performance stats
    politeSaveStats(&hostLink, "stats.txt");

    printf("Finished\n");
    
    return 0;
}
