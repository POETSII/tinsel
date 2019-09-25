// SPDX-License-Identifier: BSD-2-Clause
#include "matrixmult.h"
#include "matrices.h"

#include <HostLink.h>
#include <POLite.h>
#include <sys/time.h>

int main() {

    // Connection to tinsel machine
    HostLink hostLink;

    // Create POETS graph
    PGraph<MatDevice, MatState, None, MatMessage> graph;

    // Create 3D mesh of devices
    PDeviceId mesh[LENGTH][WIDTH][HEIGHT];
    for (uint32_t x = 0; x < LENGTH; x++) {
        for (uint32_t y = 0; y < WIDTH; y++) {
            for (uint32_t z = 0; z < HEIGHT; z++) {
                mesh[x][y][z] = graph.newDevice();
            }
        }
    }
    
    // Add edges
    for (uint32_t x = 0; x < LENGTH; x++) {
        for (uint32_t y = 0; y < WIDTH; y++) {
            for (uint32_t z = 0; z < HEIGHT; z++) {
                if (x < LENGTH-1) {
                    graph.addEdge(mesh[x][y][z], 0, mesh[x+1][y][z]);
                    }
                if (y < WIDTH-1) {
                    graph.addEdge(mesh[x][y][z], 0, mesh[x][y+1][z]);
                    }
                if (z < HEIGHT-1) {
                    graph.addEdge(mesh[x][y][z], 0, mesh[x][y][z+1]);
                    }
            }
        }
    } 

    // Prepare mapping from graph to hardware
    graph.map();
                
    // Initialise device coordinates/dimensions
    for (uint32_t x = 0; x < LENGTH; x++) {
        for (uint32_t y = 0; y < WIDTH; y++) {
            for (uint32_t z = 0; z < HEIGHT; z++) {
                
                //printf("X = %d: Y = %d: Z = %d ", x, y, z);
                //printf("ID = %d ", mesh[x][y][z]);
                //printf("\n");
                
                // Initialise device IDs
                graph.devices[mesh[x][y][z]]->state.id = mesh[x][y][z];
                
                // Initialise Mesh coordinates on devices
                graph.devices[mesh[x][y][z]]->state.x = x;
                graph.devices[mesh[x][y][z]]->state.y = y;
                graph.devices[mesh[x][y][z]]->state.z = z;

                //Inform each device of matrix size for message passing decisions
                graph.devices[mesh[x][y][z]]->state.xmax = LENGTH;
                graph.devices[mesh[x][y][z]]->state.ymax = WIDTH;
                graph.devices[mesh[x][y][z]]->state.zmax = HEIGHT;
            }
        }
    }

    // Write graph down to tinsel machine via HostLink
    graph.write(&hostLink);

    // Load code and trigger execution
    hostLink.boot("code.v", "data.v");
    hostLink.go();
    printf("Starting\n");

    // Start timer
    struct timeval start, finish, diff;
    gettimeofday(&start, NULL);


    int deviceAddr = 0;

    for (uint32_t w = 0; w < WIDTH; w++) {
        for (uint32_t l = 0; l < LENGTH; l++) {
            // Construct messages -> One same element from each matrix
            PMessage<None, MatMessage> sendMsg;

            // From maxtrix A
            deviceAddr = graph.toDeviceAddr[mesh[0][w][l]];
            //printf("deviceAddr = %d\n", deviceAddr);
            sendMsg.devId = getLocalDeviceId(deviceAddr);
            sendMsg.payload.from = EXTERNALX;
            sendMsg.payload.element1 = matrixA[l][w];
            //printf("%d \n ", matrixA[l][w]);
            hostLink.send(getThreadId(deviceAddr), 2, &sendMsg);
            //printf("Sent %d from matrix A to device\n", sendMsg.payload.element1);

            // From maxtrix B
            deviceAddr = graph.toDeviceAddr[mesh[l][0][w]];
            //printf("deviceAddr = %d\n", deviceAddr);
            sendMsg.devId = getLocalDeviceId(deviceAddr);
            sendMsg.payload.from = EXTERNALY;
            sendMsg.payload.element2 = matrixB[w][l];
            //printf("%d \n ", matrixB[w][l]);
            hostLink.send(getThreadId(deviceAddr), 2, &sendMsg);
            //printf("Sent %d from matrix B to device\n", sendMsg.payload.element2);

        }
    }
/*
    // Allocate array to contain final value of each device
    uint32_t aggregate[WIDTH][LENGTH][HEIGHT] {};
    uint32_t source_x[WIDTH][LENGTH][HEIGHT] {};
    uint32_t source_y[WIDTH][LENGTH][HEIGHT] {};
    uint32_t source_z[WIDTH][LENGTH][HEIGHT] {};
    uint32_t element1[WIDTH][LENGTH][HEIGHT] {};
    uint32_t element2[WIDTH][LENGTH][HEIGHT] {};
    //uint32_t id[WIDTH][LENGTH][HEIGHT] {};

    // Receive final value of each device
    for (uint32_t i = 0; i < RETMATSIZE; i++) {
        // Receive message
        PMessage<None, MatMessage> msg;
        hostLink.recvMsg(&msg, sizeof(msg));
        if (i == 0) gettimeofday(&finish, NULL);
        // Save final value
        aggregate[graph.devices[msg.payload.from]->state.x][graph.devices[msg.payload.from]->state.y][graph.devices[msg.payload.from]->state.z] = msg.payload.aggregate;
        source_x[graph.devices[msg.payload.from]->state.x][graph.devices[msg.payload.from]->state.y][graph.devices[msg.payload.from]->state.z] = msg.payload.source_x;
        source_y[graph.devices[msg.payload.from]->state.x][graph.devices[msg.payload.from]->state.y][graph.devices[msg.payload.from]->state.z] = msg.payload.source_y;
        source_z[graph.devices[msg.payload.from]->state.x][graph.devices[msg.payload.from]->state.y][graph.devices[msg.payload.from]->state.z] = msg.payload.source_z;
        element1[graph.devices[msg.payload.from]->state.x][graph.devices[msg.payload.from]->state.y][graph.devices[msg.payload.from]->state.z] = msg.payload.element1;
        element2[graph.devices[msg.payload.from]->state.x][graph.devices[msg.payload.from]->state.y][graph.devices[msg.payload.from]->state.z] = msg.payload.element2;
        //id[graph.devices[msg.payload.from]->state.x][graph.devices[msg.payload.from]->state.y][graph.devices[msg.payload.from]->state.z] = msg.payload.from;
    }

    // Display time
    timersub(&finish, &start, &diff);
    double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
    printf("Time = %lf\n", duration);

    // Initialise device coordinates/dimensions
    for (uint32_t x = 0; x < LENGTH; x++) {
        for (uint32_t y = 0; y < WIDTH; y++) {
            for (uint32_t z = 0; z < HEIGHT; z++) {
                printf("X = %d: Y = %d: Z = %d ", x, y, z);
                //printf("id = %d,  ", id[x][y][z]);
                printf("agg = %d,  ", aggregate[x][y][z]);
                printf("s_x = %d,  ", source_x[x][y][z]);
                printf("s_y = %d,  ", source_y[x][y][z]);
                printf("s_z = %d,  ", source_z[x][y][z]);
                printf("el1 = %d,  ", element1[x][y][z]);
                printf("el2 = %d  ", element2[x][y][z]);
                printf("\n");
            }
        }
    }
*/
    // Allocate array to contain final value of each device
    uint32_t result[LENGTH][WIDTH] {};

    // Receive final value of each device
    for (uint32_t i = 0; i < RETMATSIZE; i++) {
        // Receive message
        PMessage<None, MatMessage> msg;
        hostLink.recvMsg(&msg, sizeof(msg));
        if (i == 0) gettimeofday(&finish, NULL);
        // Save final value
        result[graph.devices[msg.payload.from]->state.x][graph.devices[msg.payload.from]->state.y] = msg.payload.aggregate;
    }

    // Display time
    timersub(&finish, &start, &diff);
    double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
    printf("Time = %lf\n", duration);

    for (uint32_t y = 0; y < WIDTH; y++) {
        for (uint32_t x = 0; x < LENGTH; x++) {
            //printf("X = %d: Y = %d ", x, y);
            printf("%d ", result[x][(WIDTH-1) - y]);
        }
        printf("\n");
    }
    
    return 0;

}
