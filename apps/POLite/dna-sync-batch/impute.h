// SPDX-License-Identifier: BSD-2-Clause
#ifndef _IMPUTE_H_
#define _IMPUTE_H_

#define POLITE_DUMP_STATS
#define POLITE_COUNT_MSGS
#include "myPOLite.h"
#include "params.h"

// ImpMessage types
const uint32_t FORWARD = 0;
const uint32_t BACKWARD =  1;
const uint32_t FWDACCA = 4;
const uint32_t BWDACCA =  5;

// Flags
//const uint32_t INIT = (1 << 0); //1
//const uint32_t FINAL = (1 << 1); //2
const uint32_t ALPHA = (1 << 2); //4
const uint32_t BETA = (1 << 3); //8
const uint32_t ALPHAACCA = (1 << 4); //16
const uint32_t BETAACCA = (1 << 5); //32
const uint32_t ALPHAPOST = (1 << 6); //64
const uint32_t BETAPOST = (1 << 7); //128

struct ImpMessage {
    
    // message type
    uint32_t msgtype;
    // state number
    uint32_t stateNo;
    // match
    uint32_t match;
    // message value
    float val;
    
};

struct ImpState {
    
    // Device id
    uint32_t id; //
    // Message Counters
    uint32_t fwdRecCnt, bwdRecCnt, fwdAccaCnt, bwdAccaCnt; //
    // Mesh Coordinates
    uint32_t x, y; //
    // Match
    uint32_t match; //
    // Ready Flags
    uint32_t rdyFlags; //
    // Next Ready Flags
    uint32_t nxtRdyFlags; //
    // Sent Flags
    uint32_t sentFlags; //
    // Target Haplotype Counter
    uint32_t targCnt;
    // Node Alphas
    float alpha; //
    // Node Betas
    float beta; //
    // Node Betas
    float fwdSame; //
    // Node Betas
    float fwdDiff; //
    // Node Betas
    float bwdSame; //
    // Node Betas
    float bwdDiff; //
    // Last Node in Column Only
    // Node Betas
    float betaPosterior; //
    // Node Betas
    float alphaPosterior; //
    // Node Old Alphas
    float oldAlpha; //
    // Node Old Betas
    float oldBeta; //
    
    
#ifdef LINEARINTERP    
    // Linear Interpolation Results
    // Alpha
    float alphaLin[LINRATIO - 1u];
    // Beta
    float betaLin[LINRATIO - 1u];
    // Previous Alpha
    float prevAlpha;
    // Previous Beta
    float prevBeta;
#endif
    
#ifdef IMPDEBUG
        // Sent Counter
        uint32_t sentCnt;
#endif
    
};

struct ImpDevice : PDevice<ImpState, None, ImpMessage> {

    // Called once by POLite at start of execution
    inline void init() {
        
        s->fwdRecCnt = 0u;
        s->bwdRecCnt = 0u;
        s->fwdAccaCnt = 0u;
        s->bwdAccaCnt = 0u;
        s->rdyFlags = 0u;
        s->sentFlags = 0u;
        s->targCnt = 0u;
        
        s->alpha = 0.0f;
        s->beta = 0.0f;
        
#ifdef IMPDEBUG
        s->sentCnt = 0u;
#endif
        
        *readyToSend = No;
        
    }

