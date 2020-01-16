// SPDX-License-Identifier: BSD-2-Clause
#ifndef _HEAT_H_
#define _HEAT_H_

#define POLITE_MAX_FANOUT 32
#include <POLite.h>

// Input reception and message validity flags
const uint32_t EL1 = 1 << 0;
const uint32_t EL2 = 1 << 1;
const uint32_t EL3 = 1 << 2;
const uint32_t AGG = 1 << 3;
const uint32_t ANS1 = 1 << 4;
const uint32_t ANS2 = 1 << 5;

// MatMessage dir -> Internal/External Plane Flags (Device ID used in finish call)
const uint32_t INTERNALX = 0;
const uint32_t INTERNALY = 1;
const uint32_t INTERNALZ = 2;

// Smith-Waterman Constants
const int32_t MATCH = 3;
const int32_t MISMATCH = -3;
const int32_t GAP = -2;

struct MatMessage {
    
    // Direction literals listed above
    uint32_t dir;
    // Matrix Elements
    int32_t val;
    
};

struct MatState {
    
    // Device id
    uint32_t id;
    // Nucleotides
    char query, subject;
    // Mesh Coordinates
    uint32_t x,y;
    // Mesh Dimnesions
    uint32_t xmax, ymax;
    
    // Aggregate
    uint32_t element1, element2, element3, aggregate;
    // Reception flags
    uint32_t inflags;
        // Reception flags
    uint32_t sentflags;
    
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
            msg->val = s->aggregate;
            s->sentflags |= EL1;
            
        }
        if (*readyToSend == Pin(INTERNALY)) {
            
            msg->dir = INTERNALY;
            msg->val = s->aggregate;
            s->sentflags |= EL2;
            
        }
        if (*readyToSend == Pin(INTERNALZ)) {
            
            msg->dir = INTERNALZ;
            msg->val = s->aggregate;
            s->sentflags |= EL3;
            
        }
        
        if ((s->x < ((s->xmax)-1)) && (s->inflags & EL1) && !(s->sentflags & EL1)) {
            
            *readyToSend = Pin(INTERNALX);
            
        }
        else if ((s->y < ((s->ymax)-1)) && (s->inflags & EL2) && !(s->sentflags & EL2)) {
            
            *readyToSend = Pin(INTERNALY);
            
        }
        else if ((s->x < ((s->xmax)-1)) && (s->y < ((s->ymax)-1)) && (s->inflags & EL3) && !(s->sentflags & EL3)) {
            
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
            
            if (msg->val >= abs(GAP)) {
                s->element1 = msg->val + GAP;
            }
            else {
                s->element1 = 0;
            }
            
            // Indicate x dimension messgae has been received
            s->inflags |= EL1;
            
        }

        // Is the message travelling in the y dimension?
        if (msg->dir == INTERNALY) {
            
            if (msg->val >= abs(GAP)) {
                s->element2 = msg->val + GAP;
            }
            else {
                s->element2 = 0;
            }

            // Indicate y dimension messgae has been received
            s->inflags |= EL2;

        }

        // Is the message travelling in the z dimension?
        if (msg->dir == INTERNALZ) {
            
            // Do the nucleotides match?
            if (s->query == s->subject) {
                s->element3 = msg->val + MATCH;
            }
            else {
                
                if (msg->val >= abs(MISMATCH)) {
                    s->element3 = msg->val + MISMATCH;
                }
                else {
                    s->element3 = 0;
                }
                
            }
            
            // Indicate z dimension messgae has been received
            s->inflags |= EL3;
            
        }

        // If all three elements received
        if ((s->inflags & EL1) && (s->inflags & EL2) && (s->inflags & EL3)) {
            
            // Find largest element
            if (s->element1 >= s->element2 && s->element1 >= s->element3) {
                s->aggregate = s->element1;
            }
            else if (s->element2 >= s->element1 && s->element2 >= s->element3) {
                s->aggregate = s->element2;
            }
            else {
                s->aggregate = s->element3;
            }
            
            // Should this value be propagated in the X dimension?
            if (s->x < ((s->xmax)-1)) {
                *readyToSend = Pin(INTERNALX);
            }
            // Should this value be propagated in the Y dimension?
            else if (s->y < ((s->ymax)-1)) {
                *readyToSend = Pin(INTERNALY);
            }
            // Should this value be propagated in the Z dimension?
            else if ( (s->x < ((s->xmax)-1)) && (s->y < ((s->ymax)-1)) ) {
                *readyToSend = Pin(INTERNALZ);
            }

        }

    }

    // Called by POLite when system becomes idle
    inline bool step() {
        
        // Initialise pre-populated sides
        if ((s->x == 0) && !(s->inflags & EL1) && !(s->inflags & EL2) && !(s->inflags & EL3)) {
            s->inflags |= EL1;
            s->inflags |= EL2;
            s->inflags |= EL3;
            *readyToSend = Pin(INTERNALX);
            return true;
        }
        else if ((s->y == 0) && !(s->inflags & EL1) && !(s->inflags & EL2) && !(s->inflags & EL3)) {
            s->inflags |= EL1;
            s->inflags |= EL2;
            s->inflags |= EL3;
            *readyToSend = Pin(INTERNALY);
            return true;
        }
        else {
            return false;
        }
        
    }

    // Optionally send message to host on termination
    inline bool finish(volatile MatMessage* msg) {
    
        msg->dir = s->id;
        msg->val = s->aggregate;
        return true;
        
    }
};



#endif
