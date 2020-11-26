// SPDX-License-Identifier: BSD-2-Clause
#ifndef _IMPUTE_H_
#define _IMPUTE_H_

#define POLITE_DUMP_STATS
#define POLITE_COUNT_MSGS
#include <POLite.h>
#include "model.h"

// ImpMessage types
const uint32_t ALPHAINDUCT = 0;
const uint32_t BETAINDUCT =  1;
const uint32_t TERMINATION = 2;

// Flags
const uint32_t INIT = 1 << 0;
const uint32_t FINAL = 1 << 1;
const uint32_t ALPHA = 1 << 2;
const uint32_t BETA = 1 << 3;

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
    uint32_t aindreccount, bindreccount, finreccount, ccountl, ccountu;
    // Mesh Coordinates
    uint32_t x, y;
    // Mesh Dimnesions
    uint32_t xmax, ymax;
    // Ready Flags
    uint32_t readyflags;
    // Sent Flags
    uint32_t sentflags;
    // Initial Probability
    float initprob;
    // Transition Probability
    float transprob;
    // Emission Probability
    float emisprob;
    // Node Alphas
    float alpha;
    // Node Betas
    float beta;
    // Node Posteriors
    float posterior;
    // P(O|lambda)
    float answer;
    
    #ifdef IMPDEBUG
        // Step Counter
        uint32_t stepcount;
    #endif
    
};

struct ImpDevice : PDevice<ImpState, None, ImpMessage> {

    // Called once by POLite at start of execution
    inline void init() {
        
        #ifdef IMPDEBUG
            s->stepcount = 0;
        #endif
        
        *readyToSend = No;
        
    }

    // Send handler
    inline void send(volatile ImpMessage* msg) {
        
        if (*readyToSend == Pin(ALPHAINDUCT)) {
            
            msg->msgtype = ALPHAINDUCT;
            msg->val = s->alpha;
            s->sentflags |= ALPHA;
        
        }
        
        if (*readyToSend == Pin(BETAINDUCT)) {
            
            msg->msgtype = BETAINDUCT;
            msg->val = s->beta;
            s->sentflags |= BETA;

        }
        
        if (*readyToSend == Pin(TERMINATION)) {
            
            msg->msgtype = TERMINATION;
            msg->val = s->posterior;
            
        }
        
        if (*readyToSend == HostPin) {
            
            #ifndef IMPDEBUG
                msg->msgtype = s->ccountu;
                msg->val = s->ccountl;
            #endif
        
        }
        
        if ((s->readyflags & ALPHA) && !(s->sentflags & ALPHA)) {
            // Have We calculated alpha but not sent it?
            
            *readyToSend = Pin(ALPHAINDUCT);
            
        }
        else if ((s->readyflags & BETA) && !(s->sentflags & BETA)) {
            // Have We calculated beta but not sent it?
            
            *readyToSend = Pin(BETAINDUCT);
            
        }
        else {
            *readyToSend = No;
        }
        
    }

    // Receive handler
    inline void recv(ImpMessage* msg, None* edge) {
        
        
        // Received Alpha Inductive Message (Forward Algo)
        if (msg->msgtype == ALPHAINDUCT) {

            s->alpha += msg->val * s->transprob;
            s->aindreccount++;
            
            // Has the summation of the alpha been calculated?
            if (s->aindreccount == s->ymax) {
                
                // Calculate the final alpha
                s->alpha = s->alpha * s->emisprob;
                
                s->posterior = s->posterior * s->alpha;
                
                // Send alpha inductively if not in last column else send termination if in last column and not in last row
                if ((s->x) != (s->xmax)-1) {
                    s->readyflags |= ALPHA;
                    *readyToSend = Pin(ALPHAINDUCT);
                }
                else if ((s->y) != (s->ymax)-1) {
                    *readyToSend = Pin(TERMINATION);
                }
                
            }
        
        }
        
        
        if (msg->msgtype == BETAINDUCT) {
            
            s->beta += msg->val * s->transprob * s->emisprob;
            s->bindreccount++;
            
            // Has the summation of the beta been calculated?
            if (s->bindreccount == s->ymax) {
                
                s->posterior = s->posterior * s->beta;
                
                // Send beta message if we are not the first node
                if ((s->x) != 0) {
                    s->readyflags |= BETA;
                    *readyToSend = Pin(BETAINDUCT);
                }

                    
            }
        
        }
        
        
        if (msg->msgtype == TERMINATION) {
            s->answer += msg->val;
            s->finreccount++;
            
        }
        
        // Have all termination and alpha messages been received for the final node?
        if ((s->finreccount == (s->ymax-1)) && (s->aindreccount == s->ymax)) {
            // Add own alpha for final answer
            s->answer += s->posterior;
            
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
        s->stepcount++;
        
        if (s->stepcount > 100) {
            return false;
        }
        #endif
        
        // Calculate and send initial alphas
        if ((s->x == 0) && !(s->sentflags & INIT)) {
            
            s->alpha = s->initprob * s->emisprob;
            s->sentflags |= INIT;
            *readyToSend = Pin(ALPHAINDUCT);
            
        }
        // Calculate and send initial betas
        else if (((s->x) == (s->xmax)-1) && !(s->sentflags & FINAL)) {
            
            s->beta = s->initprob;
            s->sentflags |= FINAL;
            *readyToSend = Pin(BETAINDUCT);
            
        }
        
        return true;
        
    }

    // Optionally send message to host on termination
    inline bool finish(volatile ImpMessage* msg) {
        
        #ifdef IMPDEBUG
        
            msg->msgtype = s->id;
            msg->val = s->posterior;
            return true;
        
        #endif
        

        return false;

        
        

    }
};



#endif