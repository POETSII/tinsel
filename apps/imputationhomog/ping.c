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

    // Received values for hardware layout
    uint32_t* baseAddress = tinselHeapBase();
    
    // Received values for hardware layout
    // -------------------------------------------->
    uint32_t fwdKey = *baseAddress;
    uint32_t bwdKey = *(baseAddress + 1u);
    uint32_t prevThread = *(baseAddress + 2u);
    uint32_t HWColNo = *(baseAddress + 3u);
    // <-------------------------------------------
    
    // Received values for each leg
    // -------------------------------------------->
    
    float fwdSame[NOOFLEGS];
    float fwdDiff[NOOFLEGS];
    float bwdSame[NOOFLEGS];
    float bwdDiff[NOOFLEGS];
    
    
    uint16_t match[NOOFSTATEPANELS][NOOFLEGS];
    
    // Populate local genetic distances and calculate total genetic distance
    float dmLocal[LINRATIO][NOOFLEGS];
    float totalDistance[NOOFLEGS];
    
    for (uint32_t x = 0u; x < NOOFLEGS; x++) {
        
        uint32_t legOffset = 4u + (x * (4u + NOOFSTATEPANELS + LINRATIO));
    
        fwdSame[x] = *(float*)(baseAddress + legOffset);
        fwdDiff[x] = *(float*)(baseAddress + legOffset + 1u);
        bwdSame[x] = *(float*)(baseAddress + legOffset + 2u);
        bwdDiff[x] = *(float*)(baseAddress + legOffset + 3u);
        
        for (uint32_t y = 0u; y < NOOFSTATEPANELS; y++) {
        
            match[y][x] = (uint16_t)*(baseAddress + legOffset + 4u + y);
        
        }
        
        totalDistance[x] = 0.0f;
        
        for (uint32_t y = 0u; y < LINRATIO; y++) {
            
            dmLocal[y][x] = *(float*)(baseAddress + legOffset + 4u + NOOFSTATEPANELS + y);
            totalDistance[x] += dmLocal[y][x];
            
        }
    
    }
    // <-------------------------------------------
    
    // Get host id
    int host = tinselHostId();
    
    float alpha[NOOFSTATEPANELS][NOOFLEGS] = {{0.0f}};
    float beta[NOOFSTATEPANELS][NOOFLEGS] = {{0.0f}};
    float alphaLin[NOOFSTATEPANELS][NOOFLEGS][LINRATIO - 1u];
    float betaLin[NOOFSTATEPANELS][NOOFLEGS][LINRATIO - 1u];
    
    float prevAlpha[NOOFSTATEPANELS][NOOFLEGS] = {{0.0f}};
    float nextAlpha[NOOFSTATEPANELS][NOOFLEGS] = {{0.0f}};
    float prevBeta[NOOFSTATEPANELS][NOOFLEGS] = {{0.0f}};
    float nextBeta[NOOFSTATEPANELS][NOOFLEGS] = {{0.0f}};
    
    uint8_t rdyFlags[NOOFSTATEPANELS][NOOFLEGS] = {{0.0f}};
    
    /*
    if (HWColNo == 1u) {
        
        // Send to host
        volatile HostMessage* msgHost = tinselSendSlot();
        
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgHost->msgType = FORWARD;
        msgHost->observationNo = HWColNo;
        msgHost->stateNo = HWRowNo;
        msgHost->val = totalDistance[0];

        tinselSend(host, msgHost);
    
    }
    
    
    
    START HERE TO ADD THE NOOFLEGS DIMENSION INTO THE CODE (ALSO CHECK ABOVE IS CORRECT)
    */
    // Startup for forward algorithm
    if (HWColNo == 0u) {
        
        for (uint32_t y = 0u; y < NOOFSTATEPANELS; y++) {
            
            // Calculate initial probability
            alpha[y][0] = 1.0f / NOOFSTATES;
            
            // Multiply alpha by emission probability
            if (match[y][0] == 1u) {
                alpha[y][0] = alpha[y][0] * (1.0f - (1.0f / ERRORRATE));
            }
            else {
                alpha[y][0] = alpha[y][0] * (1.0f / ERRORRATE);
            }
            
            prevAlpha[y][0] = alpha[y][0];
            rdyFlags[y][0] |= PREVA;
            
            // Get pointers to mailbox message slot
            volatile ImpMessage* msgOut = tinselSendSlot();
            
            // Prepare message to send to HMM node
            tinselWaitUntil(TINSEL_CAN_SEND);
            msgOut->msgType = FORWARD;
            msgOut->leg = 0u;
            msgOut->stateNo = (y * NOOFHWROWS) + HWRowNo;
            msgOut->val = alpha[y][0];
            
            // Propagate to next column
            tinselKeySend(fwdKey, msgOut);
            
            // Send to host
            volatile HostMessage* msgHost = tinselSendSlot();

            tinselWaitUntil(TINSEL_CAN_SEND);
            msgHost->msgType = FORWARD;
            msgHost->observationNo = 0u;
            msgHost->stateNo = (y * NOOFHWROWS) + HWRowNo;
            msgHost->val = alpha[y][0];

            tinselSend(host, msgHost);
        
        }
    
    }
    
    // Startup for backward algorithm
    if (HWColNo == (NOOFHWCOLS - 1u)) {
        
        for (uint32_t y = 0u; y < NOOFSTATEPANELS; y++) {
      
            // Get pointers to mailbox message slot
            volatile ImpMessage* msgOut = tinselSendSlot();
            
            beta[y][NOOFLEGS - 1u] = 1.0f;
            
            tinselWaitUntil(TINSEL_CAN_SEND);
            msgOut->msgType = BACKWARD;
            msgOut->match = match[y][NOOFLEGS - 1u];
            msgOut->leg = NOOFLEGS - 1u;
            msgOut->stateNo = (y * NOOFHWROWS) + HWRowNo;
            msgOut->val = beta[y][NOOFLEGS - 1u];
            
            // Propagate to previous column
            tinselKeySend(bwdKey, msgOut);
            
            volatile HostMessage* msgHost = tinselSendSlot();

            tinselWaitUntil(TINSEL_CAN_SEND);
            msgHost->msgType = BACKWARD;
            msgHost->observationNo = NOOFOBS - 1u;
            msgHost->stateNo = (y * NOOFHWROWS) + HWRowNo;
            msgHost->val = beta[y][NOOFLEGS - 1u];

            tinselSend(host, msgHost);
            
            prevBeta[y][NOOFLEGS - 1u] = beta[y][NOOFLEGS - 1u];
            rdyFlags[y][NOOFLEGS - 1u] |= PREVB;
            
            // Propagate beta to previous thread as prev beta
            
            tinselWaitUntil(TINSEL_CAN_SEND);
            msgOut->msgType = BWDLIN;
            msgOut->match = match[y][NOOFLEGS - 1u];
            msgOut->stateNo = (y * NOOFHWROWS) + HWRowNo;
            msgOut->val = beta[y][NOOFLEGS - 1u];
            
            // Propagate to previous column
            tinselSend(prevThread, msgOut);
        
        }
        
    }
    
        
    uint8_t fwdRecCnt[NOOFLEGS] = {0u};
    uint8_t bwdRecCnt[NOOFLEGS] = {0u};
    
    while (1u) {
        tinselWaitUntil(TINSEL_CAN_RECV);
        volatile ImpMessage* msgIn = tinselRecv();
        
        uint32_t index = (uint32_t)(msgIn->stateNo / NOOFHWROWS);
        
        // Handle forward messages
        if (msgIn->msgType == FORWARD) {
            
            fwdRecCnt[msgIn->leg]++;
            
            for (uint32_t y = 0u; y < NOOFSTATEPANELS; y++) {
            
                if (msgIn->stateNo == ((y * NOOFHWROWS) + HWRowNo)) {
                    alpha[y][msgIn->leg] += msgIn->val * fwdSame[msgIn->leg];
                }
                else {
                    alpha[y][msgIn->leg] += msgIn->val * fwdDiff[msgIn->leg];
                }
                
                if (fwdRecCnt[msgIn->leg] == NOOFSTATES) {
                    
                    // Multiply Alpha by Emission Probability
                    if (match[y][msgIn->leg] == 1u) {
                        alpha[y][msgIn->leg] = alpha[y][msgIn->leg] * (1.0f - (1.0f / ERRORRATE));
                    }
                    else {
                        alpha[y][msgIn->leg] = alpha[y][msgIn->leg] * (1.0f / ERRORRATE);
                    }
                    
                    // Previous alpha has been calculated
                    prevAlpha[y][msgIn->leg] = alpha[y][msgIn->leg];
                    rdyFlags[y][msgIn->leg] |= PREVA;
                    
                    // If we are an intermediate node propagate the alpha to the next hardware column
                    if (HWColNo != (NOOFHWCOLS - 1u)) {
                        
                        // Get pointers to mailbox message slot
                        volatile ImpMessage* msgOut = tinselSendSlot();
                        
                        tinselWaitUntil(TINSEL_CAN_SEND);
                        msgOut->msgType = FORWARD;
                        msgOut->leg = msgIn->leg;
                        msgOut->stateNo = (y * NOOFHWROWS) + HWRowNo;
                        msgOut->val = alpha[y][msgIn->leg];
                        
                        tinselKeySend(fwdKey, msgOut);
                        
                    }
                    // If we are the last hardware column AND we are not the final leg, increase the leg and propagate to the first hardware column
                    else if (msgIn->leg != NOOFLEGS - 1u) {
                        
                        // Get pointers to mailbox message slot
                        volatile ImpMessage* msgOut = tinselSendSlot();
                        
                        tinselWaitUntil(TINSEL_CAN_SEND);
                        msgOut->msgType = FORWARD;
                        msgOut->leg = msgIn->leg + 1u;
                        msgOut->stateNo = (y * NOOFHWROWS) + HWRowNo;
                        msgOut->val = alpha[y][msgIn->leg];
                        
                        tinselKeySend(fwdKey, msgOut);
                    }
                    
                    // Propagte alpha to previous thread as next alpha
                    
                    // If we are not the first hardware column
                    if (HWColNo != 0u) {
                    
                        // Get pointers to mailbox message slot
                        volatile ImpMessage* msgOut = tinselSendSlot();
                        
                        tinselWaitUntil(TINSEL_CAN_SEND);
                        msgOut->msgType = FWDLIN;
                        msgOut->leg = msgIn->leg;
                        msgOut->stateNo = (y * NOOFHWROWS) + HWRowNo;
                        msgOut->val = alpha[y][msgIn->leg];
                        
                        tinselSend(prevThread, msgOut);
                    
                    }
                    // If we are the first hardware column AND we are not the first leg, decrease the leg and propagate to the last hardware column
                    else if (msgIn->leg != 0u) {
                        
                        // Get pointers to mailbox message slot
                        volatile ImpMessage* msgOut = tinselSendSlot();
                        
                        tinselWaitUntil(TINSEL_CAN_SEND);
                        msgOut->msgType = FWDLIN;
                        msgOut->leg = msgIn->leg - 1u;
                        msgOut->stateNo = (y * NOOFHWROWS) + HWRowNo;
                        msgOut->val = alpha[y][msgIn->leg];
                        
                        tinselSend(prevThread, msgOut);
                    }
                    
                    // Send to host
                    volatile HostMessage* msgHost = tinselSendSlot();

                    tinselWaitUntil(TINSEL_CAN_SEND);
                    msgHost->msgType = FORWARD;
                    msgHost->observationNo = (HWColNo * LINRATIO) + ((msgIn->leg * NOOFHWCOLS) * LINRATIO);
                    msgHost->stateNo = (y * NOOFHWROWS) + HWRowNo;
                    msgHost->val = alpha[y][msgIn->leg];

                    tinselSend(host, msgHost);
                    
                    // If we are the last state panel clear the counter
                    if (y == NOOFSTATEPANELS - 1u) {
                        fwdRecCnt[msgIn->leg] = 0u;
                    }
                
                }
            
            }
        
        }
        
        // Handle backward messages
        if (msgIn->msgType == BACKWARD) {
            
            bwdRecCnt[msgIn->leg]++;
            
            float emissionProb = 0.0f;
         
            if (msgIn->match == 1u) {
                emissionProb = (1.0f - (1.0f / ERRORRATE));
            }
            else {
                emissionProb = (1.0f / ERRORRATE);
            }
            
            for (uint32_t y = 0u; y < NOOFSTATEPANELS; y++) {
            
                if (msgIn->stateNo == ((y * NOOFHWROWS) + HWRowNo)) {
                    
                    beta[y][msgIn->leg] += msgIn->val * bwdSame[msgIn->leg] * emissionProb;
                    
                }
                else {
                    
                    beta[y][msgIn->leg] += msgIn->val * bwdDiff[msgIn->leg] * emissionProb;
                    
                }
                
                if (bwdRecCnt[msgIn->leg] == NOOFSTATES) {
                    
                    prevBeta[y][msgIn->leg] = beta[y][msgIn->leg];
                    rdyFlags[y][msgIn->leg] |= PREVB;
                    
                    // If we are not the first hardware column
                    if (HWColNo != 0u) {
                        
                        // Get pointers to mailbox message slot
                        volatile ImpMessage* msgOut = tinselSendSlot();

                        tinselWaitUntil(TINSEL_CAN_SEND);
                        msgOut->msgType = BACKWARD;
                        msgOut->leg = msgIn->leg;
                        msgOut->match = match[y][msgIn->leg];
                        msgOut->stateNo = (y * NOOFHWROWS) + HWRowNo;
                        msgOut->val = beta[y][msgIn->leg];
                        
                        // Propagate to previous column
                        tinselKeySend(bwdKey, msgOut);
                        
                        // Propagate beta to previous thread as prev beta
                        
                        tinselWaitUntil(TINSEL_CAN_SEND);
                        msgOut->msgType = BWDLIN;
                        msgOut->leg = msgIn->leg;
                        msgOut->match = match[y][msgIn->leg];
                        msgOut->stateNo = (y * NOOFHWROWS) + HWRowNo;
                        msgOut->val = beta[y][msgIn->leg];
                        
                        // Propagate to previous column
                        tinselSend(prevThread, msgOut);

                    }
                    // If we are the first hardware column and we are not the first leg
                    else if (msgIn->leg != 0u) {
                        
                        // Get pointers to mailbox message slot
                        volatile ImpMessage* msgOut = tinselSendSlot();

                        tinselWaitUntil(TINSEL_CAN_SEND);
                        msgOut->msgType = BACKWARD;
                        msgOut->leg = msgIn->leg - 1u;
                        msgOut->match = match[y][msgIn->leg];
                        msgOut->stateNo = (y * NOOFHWROWS) + HWRowNo;
                        msgOut->val = beta[y][msgIn->leg];
                        
                        // Propagate to previous column
                        tinselKeySend(bwdKey, msgOut);
                        
                        // Propagate beta to previous thread as prev beta
                        
                        tinselWaitUntil(TINSEL_CAN_SEND);
                        msgOut->msgType = BWDLIN;
                        msgOut->leg = msgIn->leg - 1u; //JPM DOUBLE CHECK THIS
                        msgOut->match = match[y][msgIn->leg];
                        msgOut->stateNo = (y * NOOFHWROWS) + HWRowNo;
                        msgOut->val = beta[y][msgIn->leg];
                        
                        // Propagate to previous column
                        tinselSend(prevThread, msgOut);
                        
                    }

                    volatile HostMessage* msgHost = tinselSendSlot();

                    tinselWaitUntil(TINSEL_CAN_SEND);
                    msgHost->msgType = BACKWARD;
                    msgHost->observationNo = (HWColNo * LINRATIO) + ((msgIn->leg * NOOFHWCOLS) * LINRATIO);
                    msgHost->stateNo = (y * NOOFHWROWS) + HWRowNo;
                    msgHost->val = beta[y][msgIn->leg];

                    tinselSend(host, msgHost);
                    
                    // If we are the last state panel clear the counter
                    if (y == NOOFSTATEPANELS - 1u) {
                        //Clear Counter
                        bwdRecCnt[msgIn->leg] = 0u;
                    }
                
                }
            
            }
        }
        
        // Handle forward messages
        if (msgIn->msgType == FWDLIN) {
            
            nextAlpha[index][msgIn->leg] = msgIn->val;
            rdyFlags[index][msgIn->leg] |= NEXTA;
            
        }
        
        // If we have both values for linear interpolation
        if ((rdyFlags[index][msgIn->leg] & PREVA) && (rdyFlags[index][msgIn->leg] & NEXTA)) {
            
            float totalDiff = prevAlpha[index][msgIn->leg] - nextAlpha[index][msgIn->leg];
            
            for (uint32_t x = 0u; x < (LINRATIO - 1u); x++) {
                
                alphaLin[index][msgIn->leg][x] = prevAlpha[index][msgIn->leg] - ((dmLocal[x][msgIn->leg] / totalDistance[msgIn->leg]) * totalDiff);
                prevAlpha[index][msgIn->leg] = alphaLin[index][msgIn->leg][x];

            }
            
            for (uint32_t x = 0u; x < (LINRATIO - 1u); x++) {
                
                // Send to host
                volatile HostMessage* msgHost = tinselSendSlot();

                tinselWaitUntil(TINSEL_CAN_SEND);
                msgHost->msgType = FWDLIN;
                msgHost->observationNo = ((msgIn->leg * NOOFHWCOLS) * LINRATIO) + (HWColNo * LINRATIO) + 1u + x;
                msgHost->stateNo = (index * NOOFHWROWS) + HWRowNo;
                msgHost->val = alphaLin[index][msgIn->leg][x];

                tinselSend(host, msgHost);
                
            }
            
            // Clear the ready flags to prevent re-transmission
            rdyFlags[index][msgIn->leg] &= (~PREVA);
            rdyFlags[index][msgIn->leg] &= (~NEXTA);
            
        }
        
        // Handle backward messages
        if (msgIn->msgType == BWDLIN) {
        
            nextBeta[index][msgIn->leg] = msgIn->val;
            rdyFlags[index][msgIn->leg] |= NEXTB;

        }
        
        
        for (uint32_t y = 0u; y < NOOFSTATEPANELS; y++) {
            
            // JPM ADD CHECK WHETHER MSGTYPE = BACKWARD TO PREVENT MULTIPLE CHECKS THROUGH THIS CODE
            // If we have received both values for linear interpolation
            if ((rdyFlags[y][msgIn->leg] & PREVB) && (rdyFlags[y][msgIn->leg] & NEXTB)) {
                
                float totalDiff = nextBeta[y][msgIn->leg] - prevBeta[y][msgIn->leg];
                
                for (uint32_t x = 0u; x < (LINRATIO - 1u); x++) {
                    
                    betaLin[y][msgIn->leg][x] = nextBeta[y][msgIn->leg] - ((dmLocal[(LINRATIO - 2u) - x][msgIn->leg] / totalDistance[msgIn->leg]) * totalDiff);
                    nextBeta[y][msgIn->leg] = betaLin[y][msgIn->leg][x];
                    
                }
                
                for (uint32_t x = 0u; x < (LINRATIO - 1u); x++) {
                    
                    // Send to host
                    volatile HostMessage* msgHost = tinselSendSlot();

                    tinselWaitUntil(TINSEL_CAN_SEND);
                    msgHost->msgType = BWDLIN;
                    msgHost->observationNo = ((msgIn->leg * NOOFHWCOLS) * LINRATIO) + (HWColNo * LINRATIO) + 1u + x;
                    msgHost->stateNo = (y * NOOFHWROWS) + HWRowNo;
                    msgHost->val = betaLin[y][msgIn->leg][(LINRATIO - 2u) - x];

                    tinselSend(host, msgHost);
                    
                }
                
                // Clear the ready flags to prevent re-transmission
                rdyFlags[y][msgIn->leg] &= (~PREVB);
                rdyFlags[y][msgIn->leg] &= (~NEXTB);
                
            }
        
        }
        
        // Free message slot
        tinselFree(msgIn);
    
    }
    return 0;
}

