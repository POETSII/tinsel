#include "imputation.h"

#define PREVA (1u << 0u)
#define NEXTA (1u << 1u)
#define PREVB (1u << 2u)
#define NEXTB (1u << 3u)

/*****************************************************
 * Hidden Markov Model and Linear Interpolation Node
 * ***************************************************
 * This code performs the stephens and li model for imputation
 * 
 * USE ARRAYS FOR MATCH / ALPHAS TO PUSH THE PROCESSING TO MESSAGE TIME IN FAVOUR OF POETS
 * ****************************************************/

int main()
{
    // Get thread id
    int me = tinselId();
    uint8_t localThreadID = me % (1 << TinselLogThreadsPerMailbox);
    uint8_t stateNo = localThreadID % 8u;

    // Received values
    uint32_t* baseAddress = tinselHeapBase();
    uint32_t observationNo = *baseAddress;
    uint32_t match = *(baseAddress + 1u);
    uint32_t fwdKey = *(baseAddress + 2u);
    uint32_t bwdKey = *(baseAddress + 3u);
    float fwdSame = *(float*)(baseAddress + 4u);
    float fwdDiff = *(float*)(baseAddress + 5u);
    float bwdSame = *(float*)(baseAddress + 6u);
    float bwdDiff = *(float*)(baseAddress + 7u);
    uint32_t prevThread = *(baseAddress + 8u);
    
    // Populate local genetic distances and calculate total genetic distance
    float dmLocal[LINRATIO] = {0.0f};
    float totalDistance = 0.0f;
    
    for (uint32_t x = 0u; x < LINRATIO; x++) {
        
        dmLocal[x] = *(float*)(baseAddress + 9u + x);;
        totalDistance += dmLocal[x];
        
    }
    
    // Get host id
    int host = tinselHostId();
    
    float alpha = 0.0f;
    float beta = 0.0f;
    float alphaLin[9] = {0.0f};
    float betaLin[9] = {0.0f};
    
    float prevAlpha = 0.0f;
    float nextAlpha = 0.0f;
    float prevBeta = 0.0f;
    float nextBeta = 0.0f;
    
    uint8_t rdyFlags = 0u;
    /*
    // Send to host
    volatile HostMessage* msgHost = tinselSendSlot();
    
    tinselWaitUntil(TINSEL_CAN_SEND);
    msgHost->msgType = FORWARD;
    msgHost->observationNo = observationNo;
    msgHost->stateNo = stateNo;
    msgHost->val = localThreadID;

    tinselSend(host, msgHost);
    
    // Send to host
    //volatile HostMessage* msgHost = tinselSendSlot();

    tinselWaitUntil(TINSEL_CAN_SEND);
    msgHost->msgType = BACKWARD;
    msgHost->observationNo = observationNo;
    msgHost->stateNo = stateNo;
    msgHost->val = prevThread;

    tinselSend(host, msgHost);
    */
    
    // Startup for forward algorithm
    if (observationNo == 0u) {
        
        // Calculate initial probability
        alpha = 1.0f / NOOFSTATES;
        
        // Multiply alpha by emission probability
        if (match == 1u) {
            alpha = alpha * (1.0f - (1.0f / ERRORRATE));
        }
        else {
            alpha = alpha * (1.0f / ERRORRATE);
        }
        
        prevAlpha = alpha;
        rdyFlags |= PREVA;
        
        // Get pointers to mailbox message slot
        volatile ImpMessage* msgOut = tinselSendSlot();
        
        // Prepare message to send to HMM node
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut->msgType = FORWARD;
        msgOut->stateNo = stateNo;
        msgOut->val = alpha;
        
        // Propagate to next column
        tinselKeySend(fwdKey, msgOut);
        
        // Send to host
        volatile HostMessage* msgHost = tinselSendSlot();

        tinselWaitUntil(TINSEL_CAN_SEND);
        msgHost->msgType = FORWARD;
        msgHost->observationNo = observationNo * LINRATIO;
        msgHost->stateNo = stateNo;
        msgHost->val = alpha;

        tinselSend(host, msgHost);
    
    }
    
    // Startup for backward algorithm
    if (observationNo == (NOOFTARGMARK - 1u)) {
      
        // Get pointers to mailbox message slot
        volatile ImpMessage* msgOut = tinselSendSlot();
        
        beta = 1.0f;
        
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut->msgType = BACKWARD;
        msgOut->match = match;
        msgOut->stateNo = stateNo;
        msgOut->val = beta;
        
        // Propagate to previous column
        tinselKeySend(bwdKey, msgOut);
        
        prevBeta = beta;
        rdyFlags |= PREVB;
        
        // Propagate beta to previous thread as prev beta
        
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut->msgType = BWDLIN;
        msgOut->match = match;
        msgOut->stateNo = stateNo;
        msgOut->val = beta;
        
        // Propagate to previous column
        tinselSend(prevThread, msgOut);

        volatile HostMessage* msgHost = tinselSendSlot();

        tinselWaitUntil(TINSEL_CAN_SEND);
        msgHost->msgType = BACKWARD;
        msgHost->observationNo = observationNo * LINRATIO;
        msgHost->stateNo = stateNo;
        msgHost->val = beta;

        tinselSend(host, msgHost);
        
    }
        
    uint8_t fwdRecCnt = 0u;
    uint8_t bwdRecCnt = 0u;
    
    while (1u) {
        tinselWaitUntil(TINSEL_CAN_RECV);
        volatile ImpMessage* msgIn = tinselRecv();
        
        // Handle forward messages
        if (msgIn->msgType == FORWARD) {
            
            fwdRecCnt++;
            
            if (msgIn->stateNo == stateNo) {
                alpha += msgIn->val * fwdSame;
            }
            else {
                alpha += msgIn->val * fwdDiff;
            }
            
            if (fwdRecCnt == NOOFSTATES) {
                
                // Multiply Alpha by Emission Probability
                if (match == 1u) {
                    alpha = alpha * (1.0f - (1.0f / ERRORRATE));
                }
                else {
                    alpha = alpha * (1.0f / ERRORRATE);
                }
                
                // Previous alpha has been calculated
                prevAlpha = alpha;
                rdyFlags |= PREVA;
                
                // If we are an intermediate node propagate the alpha to the next column
                // Else send the alpha out to the host
                if (observationNo != (NOOFTARGMARK - 1u)) {
                    
                    // Get pointers to mailbox message slot
                    volatile ImpMessage* msgOut = tinselSendSlot();
                    
                    tinselWaitUntil(TINSEL_CAN_SEND);
                    msgOut->msgType = FORWARD;
                    msgOut->stateNo = stateNo;
                    msgOut->val = alpha;
                    
                    tinselKeySend(fwdKey, msgOut);
                    
                }
                
                // Propagte alpha to previous thread as next alpha
                
                // Get pointers to mailbox message slot
                volatile ImpMessage* msgOut = tinselSendSlot();
                
                tinselWaitUntil(TINSEL_CAN_SEND);
                msgOut->msgType = FWDLIN;
                msgOut->stateNo = stateNo;
                msgOut->val = alpha;
                
                tinselSend(prevThread, msgOut);
                
                // Send to host
                volatile HostMessage* msgHost = tinselSendSlot();

                tinselWaitUntil(TINSEL_CAN_SEND);
                msgHost->msgType = FORWARD;
                msgHost->observationNo = observationNo * LINRATIO;
                msgHost->stateNo = stateNo;
                msgHost->val = alpha;

                tinselSend(host, msgHost);
            
            }
        
        }
        
        // Handle backward messages
        if (msgIn->msgType == BACKWARD) {
            
            bwdRecCnt++;
            
            float emissionProb = 0.0f;
         
            if (msgIn->match == 1u) {
                emissionProb = (1.0f - (1.0f / ERRORRATE));
            }
            else {
                emissionProb = (1.0f / ERRORRATE);
            }
            
            if (msgIn->stateNo == stateNo) {
                
                beta += msgIn->val * bwdSame * emissionProb;
                
            }
            else {
                
                beta += msgIn->val * bwdDiff * emissionProb;
                
            }
            
            if (bwdRecCnt == NOOFSTATES) {
                
                prevBeta = beta;
                rdyFlags |= PREVB;
                
                if (observationNo != 0u) {
                    
                    // Get pointers to mailbox message slot
                    volatile ImpMessage* msgOut = tinselSendSlot();

                    tinselWaitUntil(TINSEL_CAN_SEND);
                    msgOut->msgType = BACKWARD;
                    msgOut->match = match;
                    msgOut->stateNo = stateNo;
                    msgOut->val = beta;
                    
                    // Propagate to previous column
                    tinselKeySend(bwdKey, msgOut);
                    
                    // Propagate beta to previous thread as prev beta
                    
                    tinselWaitUntil(TINSEL_CAN_SEND);
                    msgOut->msgType = BWDLIN;
                    msgOut->match = match;
                    msgOut->stateNo = stateNo;
                    msgOut->val = beta;
                    
                    // Propagate to previous column
                    tinselSend(prevThread, msgOut);

                }

                volatile HostMessage* msgHost = tinselSendSlot();;

                tinselWaitUntil(TINSEL_CAN_SEND);
                msgHost->msgType = BACKWARD;
                msgHost->observationNo = observationNo * LINRATIO;
                msgHost->stateNo = stateNo;
                msgHost->val = beta;

                tinselSend(host, msgHost);
            
            }
            
        }
        
        // Handle forward messages
        if (msgIn->msgType == FWDLIN) {
            
            nextAlpha = msgIn->val;
            rdyFlags |= NEXTA;
            
        }
        
        // If we have both values for linear interpolation
        if ((rdyFlags & PREVA) && (rdyFlags & NEXTA)) {
            
            float totalDiff = prevAlpha - nextAlpha;
            
            for (uint32_t x = 0u; x < (LINRATIO - 1u); x++) {
                
                //alphaLin[x] = prevAlpha - ((dmLocal[x] / totalDistance) * totalDiff);
                //prevAlpha = alphaLin[x];
                alphaLin[x] = (float)(observationNo + x + 1u);

            }
            
            for (uint32_t x = 0u; x < (LINRATIO - 1u); x++) {
                
                // Send to host
                volatile HostMessage* msgHost = tinselSendSlot();

                tinselWaitUntil(TINSEL_CAN_SEND);
                msgHost->msgType = FWDLIN;
                msgHost->observationNo = (observationNo * LINRATIO) + 1u + x;
                msgHost->stateNo = stateNo;
                msgHost->val = alphaLin[x];

                tinselSend(host, msgHost);
                
            }
            
            //Clear the ready flags to prevent re-transmission
            rdyFlags &= (~PREVA);
            rdyFlags &= (~NEXTA);
            
        }
        
        // Handle backward messages
        if (msgIn->msgType == BWDLIN) {
        
            nextBeta = msgIn->val;
            rdyFlags |= NEXTB;

        }
        
        // If we have received both values for linear interpolation
        if ((rdyFlags & PREVB) && (rdyFlags & NEXTB)) {
            
            float totalDiff = nextBeta - prevBeta;
            
            for (uint32_t x = 0u; x < (LINRATIO - 1u); x++) {
                
                //betaLin[x] = nextBeta - ((dmLocal[(LINRATIO - 1u) - x] / totalDistance) * totalDiff);
                //nextBeta = betaLin[x];
                betaLin[x] = 69.0f;
                
            }
            
            for (uint32_t x = 0u; x < (LINRATIO - 1u); x++) {
                
                // Send to host
                volatile HostMessage* msgHost = tinselSendSlot();

                tinselWaitUntil(TINSEL_CAN_SEND);
                msgHost->msgType = BWDLIN;
                msgHost->observationNo = (observationNo * LINRATIO) + 1u + x;
                msgHost->stateNo = stateNo;
                msgHost->val = betaLin[x];
                //msgHost->val = 00000.00000f;

                tinselSend(host, msgHost);
                
            }
            
            //Clear the ready flags to prevent re-transmission
            rdyFlags &= (~PREVB);
            rdyFlags &= (~NEXTB);
            
        }
        
        // Free message slot
        tinselFree(msgIn);
    
    }
    return 0;
}