    // Send handler
    inline void send(volatile ImpMessage* msg) {
        
        if (*readyToSend == Pin(FORWARD)) {
            
            msg->msgtype = FORWARD;
            msg->stateNo = s->y;
            msg->val = s->alpha;
            
            s->rdyFlags &= (~ALPHA);
            s->sentFlags |= ALPHA;
            
#ifdef IMPDEBUG
            s->sentCnt++;
#endif
        
        }
        
        if (*readyToSend == Pin(BACKWARD)) {
            
            msg->msgtype = BACKWARD;
            msg->stateNo = s->y;
            msg->match = s->match;
            msg->val = s->beta;
            
            s->rdyFlags &= (~BETA);
            s->sentFlags |= BETA;
        

        }
        
        if (*readyToSend == Pin(FWDACCA)) {
            
            msg->msgtype = FWDACCA;
            msg->val = s->oldAlpha;
            
            s->rdyFlags &= (~ALPHAACCA);
            s->sentFlags |= ALPHAACCA;
        
        }
        
        if (*readyToSend == Pin(BWDACCA)) {
            
            msg->msgtype = BWDACCA;
            msg->val = s->oldBeta;
            
            s->rdyFlags &= (~BETAACCA);
            s->sentFlags |= BETAACCA;
        
        }
        
        if ((s->rdyFlags & ALPHA) && !(s->sentFlags & ALPHA)) {
            // Have We calculated alpha but not sent it?
            
            *readyToSend = Pin(FORWARD);
            
        }
        else if ((s->rdyFlags & BETA) && !(s->sentFlags & BETA)) {
            // Have We calculated beta but not sent it?
            
            *readyToSend = Pin(BACKWARD);
            
        }
        else if ((s->rdyFlags & ALPHAACCA) && !(s->sentFlags & ALPHAACCA)) {
            // Have We calculated alpha but not sent it?
            
            *readyToSend = Pin(FWDACCA);
            
        }
        else if ((s->rdyFlags & BETAACCA) && !(s->sentFlags & BETAACCA)) {
            // Have We calculated beta but not sent it?
            
            *readyToSend = Pin(BWDACCA);
            
        }
        else {
            *readyToSend = No;
        }
        
    }

    // Receive handler
    inline void recv(ImpMessage* msg, None* edge) {
        
        // Received Alpha Inductive Message (Forward Algo)
        if (msg->msgtype == FORWARD) {

            if (msg->stateNo == s->y) {
                
                s->alpha += msg->val * s->fwdSame;
#ifdef LINEARINTERP                 
                s->prevAlpha = msg->val;
#endif                
            }
            else {
                
                s->alpha += msg->val * s->fwdDiff;
                
            }
            
            s->fwdRecCnt++;
            
            // Has the summation of the alpha been calculated?
            if (s->fwdRecCnt == NOOFSTATES) {
                
                // Calculate the final alpha
                if (s->match != 2u) {
                    if (s->match == 1u) {
                        s->alpha = s->alpha * (1.0f - (1.0f / ERRORRATE));
                    }
                    else {
                        s->alpha = s->alpha * (1.0f / ERRORRATE);
                    }
                }
                
                
                // Send alpha inductively if not in last column
                if ( (s->x) != (NOOFOBS - 1u) ) {
                    s->nxtRdyFlags |= ALPHA;
                }
                
                // Send alpha accumulation message
                if (s->y != NOOFSTATES - 1) {
                    s->nxtRdyFlags |= ALPHAACCA;
                }
                
                s->fwdRecCnt = 0u;
                
            }
        
        }
        
        
        if (msg->msgtype == BACKWARD) {
            
            float emissionProb = 0.0f;
            
            if (msg->match == 2u) {
                emissionProb = 1.0f;
            }
            else if (msg->match == 1u) {
                emissionProb = (1.0f - (1.0f / ERRORRATE));
            }
            else {
                emissionProb = (1.0f / ERRORRATE);
            }
            
            if (msg->stateNo == s->y) {
                
                s->beta += msg->val * s->bwdSame * emissionProb;
#ifdef LINEARINTERP                 
                s->prevBeta = msg->val;
#endif                
            }
            else {
                
                s->beta += msg->val * s->bwdDiff * emissionProb;
                
            }
            
            s->bwdRecCnt++;
            
            // Has the summation of the beta been calculated?
            if (s->bwdRecCnt == NOOFSTATES) {
                
                // Send beta message if we are not the first node
                if ((s->x) != 0u) {
                    s->nxtRdyFlags |= BETA;
                }
                
                // Send alpha accumulation message
                if (s->y != NOOFSTATES - 1) {
                    s->nxtRdyFlags |= BETAACCA;
                }
                
                s->bwdRecCnt = 0u;
                    
            }
        
        }
        
        // Received Alpha Accumulation Message (Forward Algo)
        if (msg->msgtype == FWDACCA) {
            
            s->fwdAccaCnt++;
            s->alphaPosterior += msg->val;
            
            if (s->fwdAccaCnt == NOOFSTATES - 1) {
                
                s->alphaPosterior += s->oldAlpha;
                s->nxtRdyFlags |= ALPHAPOST;
                
                s->fwdAccaCnt = 0u;
                
            }
            
        }
        
        // Received Beta Accumulation Message (Forward Algo)
        if (msg->msgtype == BWDACCA) {
            
            s->bwdAccaCnt++;
            s->betaPosterior += msg->val;
            
            if (s->bwdAccaCnt == NOOFSTATES - 1) {
                
                s->betaPosterior += s->oldBeta;
                s->nxtRdyFlags |= BETAPOST;
                
                s->bwdAccaCnt = 0u;
                
            }
            
        }

    }

