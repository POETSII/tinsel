// SPDX-License-Identifier: BSD-2-Clause
#ifndef _HEAT_H_
#define _HEAT_H_

#define POLITE_MAX_FANOUT 32
#include <POLite.h>

// Input reception and message validity flags
const uint32_t EL1 = 1 << 0;
const uint32_t EL2 = 1 << 1;
const uint32_t AGG = 1 << 2;
const uint32_t ANS1 = 1 << 3;
const uint32_t ANS2 = 1 << 4;
const uint32_t CCNT = 1 << 5;

// MatMessage dir -> Internal/External Plane Flags (Device ID used in finish call)
const uint32_t INTERNALX = 0;
const uint32_t INTERNALY = 1;
const uint32_t INTERNALZ = 2;
const uint32_t CCNTDIR = 0xFFFFFFFF;

struct MatMessage {
    
    // Direction literals listed above
    uint32_t dir;
    // Matrix Elements
    int32_t val[4];
    
};

struct MatState {
    
    // Device id
    uint32_t id;
    // Matrix elements
    int32_t elementX[2][2];
    // Matrix elements
    int32_t elementY[2][2];
    // Matrix elements
    int32_t aggregate[2][2];
    // Reception flags
    uint32_t inflags;
        // Reception flags
    uint32_t sentflags;
    // Mesh Coordinates
    uint32_t x,y,z;
    // Mesh Dimnesions
    uint32_t xmax, ymax, zmax;
    
};

struct MatDevice : PDevice<MatState, None, MatMessage> {

    // Called once by POLite at start of execution
    inline void init() {
        
        *readyToSend = No;
        
    }

    // Send handler
    inline void send(volatile MatMessage* msg) {
        
        if (*readyToSend == Pin(INTERNALX)) {
            
            msg->dir = INTERNALX;
            msg->val[0] = s->elementX[0][0];
            msg->val[1] = s->elementX[0][1];
            msg->val[2] = s->elementX[1][0];
            msg->val[3] = s->elementX[1][1];
            s->sentflags |= EL1;
            
        }
        if (*readyToSend == Pin(INTERNALY)) {
            
            msg->dir = INTERNALY;
            msg->val[0] = s->elementY[0][0];
            msg->val[1] = s->elementY[0][1];
            msg->val[2] = s->elementY[1][0];
            msg->val[3] = s->elementY[1][1];
            s->sentflags |= EL2;
            
        }
        if (*readyToSend == Pin(INTERNALZ)) {
            
            msg->dir = INTERNALZ;
            msg->val[0] = s->aggregate[0][0];
            msg->val[1] = s->aggregate[0][1];
            msg->val[2] = s->aggregate[1][0];
            msg->val[3] = s->aggregate[1][1];
            s->sentflags |= AGG;
            
        }
        if (*readyToSend == HostPin) {
            
            if (!(s->sentflags & AGG)) {
                msg->dir = s->id;
                msg->val[0] = s->aggregate[0][0];
                msg->val[1] = s->aggregate[0][1];
                msg->val[2] = s->aggregate[1][0];
                msg->val[3] = s->aggregate[1][1];
                s->sentflags |= AGG;
            }
            else if (!(s->sentflags & CCNT)) {
                msg->dir = CCNTDIR;
                msg->val[0] = s->elementX[0][0];
                msg->val[1] = s->elementX[0][1];
                s->sentflags |= CCNT;
            }
        }
        
        if ((s->x < ((s->xmax)-1)) && (s->inflags & EL1) && !(s->sentflags & EL1)) {
            // Should we transmit element1, have we received it but not re-transmitted it?
            
            *readyToSend = Pin(INTERNALX);
            
        }
        else if ((s->y < ((s->ymax)-1)) && (s->inflags & EL2) && !(s->sentflags & EL2)) {
            // Should we transmit element2, have we received it but not re-transmitted it?
            
            *readyToSend = Pin(INTERNALY);
            
        }
        else if ((s->z < ((s->zmax)-1)) && ((s->inflags & ANS1) || (s->inflags & ANS2)) && !(s->sentflags & AGG)) {
            // Should we transmit the aggregate, have we calculated it but not re-transmitted it?
            
            *readyToSend = Pin(INTERNALZ);
            
        }
        else if ((s->z == ((s->zmax)-1)) && ((s->inflags & ANS1) || (s->inflags & ANS2)) && !(s->sentflags & AGG)) {
            // Should we transmit the aggregate, have we calculated it but not re-transmitted it?
            
            *readyToSend = HostPin;
            
        }
        else if ((((s->x) == (s->xmax)-1) && ((s->y) == (s->ymax)-1) && ((s->z) == (s->zmax)-1)) && !(s->sentflags & CCNT)) {
            // Are we the last node? Should we transmit the counts?
            
            *readyToSend = HostPin;
            
        }
        else {
            
            // No more stored values to send
            *readyToSend = No;
        }
        
    }

