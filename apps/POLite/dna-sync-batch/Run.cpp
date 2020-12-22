// SPDX-License-Identifier: BSD-2-Clause
#include "impute.h"
#include "model.h"
#include "myPOLite.h"
#include "params.h"

#include <HostLink.h>
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
 * ssh jordmorr@byron.cl.cam.ac.uk
 * scp -r C:\Users\drjor\Documents\tinsel\apps\POLite\dna-sync-batch jordmorr@byron.cl.cam.ac.uk:~/tinsel/apps/POLite
 * scp jordmorr@byron.cl.cam.ac.uk:~/tinsel/apps/POLite/dna-sync-batch/build/results.csv C:\Users\drjor\Documents\tinsel\apps\POLite\dna-sync-batch
 * ****************************************************/

int main(int argc, char **argv)
{
    
    // Start timer for mesh creation and mapping
    struct timeval start_map, finish_map, diff_map;
    gettimeofday(&start_map, NULL);
    
    // Connection to tinsel machine
    //HostLink hostLink;
    HostLink hostLink(2, 4);

    // Create POETS graph
    //PGraph<ImpDevice, ImpState, None, ImpMessage> graph;
    PGraph<ImpDevice, ImpState, None, ImpMessage> graph(2, 4);

    // Create 2D mesh of devices
    static PDeviceId mesh[NOOFSTATES][NOOFOBS];
    for (uint32_t y = 0; y < NOOFSTATES; y++) {
        for (uint32_t x = 0; x < NOOFOBS; x++) {
            
                mesh[y][x] = graph.newDevice();
                
        }
    }
    
    
    
    // Add induction edges
    // Forward Connections
    for (uint32_t x = 0; x < NOOFOBS - 1; x++) {
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
  
    // Accumulation Messages
    for (uint32_t x = 0; x < NOOFOBS; x++) {
        for (uint32_t y = 0; y < NOOFSTATES - 1; y++) {
            
            graph.addEdge(mesh[y][x], 2, mesh[NOOFSTATES - 1][x]);
            
        }
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
    for (uint32_t x = 0; x < NOOFOBS; x++) {
        
        float tau_m0 = 0.0f;
        float same0 = 0.0f;
        float diff0 = 0.0f;
        
        // Tau M Values
        if ((x != (NOOFOBS - 1u))) {
            
            tau_m0 = (1 - exp((-4 * NE * dm[x]) / NOOFSTATES));
            same0 = (1 - tau_m0) + (tau_m0 / NOOFSTATES);
            diff0 = tau_m0 / NOOFSTATES;
            
            
        }
        
        float tau_m1 = 0.0f;
        float same1 = 0.0f;
        float diff1 = 0.0f;
        
        if (x != 0u) {
            
            tau_m1 = (1 - exp((-4 * NE * dm[x - 1u]) / NOOFSTATES));
            same1 = (1 - tau_m1) + (tau_m1 / NOOFSTATES);
            diff1 = tau_m1 / NOOFSTATES;
            
        }
        
        for (uint32_t y = 0; y < NOOFSTATES; y++) {
                
            // Initialise device IDs
            graph.devices[mesh[y][x]]->state.id = mesh[y][x];
            
            // Initialise Mesh coordinates on devices
            graph.devices[mesh[y][x]]->state.x = x;
            graph.devices[mesh[y][x]]->state.y = y;
            
            // Initialise Label Values
            graph.devices[mesh[y][x]]->state.label = hmm_labels[y][x];
            
            uint32_t match = 0u;
            
            if (observation[x][1] == 2u) {
                match = 2u;
            }
            else {
                if (hmm_labels[y][x] == observation[x][1]) {
                    match = 1u;
                }
            }
            
            // Initialise Match Value
            graph.devices[mesh[y][x]]->state.match = match;
            
            // Initialise Transition Probabilities
            graph.devices[mesh[y][x]]->state.bwdSame = same0;
            graph.devices[mesh[y][x]]->state.bwdDiff = diff0;
            graph.devices[mesh[y][x]]->state.fwdSame = same1;
            graph.devices[mesh[y][x]]->state.fwdDiff = diff1;
            
            // Initialise Posterior Values
            for (uint32_t i = 0u; i < NOOFTARG; i++) {
                graph.devices[mesh[y][x]]->state.posterior[i] = 1.0f;
            }
            
            
            
        }
    }
    
    // Write graph down to tinsel machine via HostLink
    graph.write(&hostLink);

    // Load code and trigger execution
    hostLink.boot("code.v", "data.v");
    hostLink.go();
    printf("Starting\n");
    
    // Consume performance stats
    //politeSaveStats(&hostLink, "stats.txt");
  
      // Record init time
    gettimeofday(&finish_init, NULL);
    timersub(&finish_init, &start_init, &diff_init);
    double init_duration = (double) diff_init.tv_sec + (double) diff_init.tv_usec / 1000000.0;
    
    // Start timer for overall processing
    struct timeval start_proc, finish_proc, diff_proc;
    gettimeofday(&start_proc, NULL); 
    
#ifdef IMPDEBUG
        
        /*
        // Allocate array to contain final value of each device
        static float result[NOOFOBS] {};

        // Receive final value of each device
        for (uint32_t i = 0; i < (NOOFOBS); i++) {
            
            //printf("%d received \n", i);
            
            // Receive message
            PMessage<ImpMessage> msg;
            hostLink.recvMsg(&msg, sizeof(msg));

            // Save final value
            result[graph.devices[msg.payload.msgtype]->state.x] = msg.payload.val;
            
        }

        //Create a file pointer
        FILE * fp;
        // open the file for writing
        fp = fopen ("results.csv","w");

        uint32_t alphaSelect = 1u;
        
        if (alphaSelect) {

            for (uint32_t x = 0u; x < NOOFOBS; x++) {
            
                if (x != (NOOFOBS - 1u) ) {
                    fprintf(fp, "%e,", result[x]);
                }
                else {
                    fprintf(fp, "%e", result[x]);
                }
            
            }
                
        }
        else {
            
            for (uint32_t x = 0u; x < NOOFOBS; x++) {
        
                if (x != (NOOFOBS - 1u) ) {
                    fprintf(fp, "%e,", result[(NOOFOBS - 1) - x]);
                }
                else {
                    fprintf(fp, "%e", result[(NOOFOBS - 1) - x]);
                }
        
            }  

        }
        
        // close the file 
        fclose (fp);
        */
        
        static float result[NOOFSTATES][NOOFOBS] {};

        // Receive final value of each device
        for (uint32_t i = 0; i < (NOOFSTATES*NOOFOBS); i++) {
            
            // Receive message
            PMessage<ImpMessage> msg;
            hostLink.recvMsg(&msg, sizeof(msg));

            // Save final value
            result[graph.devices[msg.payload.msgtype]->state.y][graph.devices[msg.payload.msgtype]->state.x] = msg.payload.val;
            
        }

        //Create a file pointer
        FILE * fp;
        // open the file for writing
        fp = fopen ("results.csv","w");
        
        uint32_t alphaSelect = 1u;
        
        if (alphaSelect) {
        
            for (uint32_t y = 0u; y < NOOFSTATES; y++) {
                for (uint32_t x = 0u; x < NOOFOBS; x++) {
                
                    if (x != (NOOFOBS - 1u) ) {
                        fprintf(fp, "%e,", result[y][x]);
                    }
                    else {
                        fprintf(fp, "%e", result[y][x]);
                    }
                
                }
                fprintf(fp, "\n");
            }
        }
        else {
            for (uint32_t y = 0u; y < NOOFSTATES; y++) {
                for (uint32_t x = 0u; x < NOOFOBS; x++) {
                
                    if (x != (NOOFOBS - 1u) ) {
                        fprintf(fp, "%e,", result[y][(NOOFOBS - 1) - x]);
                    }
                    else {
                        fprintf(fp, "%e", result[y][(NOOFOBS - 1) - x]);
                    }
                
                }
                fprintf(fp, "\n");
            }
        }
        // close the file 
        fclose (fp);
       
#else

    // Receive first message and close timer
    PMessage<ImpMessage> msg;
    hostLink.recvMsg(&msg, sizeof(msg));
    
    gettimeofday(&finish_proc, NULL);
    timersub(&finish_proc, &start_proc, &diff_proc);
    double proc_duration = (double) diff_proc.tv_sec + (double) diff_proc.tv_usec / 1000000.0;
    printf("%lf\n", proc_duration);
        
#endif

    printf("Finished\n");
    
    return 0;
}
