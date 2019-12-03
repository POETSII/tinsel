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
    
    uint32_t lower_count = 0u;
    uint64_t upper_count = 0u;
    double total_time = 0.0f;
    
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
        static PDeviceId mesh[MESHLEN][MESHWID][MESHHEI];

        for (uint32_t x = 0; x < MESHLEN; x++) {
            for (uint32_t y = 0; y < MESHWID; y++) {
                for (uint32_t z = 0; z < MESHHEI; z++) {
                    
                    mesh[x][y][z] = graph->newDevice();
                    
                    printf("Node with ID => %d\n", mesh[x][y][z]);
                    
                }
            }
        }
        
        // Add edges
        for (uint32_t x = 0; x < MESHLEN; x++) {
            for (uint32_t y = 0; y < MESHWID; y++) {
                for (uint32_t z = 0; z < MESHHEI; z++) {
                    
                    if (x < MESHLEN-1) {
                        graph->addEdge(mesh[x][y][z], 0, mesh[x+1][y][z]);
                        }
                    if (y < MESHWID-1) {
                        graph->addEdge(mesh[x][y][z], 1, mesh[x][y+1][z]);
                        }
                    if (z < MESHHEI-1) {
                        graph->addEdge(mesh[x][y][z], 2, mesh[x][y][z+1]);
                        }
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
        for (uint32_t x = 0; x < MESHLEN; x++) {
            for (uint32_t y = 0; y < MESHWID; y++) {
                for (uint32_t z = 0; z < MESHHEI; z++) {
                    
                    printf("Initiliazing node with ID => %d\n", mesh[x][y][z]);
                    
                    // Initialise device IDs
                    graph->devices[mesh[x][y][z]]->state.id = mesh[x][y][z];
                    
                    // Initialise Mesh coordinates on devices
                    graph->devices[mesh[x][y][z]]->state.x = x;
                    graph->devices[mesh[x][y][z]]->state.y = y;
                    graph->devices[mesh[x][y][z]]->state.z = z;

                    //Inform each device of matrix size for message passing decisions
                    graph->devices[mesh[x][y][z]]->state.xmax = MESHLEN;
                    graph->devices[mesh[x][y][z]]->state.ymax = MESHWID;
                    graph->devices[mesh[x][y][z]]->state.zmax = MESHHEI;
                    
                    //Clear message reception flags
                    graph->devices[mesh[x][y][z]]->state.inflags = 0;
                    graph->devices[mesh[x][y][z]]->state.sentflags = 0;
                    
                    //Clear aggregates
                    graph->devices[mesh[x][y][z]]->state.aggregate[0][0] = 0;
                    graph->devices[mesh[x][y][z]]->state.aggregate[0][1] = 0;
                    graph->devices[mesh[x][y][z]]->state.aggregate[1][0] = 0;
                    graph->devices[mesh[x][y][z]]->state.aggregate[1][1] = 0;
                    
                    if (x == 0) {
                        graph->devices[mesh[x][y][z]]->state.elementX[0][0] = matrixA[(y*2)][(z*2)];
                        graph->devices[mesh[x][y][z]]->state.elementX[0][1] = matrixA[(y*2)+1][(z*2)];
                        graph->devices[mesh[x][y][z]]->state.elementX[1][0] = matrixA[(y*2)][(z*2)+1];
                        graph->devices[mesh[x][y][z]]->state.elementX[1][1] = matrixA[(y*2)+1][(z*2)+1];
                    }
                    if (y == 0) {
                        graph->devices[mesh[x][y][z]]->state.elementY[0][0] = matrixB[(z*2)][(x*2)];
                        graph->devices[mesh[x][y][z]]->state.elementY[0][1] = matrixB[(z*2)][(x*2)+1];
                        graph->devices[mesh[x][y][z]]->state.elementY[1][0] = matrixB[(z*2)+1][(x*2)];
                        graph->devices[mesh[x][y][z]]->state.elementY[1][1] = matrixB[(z*2)+1][(x*2)+1];
                    }
                }
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
        
        // Allocate array to contain final value of each device
        uint32_t result[(MESHLEN*2)][(MESHWID*2)] {};
        
        printf("Result matrix created with dims => %dx%d\n", (MESHLEN*2), (MESHWID*2));
        
        // Start timer for overall processing
        struct timeval start_proc, finish_proc, diff_proc;
        gettimeofday(&start_proc, NULL); 

        printf("Expecting returns from count => %d\n", RETMATSIZE);       

        // Receive final value of each device
        for (uint32_t i = 0; i < (RETMATSIZE + 1); i++) {
            
            printf("Expecting message => %d\n", i);
            
            // Receive message
            PMessage<None, MatMessage> msg;
            host_link->recvMsg(&msg, sizeof(msg));
            //if (i == 0) gettimeofday(&finish_proc, NULL);
            
            printf("Message received => %d\n", i);
            
            // Save final value
            if (msg.payload.dir != CCNTDIR) {
                //result[(graph->devices[msg.payload.dir]->state.x)*2][(graph->devices[msg.payload.dir]->state.y)*2] = msg.payload.val1;
                //result[((graph->devices[msg.payload.dir]->state.x)*2)+1][(graph->devices[msg.payload.dir]->state.y)*2] = msg.payload.val2;
                //result[(graph->devices[msg.payload.dir]->state.x)*2][((graph->devices[msg.payload.dir]->state.y)*2)+1] = msg.payload.val3;
                //result[((graph->devices[msg.payload.dir]->state.x)*2)+1][((graph->devices[msg.payload.dir]->state.y)*2)+1] = msg.payload.val4;
                
                result[0][0] = msg.payload.val[0];
                result[0][1] = msg.payload.val[1];
                result[1][0] = msg.payload.val[2];
                result[1][1] = msg.payload.val[3];
            }
            else {
                lower_count = msg.payload.val[0];
                upper_count = msg.payload.val[1];
            }
            
        }
        
        total_time = ((upper_count << 32) + lower_count) / 240000000.0;

        //timersub(&finish_proc, &start_proc, &diff_proc);
        //double proc_duration = (double) diff_proc.tv_sec + (double) diff_proc.tv_usec / 1000000.0;
        
        for (uint32_t y = 0; y < (MESHWID*2); y++) {
            for (uint32_t x = 0; x < (MESHLEN*2); x++) {
                
                printf("%d ", result[x][y]);
                
            }
            
            printf("\n");
            
        }
        
        printf("Upper count => %lld\n", upper_count);
        printf("Lower count => %lld\n", lower_count);
        
        printf("%lf,%lf,%lf", map_duration, init_duration, total_time);
    
    }
    
    return 0;

}
