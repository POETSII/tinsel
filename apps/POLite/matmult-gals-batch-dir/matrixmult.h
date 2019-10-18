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

// MatMessage dir -> Internal/External Plane Flags (Device ID used in finish call)
const uint32_t INTERNALX = 0;
const uint32_t INTERNALY = 1;
const uint32_t INTERNALZ = 2;

struct MatMessage {
    
    // Direction literals listed above
    uint32_t dir;
    // Matrix Elements
    int32_t val;
    
};

struct MatState {
    
    // Device id
    uint32_t id;
    // Matrix elements
    int32_t element1, element2, aggregate;
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
            msg->val = s->element1;
            s->sentflags |= EL1;
            
        }
        if (*readyToSend == Pin(INTERNALY)) {
            
            msg->dir = INTERNALY;
            msg->val = s->element2;
            s->sentflags |= EL2;
            
        }
        if (*readyToSend == Pin(INTERNALZ)) {
            
            msg->dir = INTERNALZ;
            msg->val = s->aggregate;
            s->sentflags |= AGG;
            
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
        else {
            
            // No more stored values to send
            *readyToSend = No;
        }
        
    }

    // Receive handler
    inline void recv(MatMessage* msg, None* edge) {

        // Is the message travelling in the x dimension?
        if (msg->dir == INTERNALX) {
            
            s->element1 = msg->val;
            s->inflags |= EL1;
            
            // Should this value be propagated in the x dimension?
            if (s->x < ((s->xmax)-1)) {
                *readyToSend = Pin(INTERNALX);
            }
            
        }

        // Is the message travelling in the y dimension?
        if (msg->dir == INTERNALY) {
            
            s->element2 = msg->val;
            s->inflags |= EL2;
            
            // Should this value be propagated in the y dimension?
            if (s->y < ((s->ymax)-1)) {
                *readyToSend = Pin(INTERNALY);
            }

        }

        // Is the message travelling in the z dimension?
        if (msg->dir == INTERNALZ) {
            
            s->aggregate = msg->val;
            s->inflags |= AGG;
            
        }

        // If both elements received and device is first in the z-dimension and the aggregregate has not been calculated
        if (((s->z) == 0) && (s->inflags & EL1) && (s->inflags & EL2) && !(s->inflags & ANS1)) {
            
            // Calculate aggregate
            s->aggregate = s->element1 * s->element2;
            s->inflags |= ANS1;
            *readyToSend = Pin(INTERNALZ);
            
        }
        
        // If all three elements received and the aggregate has not already been calculated
        if ((s->inflags & EL1) && (s->inflags & EL2) && (s->inflags & AGG) && !(s->inflags & ANS2)) {
            
            // Calculate aggregate
            s->aggregate = (s->element1 * s->element2) + s->aggregate;
            s->inflags |= ANS2;
            
            // Send aggregate if not last in z dimension
            if (((s->z) != (s->zmax)-1)) {
                
                *readyToSend = Pin(INTERNALZ);
                
            }

        }
    }

    // Called by POLite when system becomes idle
    inline bool step() {
        
        // Initialise pre-populated sides
        if ((s->x == 0) && !(s->inflags & EL1)) {
            s->inflags |= EL1;
            *readyToSend = Pin(INTERNALX);
            return true;
        }
        else if ((s->y == 0) && !(s->inflags & EL2)) {
            s->inflags |= EL2;
            *readyToSend = Pin(INTERNALY);
            return true;
        }
        else if ((s->x == 0) && (s->y == 0) && (s->z == 0) && !(s->inflags & ANS1)) {
            s->aggregate = s->element1 * s->element2;
            s->inflags |= ANS1;
            *readyToSend = Pin(INTERNALZ);
            return true;
        }
        else {
            return false;
        }
        
    }

    // Optionally send message to host on termination
    inline bool finish(volatile MatMessage* msg) {
        
        if ((s->z) == (s->zmax)-1) {
            msg->dir = s->id;
            msg->val = s->aggregate;
            return true;
        }
        else {
            return false;
        }
        
        

    }
};



#endif
