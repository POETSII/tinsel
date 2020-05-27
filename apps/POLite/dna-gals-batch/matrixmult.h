// SPDX-License-Identifier: BSD-2-Clause
#ifndef _HEAT_H_
#define _HEAT_H_

#define POLITE_MAX_FANOUT 32
#include <POLite.h>

// MatMessage types
const uint32_t INDUCTION = 0;
const uint32_t TERMINATION = 1;

// Flags
const uint32_t INIT = 1 << 0;

struct MatMessage {
    
    // message type
    uint32_t msgtype;
    // message value
    float val;
    
};

struct MatState {
    
    // Device id
    uint32_t id;
    // Message Counters
    uint32_t indreccount, finreccount, sendcount;
    // Mesh Coordinates
    uint32_t x, y;
    // Mesh Dimnesions
    uint32_t xmax, ymax;
    // Sent Flags
    uint32_t sentflags;
    // Initial Probability
    float initprob;
    // Transition Probability
    float transprob;
    // Emission Probability
    float emisprob;
    // Node Alpha
    float alpha;
    // P(O|lambda)
    float answer;
    
};

struct MatDevice : PDevice<MatState, None, MatMessage> {

    // Called once by POLite at start of execution
    inline void init() {
        
        *readyToSend = No;
        
    }

    // Send handler
    inline void send(volatile MatMessage* msg) {
        
        if (s->x != (s->xmax-1)) {
        
            msg->msgtype = INDUCTION;
            msg->val = s->alpha * s->transprob;
            s->sendcount++;
            
            if (s->sendcount < ymax) {
                
                *readyToSend = Pin(s->sendcount);
                
            }
            else {
                
                // No more stored values to send
                *readyToSend = No;
            }
        
        }
        else {
            
            msg->msgtype = TERMINATION;
            msg->val = s->alpha;
            
            *readyToSend = No;
            
        }
        
    }

    // Receive handler
    inline void recv(MatMessage* msg, None* edge) {
        
        if (msg->msgtype == INDUCTION) {

            s->alpha += msg->val;
            s->indreccount++;
            
            // Has the summation of the alpha been calculated?
            if (s->indreccount == ymax) {
                
                // Calculate the final alpha
                s->alpha = s->alpha * s->emisprob;
                
                if (x != (xmax-1)) {
                    *readyToSend = Pin(s->sendcount);
                }
                else {
                    *readyToSend = Pin(0);
                }
                    
            }
        
        }
        if (msg->msgtype == TERMINATION) {
            s->answer += msg->val;
            s->finreccount++;
        }

    }

    // Called by POLite when system becomes idle
    inline bool step() {
        
        // Calculate and send initial alphas
        if ((s->x == 0) && !(s->sentflags & INIT)) {
            
            s->alpha = s->initprob * s->emisprob;
            s->sentflags |= INIT;
            *readyToSend = Pin(s->sendcount);
            
            return true;
        }
        if (((s->x) == (s->xmax)-1) && ((s->y) == (s->ymax)-1) && (s->finreccount != s->ymax)) {
            return true;
        }
        else {
            return false;
        }
        
    }

    // Optionally send message to host on termination
    inline bool finish(volatile MatMessage* msg) {
        
        if (((s->x) == (s->xmax)-1) && ((s->y) == (s->ymax)-1)) {
            msg->val = s->answer;
            return true;
        }
        else {
            return false;
        }
        
        

    }
};



#endif
