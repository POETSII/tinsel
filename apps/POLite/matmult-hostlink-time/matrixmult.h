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
        
        // Initialise pre-populated sides
        if (s->x == 0) {
            s->inflags |= EL1;
        }
        if (s->y == 0) {
            s->inflags |= EL2;
        }
        if ((s->x == 0) && (s->y == 0) && (s->z == 0)) {
            s->aggregate = s->element1 * s->element2;
            s->inflags |= ANS1;
        }
        if (s->inflags == 0) {
            *readyToSend = HostPin;
        }
        
        *readyToSend = HostPin;
        
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
        if (*readyToSend == HostPin) {
            
            msg->dir = s->id;
            msg->val = 1;
            s->sentflags |= AGG;
            
        }
        
        *readyToSend = No;
        
    }

    // Receive handler
    inline void recv(MatMessage* msg, None* edge) {


    }

    // Called by POLite when system becomes idle
    inline bool step() {
        
        return false;
        
    }

    // Optionally send message to host on termination
    inline bool finish(volatile MatMessage* msg) {
        
        return false;
        
    }   
};



#endif
