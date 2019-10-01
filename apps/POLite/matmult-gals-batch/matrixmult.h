// SPDX-License-Identifier: BSD-2-Clause
#ifndef _HEAT_H_
#define _HEAT_H_

#include <POLite.h>

// Tracking Flags flags
const uint32_t INIT = 1 << 0;

struct MatMessage {
    // Message Origin -> Implementation uses 0 for A matrix, 1 for B matrix, 
    // 2 for agregated matrix, DeviceID for HostLink return
    uint32_t from;
    // Matrix Elements
    int32_t aggregate;
    // Source Coordinates
    uint32_t source_x, source_y, source_z;
};

struct MatState {
    // Device id
    uint32_t id;
    // Mesh Coordinates
    uint32_t x,y,z;
    // Mesh Dimnesions
    uint32_t xmax, ymax, zmax;
    // Message origin
    uint32_t from;
    // Possible Inputs
    int32_t element1, element2, aggregate;
    // Tracking flags
    uint32_t trackflag;
};

struct MatDevice : PDevice<MatState, None, MatMessage> {

    // Called once by POLite at start of execution
    inline void init() {
        *readyToSend = No;
    }

    // Send handler
    inline void send(volatile MatMessage* msg) {
        msg->from = s->from;
        msg->source_x = s->x;
        msg->source_y = s->y;
        msg->source_z = s->z;
        msg->aggregate = s->aggregate;
        *readyToSend = No;
    }

    // Receive handler
    inline void recv(MatMessage* msg, None* edge) {

        s->aggregate = msg->aggregate;
        s->aggregate = (s->element1 * s->element2) + s->aggregate;
        
        // Send aggregate if not last in z dimension
        if (((s->z) != (s->zmax)-1)) {
            *readyToSend = Pin(0);
        }
        
    }

    // Called by POLite when system becomes idle
    inline bool step() {
        
        if (((s->z) == 0) && !(s->trackflag & INIT)) {
            s->aggregate = s->element1 * s->element2;
            s->trackflag |= INIT;
            *readyToSend = Pin(0);
            return true;
        }
        else {
            return false;
        }    
    }

    // Optionally send message to host on termination
    inline bool finish(volatile MatMessage* msg) {
        
        if ((s->z) == (s->zmax)-1) {
            msg->from = s->id;
            msg->aggregate = s->aggregate;
            return true;
        }
        else {
            return false;
        }
        
        

    }
};



#endif
