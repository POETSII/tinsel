// SPDX-License-Identifier: BSD-2-Clause
#ifndef _HEAT_H_
#define _HEAT_H_

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
const uint32_t EXTERNALX = 3;
const uint32_t EXTERNALY = 4;
const uint32_t UNKNOWN = 5;

// State array size
const uint32_t BUFSIZE = 3;

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
    // Message Counter
    uint32_t msgcnt;
    
    uint32_t stepcnt;
};

struct MatDevice : PDevice<MatState, None, MatMessage> {

    // Called once by POLite at start of execution
    inline void init() {
        *readyToSend = No;
        
        s->stepcnt = 0;
        
        // Maximum number of meesages a node could send
        s->msgcnt = 3;
        
        // Match number of messages to number of edges
        
        // If last in x dimension, no message in x dimension to be sent
        if (s->x == ((s->xmax)-1)) {
            s->msgcnt = s->msgcnt - 1;
        }
        // If last in y dimension, no message in y dimension to be sent
        if (s->y == ((s->ymax)-1)) {
            s->msgcnt = s->msgcnt - 1;
        }
        // If last in z dimension, no message in z dimension to be sent
        if ((s->z) == (s->zmax)-1) {
            s->msgcnt = s->msgcnt - 1;
        }
        
    }

    // Send handler
    inline void send(volatile MatMessage* msg) {
        
        if (*readyToSend == Pin(INTERNALX)) {
            
            msg->dir = INTERNALX;
            msg->val = s->element1;
            
        }
        if (*readyToSend == Pin(INTERNALY)) {
            
            msg->dir = INTERNALY;
            msg->val = s->element2;
            
        }
        if (*readyToSend == Pin(INTERNALZ)) {
            
            msg->dir = INTERNALZ;
            msg->val = s->aggregate;
            
        }
        
        // Decrement message counter
        s->msgcnt = s->msgcnt - 1;
        
        // Return send flag to no wish to send
        *readyToSend = No;
    }

    // Receive handler
    inline void recv(MatMessage* msg, None* edge) {

        // Is the message travelling in the x dimension?
        if ((msg->dir == INTERNALX) || (msg->dir == EXTERNALX)) {
            
            s->element1 = msg->val;
            s->inflags |= EL1;
            
        }

        // Is the message travelling in the y dimension?
        if ((msg->dir == INTERNALY) || (msg->dir == EXTERNALY)) {
            
            s->element2 = msg->val;
            s->inflags |= EL2;

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
            
        }
        
        // If all three elements received and the aggregate has not already been calculated
        if ((s->inflags & EL1) && (s->inflags & EL2) && (s->inflags & AGG) && !(s->inflags & ANS2)) {
            
            // Calculate aggregate
            s->aggregate = (s->element1 * s->element2) + s->aggregate;
            s->inflags |= ANS2;

        }
    }

    // Called by POLite when system becomes idle
    inline bool step() {
        
        s->stepcnt++;
        
        // Are we still expecting to send messages?
        if (s->msgcnt != 0) {
               
            // Should we transmit element1, have we received it but not re-transmitted it?
            if ((s->x < ((s->xmax)-1)) && (s->inflags & EL1) && !(s->sentflags & EL1) && (*readyToSend == No)) {
                
                s->sentflags |= EL1;
                *readyToSend = Pin(INTERNALX);
                
            }
            
            // Should we transmit element2, have we received it but not re-transmitted it?
            if ((s->y < ((s->ymax)-1)) && (s->inflags & EL2) && !(s->sentflags & EL2) && (*readyToSend == No)) {
                
                s->sentflags |= EL2;
                *readyToSend = Pin(INTERNALY);
                
            }
            
            // Should we transmit the aggregate, have we calculated it but not re-transmitted it?
            if ((s->z < ((s->zmax)-1)) && ((s->inflags & ANS1) || (s->inflags & ANS2)) && !(s->sentflags & AGG) && (*readyToSend == No)) {
                
                s->sentflags |= AGG;
                *readyToSend = Pin(INTERNALZ);
                
            }
            
        }
        
        // Has the node sent all the messages it needs to and has it calculated its aggregate?
        if ((s->msgcnt == 0) && ((s->inflags & ANS1) || (s->inflags & ANS2))) {
            // No messages queued. Nothing more to do.
            return false;
        }
        else if (s->stepcnt > 10000) {
            // Safety feature incase it loops forever
            return false;
        }
        else {
            // Messages still queued. More to do.
            return true; 
        }
        
    }

    // Optionally send message to host on termination
    inline bool finish(volatile MatMessage* msg) {
        
        // If node is last in the z dimension, the aggregate forms part of the answer matrix and should be returned
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
