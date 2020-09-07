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
    uint8_t localThreadID = me % (1u << TinselLogThreadsPerMailbox);
    
    // Calculate model parameters
    uint8_t HWRowNo = localThreadID % 8u;

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
    
    float alpha[NOOFSTATEPANELS] = {0.0f};
    float beta[NOOFSTATEPANELS] = {0.0f};
    float alphaLin[NOOFSTATEPANELS][LINRATIO - 1u];
    float betaLin[NOOFSTATEPANELS][LINRATIO - 1u];
    
    float prevAlpha[NOOFSTATEPANELS] = {0.0f};
    float nextAlpha[NOOFSTATEPANELS] = {0.0f};
    float prevBeta[NOOFSTATEPANELS] = {0.0f};
    float nextBeta[NOOFSTATEPANELS] = {0.0f};
    
    uint8_t rdyFlags[NOOFSTATEPANELS] = {0u};
    /*
    // Send to host
    volatile HostMessage* msgHost = tinselSendSlot();
    
    tinselWaitUntil(TINSEL_CAN_SEND);
    msgHost->msgType = FORWARD;
    msgHost->observationNo = observationNo;
    msgHost->stateNo = HWRowNo;
    msgHost->val = localThreadID;

    tinselSend(host, msgHost);
    
    // Send to host
    //volatile HostMessage* msgHost = tinselSendSlot();

    tinselWaitUntil(TINSEL_CAN_SEND);
    msgHost->msgType = BACKWARD;
    msgHost->observationNo = observationNo;
    msgHost->stateNo = HWRowNo;
    msgHost->val = prevThread;

    tinselSend(host, msgHost);
    */
    
    // Startup for forward algorithm
    if (observationNo == 0u) {
        
        // Calculate initial probability
        alpha[0] = 1.0f / NOOFHWROWS;
        
        // Multiply alpha by emission probability
        if (match == 1u) {
            alpha[0] = alpha[0] * (1.0f - (1.0f / ERRORRATE));
        }
        else {
            alpha[0] = alpha[0] * (1.0f / ERRORRATE);
        }
        
        prevAlpha[0] = alpha[0];
        rdyFlags[0] |= PREVA;
        
        // Get pointers to mailbox message slot
        volatile ImpMessage* msgOut = tinselSendSlot();
        
        // Prepare message to send to HMM node
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut->msgType = FORWARD;
        msgOut->stateNo = HWRowNo;
        msgOut->val = alpha[0];
        
        // Propagate to next column
        tinselKeySend(fwdKey, msgOut);
        
        // Send to host
        volatile HostMessage* msgHost = tinselSendSlot();

        tinselWaitUntil(TINSEL_CAN_SEND);
        msgHost->msgType = FORWARD;
        msgHost->observationNo = observationNo * LINRATIO;
        msgHost->stateNo = HWRowNo;
        msgHost->val = alpha[0];

        tinselSend(host, msgHost);
    
    }
    
    // Startup for backward algorithm
    if (observationNo == (NOOFTARGMARK - 1u)) {
      
        // Get pointers to mailbox message slot
        volatile ImpMessage* msgOut = tinselSendSlot();
        
        beta[0] = 1.0f;
        
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut->msgType = BACKWARD;
        msgOut->match = match;
        msgOut->stateNo = HWRowNo;
        msgOut->val = beta[0];
        
        // Propagate to previous column
        tinselKeySend(bwdKey, msgOut);
        
        prevBeta[0] = beta[0];
        rdyFlags[0] |= PREVB;
        
        // Propagate beta to previous thread as prev beta
        
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut->msgType = BWDLIN;
        msgOut->match = match;
        msgOut->stateNo = HWRowNo;
        msgOut->val = beta[0];
        
        // Propagate to previous column
        tinselSend(prevThread, msgOut);

        volatile HostMessage* msgHost = tinselSendSlot();

        tinselWaitUntil(TINSEL_CAN_SEND);
        msgHost->msgType = BACKWARD;
        msgHost->observationNo = observationNo * LINRATIO;
        msgHost->stateNo = HWRowNo;
        msgHost->val = beta[0];

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
            
            if (msgIn->stateNo == HWRowNo) {
                alpha[0] += msgIn->val * fwdSame;
            }
            else {
                alpha[0] += msgIn->val * fwdDiff;
            }
            
            if (fwdRecCnt == NOOFHWROWS) {
                
                // Multiply Alpha by Emission Probability
                if (match == 1u) {
                    alpha[0] = alpha[0] * (1.0f - (1.0f / ERRORRATE));
                }
                else {
                    alpha[0] = alpha[0] * (1.0f / ERRORRATE);
                }
                
                // Previous alpha has been calculated
                prevAlpha[0] = alpha[0];
                rdyFlags[0] |= PREVA;
                
                // If we are an intermediate node propagate the alpha to the next column
                // Else send the alpha out to the host
                if (observationNo != (NOOFTARGMARK - 1u)) {
                    
                    // Get pointers to mailbox message slot
                    volatile ImpMessage* msgOut = tinselSendSlot();
                    
                    tinselWaitUntil(TINSEL_CAN_SEND);
                    msgOut->msgType = FORWARD;
                    msgOut->stateNo = HWRowNo;
                    msgOut->val = alpha[0];
                    
                    tinselKeySend(fwdKey, msgOut);
                    
                }
                
                // Propagte alpha to previous thread as next alpha
                
                // Get pointers to mailbox message slot
                volatile ImpMessage* msgOut = tinselSendSlot();
                
                tinselWaitUntil(TINSEL_CAN_SEND);
                msgOut->msgType = FWDLIN;
                msgOut->stateNo = HWRowNo;
                msgOut->val = alpha[0];
                
                tinselSend(prevThread, msgOut);
                
                // Send to host
                volatile HostMessage* msgHost = tinselSendSlot();

                tinselWaitUntil(TINSEL_CAN_SEND);
                msgHost->msgType = FORWARD;
                msgHost->observationNo = observationNo * LINRATIO;
                msgHost->stateNo = HWRowNo;
                msgHost->val = alpha[0];

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
            
            if (msgIn->stateNo == HWRowNo) {
                
                beta[0] += msgIn->val * bwdSame * emissionProb;
                
            }
            else {
                
                beta[0] += msgIn->val * bwdDiff * emissionProb;
                
            }
            
            if (bwdRecCnt == NOOFHWROWS) {
                
                prevBeta[0] = beta[0];
                rdyFlags[0] |= PREVB;
                
                if (observationNo != 0u) {
                    
                    // Get pointers to mailbox message slot
                    volatile ImpMessage* msgOut = tinselSendSlot();

                    tinselWaitUntil(TINSEL_CAN_SEND);
                    msgOut->msgType = BACKWARD;
                    msgOut->match = match;
                    msgOut->stateNo = HWRowNo;
                    msgOut->val = beta[0];
                    
                    // Propagate to previous column
                    tinselKeySend(bwdKey, msgOut);
                    
                    // Propagate beta to previous thread as prev beta
                    
                    tinselWaitUntil(TINSEL_CAN_SEND);
                    msgOut->msgType = BWDLIN;
                    msgOut->match = match;
                    msgOut->stateNo = HWRowNo;
                    msgOut->val = beta[0];
                    
                    // Propagate to previous column
                    tinselSend(prevThread, msgOut);

                }

                volatile HostMessage* msgHost = tinselSendSlot();;

                tinselWaitUntil(TINSEL_CAN_SEND);
                msgHost->msgType = BACKWARD;
                msgHost->observationNo = observationNo * LINRATIO;
                msgHost->stateNo = HWRowNo;
                msgHost->val = beta[0];

                tinselSend(host, msgHost);
            
            }
            
        }
        
        // Handle forward messages
        if (msgIn->msgType == FWDLIN) {
            
            nextAlpha[0] = msgIn->val;
            rdyFlags[0] |= NEXTA;
            
        }
        
        // If we have both values for linear interpolation
        if ((rdyFlags[0] & PREVA) && (rdyFlags[0] & NEXTA)) {
            
            float totalDiff = prevAlpha[0] - nextAlpha[0];
            
            for (uint32_t x = 0u; x < (LINRATIO - 1u); x++) {
                
                alphaLin[0][x] = prevAlpha[0] - ((dmLocal[x] / totalDistance) * totalDiff);
                alphaLin[0][x] = prevAlpha[0] - ((dmLocal[x] / totalDistance) * totalDiff);
                prevAlpha[0] = alphaLin[0][x];

            }
            
            for (uint32_t x = 0u; x < (LINRATIO - 1u); x++) {
                
                // Send to host
                volatile HostMessage* msgHost = tinselSendSlot();

                tinselWaitUntil(TINSEL_CAN_SEND);
                msgHost->msgType = FWDLIN;
                msgHost->observationNo = (observationNo * LINRATIO) + 1u + x;
                msgHost->stateNo = HWRowNo;
                msgHost->val = alphaLin[0][x];

                tinselSend(host, msgHost);
                
            }
            
            //Clear the ready flags to prevent re-transmission
            rdyFlags[0] &= (~PREVA);
            rdyFlags[0] &= (~NEXTA);
            
        }
        
        // Handle backward messages
        if (msgIn->msgType == BWDLIN) {
        
            nextBeta[0] = msgIn->val;
            rdyFlags[0] |= NEXTB;

        }
        
        // If we have received both values for linear interpolation
        if ((rdyFlags[0] & PREVB) && (rdyFlags[0] & NEXTB)) {
            
            float totalDiff = nextBeta[0] - prevBeta[0];
            
            for (uint32_t x = 0u; x < (LINRATIO - 1u); x++) {
                
                betaLin[0][x] = nextBeta[0] - ((dmLocal[(LINRATIO - 2u) - x] / totalDistance) * totalDiff);
                nextBeta[0] = betaLin[0][x];
                
            }
            
            for (uint32_t x = 0u; x < (LINRATIO - 1u); x++) {
                
                // Send to host
                volatile HostMessage* msgHost = tinselSendSlot();

                tinselWaitUntil(TINSEL_CAN_SEND);
                msgHost->msgType = BWDLIN;
                msgHost->observationNo = (observationNo * LINRATIO) + 1u + x;
                msgHost->stateNo = HWRowNo;
                msgHost->val = betaLin[0][(LINRATIO - 2u) - x];
                //msgHost->val = 00000.00000f;

                tinselSend(host, msgHost);
                
            }
            
            //Clear the ready flags to prevent re-transmission
            rdyFlags[0] &= (~PREVB);
            rdyFlags[0] &= (~NEXTB);
            
        }
        
        // Free message slot
        tinselFree(msgIn);
    
    }
    return 0;
}