    // Called by POLite when system becomes idle
    inline bool step() {
        
        // Copy Values Over to enable both stages of processing (induction and accumulation)
        s->oldAlpha = s->alpha;
        s->oldBeta = s->beta;
        
        // Transfer and CLear Ready Flags
        s->rdyFlags = s->nxtRdyFlags;
        s->nxtRdyFlags = 0u;
        
        // Clear sent flags
        s->sentFlags = 0u;
        
        // Clear Posterior values -> JPM This needs reading if data is to be transferred out
        s->alphaPosterior = 0u;
        s->betaPosterior = 0u;
        
        // Calculate and send initial alphas
        if ((s->x == 0) && (s->targCnt < NOOFTARG)) {
            
            s->alpha = 1.0f / NOOFSTATES;
            s->oldAlpha = 1.0f / NOOFSTATES;
            s->rdyFlags |= ALPHA;
            
            if (s->y != NOOFSTATES - 1) {
                s->rdyFlags |= ALPHAACCA;
            }
            
            //*readyToSend = Pin(FORWARD);
            
            //return true;
            
        }
        // Calculate and send initial betas
        else if (((s->x) == (NOOFOBS - 1u)) && (s->targCnt < NOOFTARG)) {
            
            s->beta = 1.0f;
            s->oldBeta = 1.0f;
            s->rdyFlags |= BETA;
            
            if (s->y != NOOFSTATES - 1) {
                s->rdyFlags |= BETAACCA;
            }
            
            //*readyToSend = Pin(BACKWARD);
            
            //return true;
            
        }
        
        s->targCnt++;
        
        if ((s->rdyFlags & ALPHA) && !(s->sentFlags & ALPHA)) {
            
            *readyToSend = Pin(FORWARD);
            
            return true;
            
        }
        
        if ((s->rdyFlags & BETA) && !(s->sentFlags & BETA)) {
            
            *readyToSend = Pin(BACKWARD);
            
            return true;
            
        }
        
        if ((s->rdyFlags & ALPHAACCA) && !(s->sentFlags & ALPHAACCA)) {
            
            *readyToSend = Pin(FWDACCA);
            
            return true;
            
        }
        
        if ((s->rdyFlags & BETAACCA) && !(s->sentFlags & BETAACCA)) {
            
            *readyToSend = Pin(BWDACCA);
            
            return true;
            
        }
        
        // There is nothing to be sent
        return false;
        
    }

    // Optionally send message to host on termination
    inline bool finish(volatile ImpMessage* msg) {

        #ifdef IMPDEBUG
        
            msg->msgtype = s->id;
            msg->val = (float)s->sentCnt;
            //msg->val = s->fwdDiff;
            return true;
        
        #endif
        
        return false;

    }
};



#endif