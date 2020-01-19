// SPDX-License-Identifier: BSD-2-Clause
#include "matrixmult.h"
#include "matrices.h"

#include <HostLink.h>
#include <POLite.h>
#include <sys/time.h>
#include <vector>

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

    // Create 3D mesh of devices
    static PDeviceId mesh[QUERYLENGTH + 1][SUBLENGTH + 1];

    for (uint32_t x = 0; x < (QUERYLENGTH + 1); x++) {
        for (uint32_t y = 0; y < (SUBLENGTH + 1); y++) {
                
                mesh[x][y] = graph->newDevice();
                
        }
    }
    
    // Add normal edges in smith-waternam graph and unless last node add traceback initialisation edges
    for (uint32_t x = 0; x < (QUERYLENGTH + 1); x++) {
        for (uint32_t y = 0; y < (SUBLENGTH + 1); y++) {
                
                if (x < QUERYLENGTH) {
                    graph->addEdge(mesh[x][y], 0, mesh[x+1][y]);
                    graph->addEdge(mesh[x+1][y], 3, mesh[x][y]);
                }
                if (y < SUBLENGTH) {
                    graph->addEdge(mesh[x][y], 1, mesh[x][y+1]);
                    graph->addEdge(mesh[x][y+1], 4, mesh[x][y]);
                }
                if ((x < QUERYLENGTH) && (y < SUBLENGTH)) {
                    graph->addEdge(mesh[x][y], 2, mesh[x+1][y+1]);
                    graph->addEdge(mesh[x+1][y+1], 5, mesh[x][y]);
                }
                if ( (x < (QUERYLENGTH)) || (y < (SUBLENGTH)) ) {
                    graph->addEdge(mesh[QUERYLENGTH][SUBLENGTH], ((y * (QUERYLENGTH + 1) + x + 6)), mesh[x][y]);
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
    for (uint32_t x = 0; x < (QUERYLENGTH + 1); x++) {
        for (uint32_t y = 0; y < (SUBLENGTH + 1); y++) {
                
            // Initialise device IDs
            graph->devices[mesh[x][y]]->state.id = mesh[x][y];
            
            // Initialise Mesh coordinates on devices
            graph->devices[mesh[x][y]]->state.x = x;
            graph->devices[mesh[x][y]]->state.y = y;

            //Inform each device of matrix size for message passing decisions
            graph->devices[mesh[x][y]]->state.xmax = (QUERYLENGTH + 1);
            graph->devices[mesh[x][y]]->state.ymax = (SUBLENGTH + 1);
            
            // Initialise gap primer and nucleotide sequences
            
            if (x == 0) {
                graph->devices[mesh[x][y]]->state.query = '-';
                
                graph->devices[mesh[x][y]]->state.element1 = 0;
                graph->devices[mesh[x][y]]->state.element2 = 0;
                graph->devices[mesh[x][y]]->state.element3 = 0;
                
                graph->devices[mesh[x][y]]->state.largestx = x;
                graph->devices[mesh[x][y]]->state.largesty = y;
                
            }
            else {
                graph->devices[mesh[x][y]]->state.query = seqQuery[x - 1];
            }
            
            if (y == 0) {
                graph->devices[mesh[x][y]]->state.subject = '-';
                
                graph->devices[mesh[x][y]]->state.element1 = 0;
                graph->devices[mesh[x][y]]->state.element2 = 0;
                graph->devices[mesh[x][y]]->state.element3 = 0;
                
                graph->devices[mesh[x][y]]->state.largestx = x;
                graph->devices[mesh[x][y]]->state.largesty = y;
            
            }
            else {
                graph->devices[mesh[x][y]]->state.subject = seqSub[y - 1];
            }
            
            // Initialise aggregates
            graph->devices[mesh[x][y]]->state.aggregate = 0;
            
            // Initialise inflags and sentflags
            graph->devices[mesh[x][y]]->state.inflags = 0;
            graph->devices[mesh[x][y]]->state.sentflags = 0;
            
            // Initialise largest element direction
            graph->devices[mesh[x][y]]->state.largestdir = 0;
            
            // Initialise largest element aggreagte seen
            graph->devices[mesh[x][y]]->state.largestagg = 0;
            
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
    //uint32_t result[(SUBLENGTH + 1)][(QUERYLENGTH + 1)] {};
    
    // Create a vector containing integers
    std::vector<char> aligned_query;
    
    // Create first non-zero aggregate
    uint32_t aggregate = 1;
    bool time_got = false;
    
    printf("everything mapped\n\n");
    
    /*
    // Receive final value of each device
    for (uint32_t i = 0; i < ( (QUERYLENGTH + 1) * (SUBLENGTH + 1) ); i++) {
        
        // Receive message
        PMessage<None, MatMessage> msg;
        host_link->recvMsg(&msg, sizeof(msg));
        if (i == 0) gettimeofday(&finish_proc, NULL);
        
        // Save final value
        result[graph->devices[msg.payload.dir]->state.y][graph->devices[msg.payload.dir]->state.x] = msg.payload.val;
        
    }
    */
    
    // Receive final value of each device
    while (aggregate != 0) {
        
        // Receive message
        PMessage<None, MatMessage> msg;
        host_link->recvMsg(&msg, sizeof(msg));
        
        if (time_got == false) {
            gettimeofday(&finish_proc, NULL);
            time_got = true;
        }
        
        // Save final value
        aligned_query.push_back(char(msg.payload.val));
        aggregate = msg.payload.dir;
        
        printf("Query = %c\n", msg.payload.val);
        printf("Aggregate = %d\n", msg.payload.dir);
        
    }
    

    timersub(&finish_proc, &start_proc, &diff_proc);
    double proc_duration = (double) diff_proc.tv_sec + (double) diff_proc.tv_usec / 1000000.0;
    
    /*
    for (uint32_t y = 0; y < (SUBLENGTH + 1); y++) {
        for (uint32_t x = 0; x < (QUERYLENGTH + 1); x++) {
            
            printf("%d ", result[y][x]);
            
        }
        
        printf("\n");
        
    }
    */
    
    std::vector<char>::size_type n = aligned_query.size();
    for (int i = n - 2; i >= 0; --i) {
        printf("%c", aligned_query[i]);
    }
    
    
    printf("\n\n%lf,%lf,%lf", map_duration, init_duration, proc_duration);
    
    return 0;

}