    // Receive handler
    inline void recv(MatMessage* msg, None* edge) {

        // Is the message travelling in the x dimension?
        if (msg->dir == INTERNALX) {
            
            s->elementX[0][0] = msg->val[0];
            s->elementX[0][1] = msg->val[1];
            s->elementX[1][0] = msg->val[2];
            s->elementX[1][1] = msg->val[3];
            s->inflags |= EL1;
            
            // Should this value be propagated in the x dimension?
            if (s->x < ((s->xmax)-1)) {
                *readyToSend = Pin(INTERNALX);
            }
            
        }

        // Is the message travelling in the y dimension?
        if (msg->dir == INTERNALY) {
            
            s->elementY[0][0] = msg->val[0];
            s->elementY[0][1] = msg->val[1];
            s->elementY[1][0] = msg->val[2];
            s->elementY[1][1] = msg->val[3];
            s->inflags |= EL2;
            
            // Should this value be propagated in the y dimension?
            if (s->y < ((s->ymax)-1)) {
                *readyToSend = Pin(INTERNALY);
            }

        }

        // Is the message travelling in the z dimension?
        if (msg->dir == INTERNALZ) {
            
            s->aggregate[0][0] = msg->val[0];
            s->aggregate[0][1] = msg->val[1];
            s->aggregate[1][0] = msg->val[2];
            s->aggregate[1][1] = msg->val[3];
            s->inflags |= AGG;
            
        }

        // If both elements received and device is first in the z-dimension and the aggregregate has not been calculated
        if (((s->z) == 0) && (s->inflags & EL1) && (s->inflags & EL2) && !(s->inflags & ANS1)) {
            
            // Calculate aggregates
            s->aggregate[0][0] = s->elementX[0][0] * s->elementY[0][0];
            s->aggregate[0][1] = s->elementX[0][1] * s->elementY[0][0];
            s->aggregate[1][0] = s->elementX[0][0] * s->elementY[0][1];
            s->aggregate[1][1] = s->elementX[0][1] * s->elementY[0][1];
            
            s->aggregate[0][0] += s->elementX[1][0] * s->elementY[1][0];
            s->aggregate[0][1] += s->elementX[1][1] * s->elementY[1][0];
            s->aggregate[1][0] += s->elementX[1][0] * s->elementY[1][1];
            s->aggregate[1][1] += s->elementX[1][1] * s->elementY[1][1];
            s->inflags |= ANS1;
            *readyToSend = Pin(INTERNALZ);
            
        }
        
        // If all three elements received and the aggregate has not already been calculated
        if ((s->inflags & EL1) && (s->inflags & EL2) && (s->inflags & AGG) && !(s->inflags & ANS2)) {
            
            // Calculate aggregate
            s->aggregate[0][0] += s->elementX[0][0] * s->elementY[0][0];
            s->aggregate[0][1] += s->elementX[0][1] * s->elementY[0][0];
            s->aggregate[1][0] += s->elementX[0][0] * s->elementY[0][1];
            s->aggregate[1][1] += s->elementX[0][1] * s->elementY[0][1];
            
            s->aggregate[0][0] += s->elementX[1][0] * s->elementY[1][0];
            s->aggregate[0][1] += s->elementX[1][1] * s->elementY[1][0];
            s->aggregate[1][0] += s->elementX[1][0] * s->elementY[1][1];
            s->aggregate[1][1] += s->elementX[1][1] * s->elementY[1][1];
            s->inflags |= ANS2;
            
            // Propagate aggregate if not last in z dimension
            if (((s->z) != (s->zmax)-1)) {
                
                *readyToSend = Pin(INTERNALZ);
                
            }
            else {
                
                if (((s->x) == (s->xmax)-1) && ((s->y) == (s->ymax)-1)) {
                #ifdef TINSEL
                    tinselPerfCountStop();
                    s->elementX[0][0] = tinselCycleCount();
                    s->elementX[0][1] = tinselCycleCountU();
                    s->inflags |= CCNT;
                #endif
                }
                
                *readyToSend = HostPin;
                
            }

        }
    }

    // Called by POLite when system becomes idle
    inline bool step() {
        
        // If we are the last node, reset and start the cycle counters
        if (((s->x) == (s->xmax)-1) && ((s->y) == (s->ymax)-1) && ((s->z) == (s->zmax)-1)) {
            #ifdef TINSEL
                tinselPerfCountReset();
                tinselPerfCountStart();
            #endif
        }
        
        // Initialise pre-populated sides
        
        if (s->x == 0) {
            s->inflags |= EL1;
            *readyToSend = Pin(INTERNALX);
        }
        if (s->y == 0) {
            s->inflags |= EL2;
            *readyToSend = Pin(INTERNALY);
        }
        if ((s->x == 0) && (s->y == 0) && (s->z == 0)) {
            s->aggregate[0][0] = s->elementX[0][0] * s->elementY[0][0];
            s->aggregate[0][1] = s->elementX[0][1] * s->elementY[0][0];
            s->aggregate[1][0] = s->elementX[0][0] * s->elementY[0][1];
            s->aggregate[1][1] = s->elementX[0][1] * s->elementY[0][1];
            
            s->aggregate[0][0] += s->elementX[1][0] * s->elementY[1][0];
            s->aggregate[0][1] += s->elementX[1][1] * s->elementY[1][0];
            s->aggregate[1][0] += s->elementX[1][0] * s->elementY[1][1];
            s->aggregate[1][1] += s->elementX[1][1] * s->elementY[1][1];
            
            s->inflags |= ANS1;
            
            // Is there only a single node?
            if (((s->x) == (s->xmax)-1) && ((s->y) == (s->ymax)-1) && ((s->z) == (s->zmax)-1)) {
                #ifdef TINSEL
                    tinselPerfCountStop();
                    s->elementX[0][0] = tinselCycleCount();
                    s->elementX[0][1] = tinselCycleCountU();
                    s->inflags |= CCNT;
                #endif
                *readyToSend = HostPin;
            }
            else {
                *readyToSend = Pin(INTERNALZ);
                
            }
        }
        if (s->inflags == 0) {
            *readyToSend = No;
        }
        
        return false;
        
    }

    // Optionally send message to host on termination
    inline bool finish(volatile MatMessage* msg) {
        
        return false;
        
    }   
};



#endif
