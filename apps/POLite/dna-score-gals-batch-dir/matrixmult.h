// SPDX-License-Identifier: BSD-2-Clause
#ifndef _HEAT_H_
#define _HEAT_H_

//#define POLITE_MAX_FANOUT 32
#include <POLite.h>

// Input reception and message validity flags
const uint32_t EL1 = 1 << 0;
const uint32_t EL2 = 1 << 1;
const uint32_t EL3 = 1 << 2;
const uint32_t AGG = 1 << 3;
const uint32_t TB = 1 << 4;
const uint32_t LASTNODE = 1 << 5;

// MatMessage dir -> Internal/External Plane Flags (Device ID used in finish call)
const uint32_t INTERNALX = 0;
const uint32_t INTERNALY = 1;
const uint32_t INTERNALZ = 2;
const uint32_t REVERSEX = 3;
const uint32_t REVERSEY = 4;
const uint32_t REVERSEZ = 5;
const uint32_t TRACEBACK = 6;
const uint32_t TRACEEDGESTART = 6;

// Smith-Waterman Constants
const int32_t MATCH = 3;
const int32_t MISMATCH = -3;
const int32_t GAP = -2;

struct MatMessage {
    
    // Direction literals listed above
    uint32_t dir;
    // Matrix Elements
    uint32_t val;
    
    // Largest aggregate seen
    uint32_t largestagg;
    // X coord Largest aggregate seen
    uint32_t largestx;
    // Y coord Largest aggregate seen
    uint32_t largesty;
    
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
    
    // Elements and Aggregate
    uint32_t element1, element2, element3, aggregate;
    // Largest element direction
    uint32_t largestdir;
    // Largest co-ords and aggregate
    uint32_t largestx, largesty, largestagg;

    // Reception flags
    uint32_t inflags;
        // Reception flags
    uint32_t sentflags;
    
    uint32_t step_cnt;
    
};

struct MatDevice : PDevice<MatState, None, MatMessage> {

    // Called once by POLite at start of execution
    inline void init() {
        
        s->step_cnt = 0;
        
        *readyToSend = No;
        
    }

    // Send handler
    inline void send(volatile MatMessage* msg) {
        
        if (*readyToSend == Pin(INTERNALX)) {
            
            msg->dir = INTERNALX;
            msg->val = s->aggregate;
            
            msg->largestagg = s->largestagg;
            msg->largestx = s->largestx;
            msg->largesty = s->largesty;
            
            s->sentflags |= EL1;
            
        }
        if (*readyToSend == Pin(INTERNALY)) {
            
            msg->dir = INTERNALY;
            msg->val = s->aggregate;
            
            msg->largestagg = s->largestagg;
            msg->largestx = s->largestx;
            msg->largesty = s->largesty;
            
            s->sentflags |= EL2;
            
        }
        if (*readyToSend == Pin(INTERNALZ)) {
            
            msg->dir = INTERNALZ;
            msg->val = s->aggregate;
            s->sentflags |= EL3;
            
        }
        if ((*readyToSend == Pin(REVERSEX)) || (*readyToSend == Pin(REVERSEY)) || (*readyToSend == Pin(REVERSEZ)) || (s->inflags & TB) ) {
            
            msg->dir = TRACEBACK;
            
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
        else if (*readyToSend == HostPin) {
            
            msg->dir = s->largestdir;
            
            if (s->largestdir != REVERSEZ)
            {
                msg->val = int('-');
            }
            else {
                msg->val = int(s->query);
            }
            
            // Are there more nucleotides in the traceback sequence?
            if (s->aggregate != 0) {
                
                *readyToSend = Pin(s->largestdir);
                
            }
            
        }
        else {
            
            *readyToSend = No;
        }
        
    }

    // Receive handler
    inline void recv(MatMessage* msg, None* edge) {
        
        // Is the message a traceback request?
        if (msg->dir == TRACEBACK) {
            *readyToSend = HostPin;
        }

        // Is the message travelling in the x dimension?
        if (msg->dir == INTERNALX) {
            
            if (msg->val >= abs(GAP)) {
                s->element1 = msg->val + GAP;
            }
            else {
                s->element1 = 0;
            }
            
            // Update largest aggregate seen if message agg bigger than stored agg
            if (msg->largestagg > s->largestagg) {
                s->largestagg = msg->largestagg;
                s->largestx = msg->largestx;
                s->largesty = msg->largesty;
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
            
            // Update largest aggregate seen if message agg bigger than stored agg
            if (msg->largestagg > s->largestagg) {
                s->largestagg = msg->largestagg;
                s->largestx = msg->largestx;
                s->largesty = msg->largesty;
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
        if ((s->inflags & EL1) && (s->inflags & EL2) && (s->inflags & EL3) && !(s->inflags & AGG)) {
            
            // Find largest element
            if (s->element1 >= s->element2 && s->element1 >= s->element3) {
                s->aggregate = s->element1;
                s->largestdir = REVERSEX;
            }
            else if (s->element2 >= s->element1 && s->element2 >= s->element3) {
                s->aggregate = s->element2;
                s->largestdir = REVERSEY;
            }
            else {
                s->aggregate = s->element3;
                s->largestdir = REVERSEZ;
            }
            
            // Check if agg is bigger than largest agg seen. If so, update.
            if (s->aggregate > s->largestagg) {
                s->largestagg = s->aggregate;
                s->largestx = s->x;
                s->largesty = s->y;
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
            
            s->inflags |= AGG;

        }

    }

    // Called by POLite when system becomes idle
    inline bool step() {
        
        s->step_cnt++;
        
        // Initialise pre-populated sides
        if ((s->x == 0) && !(s->inflags & EL1) && !(s->inflags & EL2) && !(s->inflags & EL3)) {
            s->inflags |= EL1;
            s->inflags |= EL2;
            s->inflags |= EL3;
            s->inflags |= AGG;
            *readyToSend = Pin(INTERNALX);
        }
        else if ((s->y == 0) && !(s->inflags & EL1) && !(s->inflags & EL2) && !(s->inflags & EL3)) {
            s->inflags |= EL1;
            s->inflags |= EL2;
            s->inflags |= EL3;
            s->inflags |= AGG;
            *readyToSend = Pin(INTERNALY);
        }
        
        // Are we the last node and has the forward smith-waterman algorithm completed? If so start retracement . . 
        else if ( (s->x == ((s->xmax)-1)) && (s->y == ((s->ymax)-1)) && (s->inflags & EL1) && (s->inflags & EL2) && (s->inflags & EL3) && !(s->inflags & LASTNODE) ) {
            
            // Indicate Traceback messgae to be sent
            s->inflags |= LASTNODE;
            
            // Are we the largest aggregate seen?
            if ((s->largestx == s->x) && (s->largesty == s->y)) {
                
                //YES -> Start traceback
                *readyToSend = HostPin;
                
            }
            else {
                
                //NO -> Calculate edge to largest node and send traceback message
                s->largestdir = (s->largesty * s->xmax) + s->largestx + TRACEEDGESTART;
                
                // Indicate Traceback messgae to be sent
                s->inflags |= TB;
                
                // Call edge to largest aggregate
                *readyToSend = Pin(s->largestdir);
            }
            
        }
        
        
        if (s->step_cnt > 100) {
            return false;
        }
        else {
            return true;
        }
        
        
    }

    // Optionally send message to host on termination
    inline bool finish(volatile MatMessage* msg) {
    
        /*
        msg->dir = s->id;
        msg->val = s->largestx;
        return true;
        */
        
        return false;
        
    }
};



#endif
