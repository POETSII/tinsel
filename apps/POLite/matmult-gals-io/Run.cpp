// SPDX-License-Identifier: BSD-2-Clause
#include "matrixmult.h"
#include "matrices.h"

#include <HostLink.h>
#include <POLite.h>
#include <sys/time.h>

/*****************************************************
 * Matrix Multiplier - Asynchronous - IO Stream
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
        HostLink *host_link = new HostLink(2, 2);
        //Sampleclass *qs = new Sampleclass()

        // Create POETS graph
        //PGraph<MatDevice, MatState, None, MatMessage>(2, 2) graph;
        
        PGraph <MatDevice, MatState, None, MatMessage>*graph = new PGraph<MatDevice, MatState, None, MatMessage>(2, 2);

        // Create 3D mesh of devices
        static PDeviceId mesh[MESHLEN][MESHWID][MESHHEI];

        for (uint32_t x = 0; x < MESHLEN; x++) {
            for (uint32_t y = 0; y < MESHWID; y++) {
                for (uint32_t z = 0; z < MESHHEI; z++) {
                    
                    mesh[x][y][z] = graph->newDevice();
                    
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
                        graph->addEdge(mesh[x][y][z], 0, mesh[x][y+1][z]);
                        }
                    if (z < MESHHEI-1) {
                        graph->addEdge(mesh[x][y][z], 0, mesh[x][y][z+1]);
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

        // Start timer for sending
        struct timeval start_send, finish_send, diff_send;
        gettimeofday(&start_send, NULL);
        
        // Start timer for overall processing
        struct timeval start_proc, finish_proc, diff_proc;
        gettimeofday(&start_proc, NULL);


        int deviceAddr = 0;
        
        for (uint32_t h = 0; h < MESHHEI; h++) {
            for (uint32_t w = 0; w < MESHWID; w++) {
                for (uint32_t l = 0; l < MESHLEN; l++) {
                    
                    // Construct messages -> One same element from each matrix
                    PMessage<None, MatMessage> sendMsg;

                    if (l == 0) {
                        // From maxtrix A
                        deviceAddr = graph->toDeviceAddr[mesh[0][w][h]];
                        sendMsg.devId = getLocalDeviceId(deviceAddr);
                        sendMsg.payload.from = EXTERNALX;
                        sendMsg.payload.element1 = matrixA[w][h];
                        host_link->send(getThreadId(deviceAddr), 2, &sendMsg);
                        //printf("Sent %d to node [0][%d][%d]\n", sendMsg.payload.element1, w, h);
                    }

                    if (w == 0) {
                        // From maxtrix B
                        deviceAddr = graph->toDeviceAddr[mesh[l][0][h]];
                        sendMsg.devId = getLocalDeviceId(deviceAddr);
                        sendMsg.payload.from = EXTERNALY;
                        sendMsg.payload.element2 = matrixB[h][l];
                        host_link->send(getThreadId(deviceAddr), 2, &sendMsg);
                        //printf("Sent %d to node [%d][0][%d]\n", sendMsg.payload.element2, l, h);
                    }

                }
            }
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
            result[graph->devices[msg.payload.from]->state.x][graph->devices[msg.payload.from]->state.y] = msg.payload.aggregate;
            
        }

        timersub(&finish_proc, &start_proc, &diff_proc);
        double proc_duration = (double) diff_proc.tv_sec + (double) diff_proc.tv_usec / 1000000.0;
        
        for (uint32_t y = 0; y < MESHWID; y++) {
            for (uint32_t x = 0; x < MESHLEN; x++) {
                
                printf("%d ", result[x][y]);
                
            }
            
            printf("\n");
            
        }
        
        printf("%lf,%lf,%lf,%lf", map_duration, init_duration, send_duration, proc_duration);
    
    }
    
    return 0;

}
