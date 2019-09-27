// SPDX-License-Identifier: BSD-2-Clause
#ifndef _HEAT_H_
#define _HEAT_H_

#include <POLite.h>

// Input reception flags
const uint32_t EL1 = 1 << 0;
const uint32_t EL2 = 1 << 1;
const uint32_t AGG = 1 << 2;
const uint32_t ANS1 = 1 << 3;
const uint32_t ANS2 = 1 << 4;

// Internal/External Plane Flags (Device ID used in finish call)
const uint32_t INTERNAL = 0;
const uint32_t EXTERNALX = 1;
const uint32_t EXTERNALY = 2;

struct MatMessage {
    // Message Origin -> Implementation uses 0 for A matrix, 1 for B matrix, 
    // 2 for agregated matrix, DeviceID for HostLink return
    uint32_t from;
    // Matrix Element2
    uint32_t element1, element2, aggregate;
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
    uint32_t element1, element2, aggregate;
    // Reception Flags
    uint32_t inflags;
};

struct MatDevice : PDevice<MatState, None, MatMessage> {

    // Called once by POLite at start of execution
    inline void init() {
        s->element1 = 0;
        s->element2 = 0;
        s->aggregate = 0;
        s->from = INTERNAL;
        s->inflags = 0;
        *readyToSend = No;
    }

    // Send handler
    inline void send(volatile MatMessage* msg) {
        msg->from = s->from;
        msg->source_x = s->x;
        msg->source_y = s->y;
        msg->source_z = s->z;
        msg->element1 = s->element1;
        msg->element2 = s->element2;
        msg->aggregate = s->aggregate;
        *readyToSend = No;
    }

    // Receive handler
    inline void recv(MatMessage* msg, None* edge) {

        if ( (!(s->inflags & EL1) && (msg->source_x == (s->x)-1) && (msg->source_y == s->y) && (msg->source_z == s->z) && (msg->from == INTERNAL)) || (msg->from == EXTERNALX) ) {
            s->element1 = msg->element1;
            s->inflags |= EL1;

            // Should this value be propagated in the x dimension?
            if (s->x < ((s->xmax)-1)) {
                *readyToSend = Pin(0);
            }
            
        }

        if ( (!(s->inflags & EL2) && (msg->source_x == s->x) && (msg->source_y == (s->y)-1) && (msg->source_z == s->z) && (msg->from == INTERNAL)) || (msg->from == EXTERNALY) ) {
            s->element2 = msg->element2;
            s->inflags |= EL2;

            // Should this value be propagated in the y dimension?
            if (s->y < ((s->ymax)-1)) {
                *readyToSend = Pin(0);
            }
        }

        if (!(s->inflags & AGG) && (msg->source_x == s->x) && (msg->source_y == s->y) && (msg->source_z == (s->z)-1) && (msg->from == INTERNAL)) {
            s->aggregate = msg->aggregate;
            s->inflags |= AGG;
        }

        // If both elements received and device is first in the z-dimension and the aggregregate has no been calculated
        if (((s->z) == 0) && (s->inflags & EL1) && (s->inflags & EL2) && !(s->inflags & ANS1)) {
            s->aggregate = s->element1 * s->element2;
            s->inflags |= ANS1;
            *readyToSend = Pin(0);
        }
        
        // If all three elements received
        if ((s->inflags & EL1) && (s->inflags & EL2) && (s->inflags & AGG) && !(s->inflags & ANS2)) {
            s->aggregate = (s->element1 * s->element2) + s->aggregate;
            s->inflags |= ANS2;
            
            // Send aggregate if not last in z dimension
            if (((s->z) != (s->zmax)-1)) {
            *readyToSend = Pin(0);
            }

        }
    }

    // Called by POLite when system becomes idle
    inline bool step() {
        *readyToSend = No;
            
        return false;
    }

    // Optionally send message to host on termination
    inline bool finish(volatile MatMessage* msg) {
        
        if ((s->z) == (s->zmax)-1) {
            msg->from = s->id;
            msg->aggregate = s->aggregate;
            //msg->source_x = s->x;
            //msg->source_y = s->y;
            //msg->source_z = s->z;
            //msg->element1 = s->element1;
            //msg->element2 = s->element2;
            return true;
        }
        else {
            return false;
        }
        
        

    }
};



#endif
