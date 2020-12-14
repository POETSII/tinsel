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
const uint32_t ACCUMULATE = 4;
//const uint32_t BWDACCA =  5;

// Flags

const uint32_t ALPHA = (1 << 0); //1
const uint32_t BETA = (1 << 1); //2
const uint32_t ACCA = (1 << 2); //4
const uint32_t ALPHAPOST = (1 << 3); //8
const uint32_t BETAPOST = (1 << 4); //16
const uint32_t ALLELECNTS = (1 << 5); //32

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
    uint32_t fwdRecCnt, bwdRecCnt, accaCnt; //
    // Mesh Coordinates
    uint32_t x, y; //
    // Node Label
    uint32_t label; //
    // Match
    uint32_t match; //
    // State Flags
    uint32_t stateFlags; //
    // Ready Flags
    uint32_t rdyFlags; //
    // Next Ready Flags
    uint32_t nxtRdyFlags; //
    // Sent Flags
    uint32_t sentFlags; //
    // Target Haplotype Counter
    uint32_t targCnt; //
    // Node Alphas
    float alpha; //
    // Node Betas
    float beta; //
    // Major Posterior Probability
    float majPosterior; //
    // Minor Posterior Probability
    float minPosterior; //
    // Node Betas
    float fwdSame; //
    // Node Betas
    float fwdDiff; //
    // Node Betas
    float bwdSame; //
    // Node Betas
    float bwdDiff; //
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
        uint32_t sentCnt; //
#endif
    
};

struct ImpDevice : PDevice<ImpState, None, ImpMessage> {

    // Called once by POLite at start of execution
    inline void init() {
        
        s->fwdRecCnt = 0u;
        s->bwdRecCnt = 0u;
        s->accaCnt = 0u;
        s->targCnt = 0u;
        
        s->stateFlags = 0u;
        s->rdyFlags = 0u;
        s->nxtRdyFlags = 0u;
        s->sentFlags = 0u;
        
        s->alpha = 0.0f;
        s->beta = 0.0f;
        s->majPosterior = 0.0f;
        s->minPosterior = 0.0f;
        
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
            msg->val = s->oldAlpha;
            
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
            msg->val = s->oldBeta;
            
            s->rdyFlags &= (~BETA);
            s->sentFlags |= BETA;
        

        }
        
        if (*readyToSend == Pin(ACCUMULATE)) {
            
            msg->msgtype = ACCUMULATE;
            msg->val = s->oldAlpha;
            
            s->rdyFlags &= (~ACCA);
            s->sentFlags |= ACCA;
        
        }
        
        if ((s->rdyFlags & ALPHA) && !(s->sentFlags & ALPHA)) {
            // Have We calculated alpha but not sent it?
            
            *readyToSend = Pin(FORWARD);
            
        }
        else if ((s->rdyFlags & BETA) && !(s->sentFlags & BETA)) {
            // Have We calculated beta but not sent it?
            
            *readyToSend = Pin(BACKWARD);
            
        }
        else if ((s->rdyFlags & ACCA) && !(s->sentFlags & ACCA)) {
            // Have We calculated alpha but not sent it?
            
            *readyToSend = Pin(ACCUMULATE);
            
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
                
                // Update posterior probability flags
                s->stateFlags |= ALPHAPOST;
                
                
                // Send alpha inductively if not in last column
                if ( (s->x) != (NOOFOBS - 1u) ) {
                    s->nxtRdyFlags |= ALPHA;
                }
                
                // Send accumulation message if posterior probability is complete
                if ((s->stateFlags & ALPHAPOST) && (s->stateFlags & BETAPOST) && (s->y != NOOFSTATES - 1)) {
                    
                    s->alpha = s->alpha * s->beta;
                    s->nxtRdyFlags |= ACCA;
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
                
                // Update posterior probability flags
                s->stateFlags |= BETAPOST;
                
                // Send beta message if we are not the first node
                if ((s->x) != 0u) {
                    s->nxtRdyFlags |= BETA;
                }
                
                // Send accumulation message if posterior probability is complete
                if ((s->stateFlags & ALPHAPOST) && (s->stateFlags & BETAPOST) && (s->y != NOOFSTATES - 1)) {
                    
                    s->alpha = s->alpha * s->beta;
                    s->nxtRdyFlags |= ACCA;
                }
                
                s->bwdRecCnt = 0u;
                    
            }
        
        }
        
        // Received Alpha Accumulation Message (Forward Algo)
        if (msg->msgtype == ACCUMULATE) {
            
            s->accaCnt++;
            
            // If the transmitting node is a major allele . . 
            if (msg->match == 0u) {
                
                s->majPosterior += msg->val;
                
            }
            else {
                
                s->minPosterior += msg->val;
                
            }
            
            if (s->accaCnt == NOOFSTATES - 1) {
                
                // If the final node is a major allele . . (alpha at this point = alpha * beta (posterior probability))
                if (s->label == 0u) {
                    
                    s->majPosterior += s->alpha;
                    
                }
                else {
                    
                    s->minPosterior += s->alpha;
                    
                }
                
                s->nxtRdyFlags |= ALLELECNTS;

                s->accaCnt = 0u;
                
            }
            
        }

    }

    // Called by POLite when system becomes idle
    inline bool step() {
        
        // Copy Values Over to enable both stages of processing (induction and accumulation)
        s->oldAlpha = s->alpha;
        s->oldBeta = s->beta;
        
        // Clear alpha/beta values ready for next timestep
        s->alpha = 0.0f;
        s->beta = 0.0f;
        
        // Transfer and Clear Ready Flags
        s->rdyFlags = s->nxtRdyFlags;
        s->nxtRdyFlags = 0u;
        
        // Clear sent flags
        s->sentFlags = 0u;
        
        // Clear Posterior values -> JPM This needs reading if data is to be transferred out
        s->majPosterior = 0u;
        s->minPosterior = 0u;
        
        // Calculate and send initial alphas
        if ((s->x == 0) && (s->targCnt < NOOFTARG)) {
            
            s->alpha = 1.0f / NOOFSTATES;
            s->oldAlpha = 1.0f / NOOFSTATES;
            s->rdyFlags |= ALPHA;
            s->stateFlags |= ALPHAPOST;
            
            //*readyToSend = Pin(FORWARD);
            
            //return true;
            
        }
        // Calculate and send initial betas
        else if (((s->x) == (NOOFOBS - 1u)) && (s->targCnt < NOOFTARG)) {
            
            s->beta = 1.0f;
            s->oldBeta = 1.0f;
            s->rdyFlags |= BETA;
            s->stateFlags |= BETAPOST;
            
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
        
        if ((s->rdyFlags & ACCA) && !(s->sentFlags & ACCA)) {
            
            *readyToSend = Pin(ACCUMULATE);
            
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