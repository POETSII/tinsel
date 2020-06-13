// SPDX-License-Identifier: BSD-2-Clause
#ifndef _IMPUTE_H_
#define _IMPUTE_H_

#define POLITE_MAX_FANOUT 4000
#include <POLite.h>
#include "model.h"
#include <math.h>

// ImpMessage types
const uint32_t INDUCTION = 0;
const uint32_t TERMINATION = 1;

// Flags
const uint32_t INIT = 1 << 0;

struct ImpMessage {
    
    // message type
    uint32_t msgtype;
    // message value
    float val;
    
};

struct ImpState {
    
    // Device id
    uint32_t id;
    // Message Counters
    uint32_t indreccount, finreccount, ccountl, ccountu;
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
    
    #ifdef IMPDEBUG
        // Step Counter
        uint32_t stepcount; //JPMDEBUG
    #endif
    
};

struct ImpDevice : PDevice<ImpState, None, ImpMessage> {

    // Called once by POLite at start of execution
    inline void init() {
        
        #ifdef IMPDEBUG
            s->stepcount = 0; //JPMDEBUG
        #endif
        
        *readyToSend = No;
        
    }

    // Send handler
    inline void send(volatile ImpMessage* msg) {
        
        if (s->x != (s->xmax-1)) {
        
            msg->msgtype = INDUCTION;
            msg->val = s->alpha;
                
            *readyToSend = No;

        }
        else {
            
            if (s->y != (s->ymax-1)) {
            
                msg->msgtype = TERMINATION;
                msg->val = s->alpha;
                
                *readyToSend = No;
                
            }
            else {
                
                #ifndef IMPDEBUG
                    msg->msgtype = s->ccountu;
                    msg->val = s->ccountl;
                #endif
                
                *readyToSend = No;
            }
            
        }
        
    }

    // Receive handler
    inline void recv(ImpMessage* msg, None* edge) {
        
        if (msg->msgtype == INDUCTION) {
            
            s->ccountl = 1 - exp ((-4 * 1000000 * 0.0001) / NOOFSTATES);
            s->ccountu = 1 - s->ccountl;

            s->alpha += msg->val * s->transprob;
            s->indreccount++;
            
            // Has the summation of the alpha been calculated?
            if (s->indreccount == s->ymax) {
                
                // Calculate the final alpha
                s->alpha = s->alpha * s->emisprob;
                
                // Send message if we are not the last node
                if (!(((s->x) == (s->xmax)-1) && ((s->y) == (s->ymax)-1))) {
                    *readyToSend = Pin(0);
                }

                    
            }
        
        }
        
        if (msg->msgtype == TERMINATION) {
            s->answer += msg->val;
            s->finreccount++;
            
        }
        
        // Have all termination and induction messages been received for the final node?
        if ((s->finreccount == (s->ymax-1)) && (s->indreccount == s->ymax)) {
            // Add own alpha for final answer
            s->answer += s->alpha;
            
            #ifdef TINSEL
                tinselPerfCountStop();
                s->ccountl = tinselCycleCount();
                s->ccountu = tinselCycleCountU();
            #endif
            
            *readyToSend = HostPin;
        }

    }

    // Called by POLite when system becomes idle
    inline bool step() {
        
        // If we are the last node, reset and start the cycle counters
        if (((s->x) == (s->xmax)-1) && ((s->y) == (s->ymax)-1)) {
            #ifdef TINSEL
                tinselPerfCountReset();
                tinselPerfCountStart();
            #endif
        }
            
        #ifdef IMPDEBUG
        s->stepcount++; //JPMDEBUG
        
        if (s->stepcount > 100) { //JPMDEBUG
            return false;
        }
        
        #endif
        
        // Calculate and send initial alphas
        if ((s->x == 0) && !(s->sentflags & INIT)) {
            
            s->alpha = s->initprob * s->emisprob;
            s->sentflags |= INIT;
            *readyToSend = Pin(0);
            
        }

        
        return true;
        
    }

    // Optionally send message to host on termination
    inline bool finish(volatile ImpMessage* msg) {
        
        #ifdef IMPDEBUG
        
            msg->msgtype = s->id; //JPMDEBUG
            msg->val = s->sentflags; //JPMDEBUG
            return true; //JPMDEBUG
        
        #endif
        

        return false;

        
        

    }
};



#endif
