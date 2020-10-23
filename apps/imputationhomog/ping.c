#include "imputation.h"

/*****************************************************
 * Hidden Markov Model and Linear Interpolation Node
 * ***************************************************
 * This code performs the stephens and li model for imputation
 * 
 * USE ARRAYS FOR MATCH / ALPHAS TO PUSH THE PROCESSING TO MESSAGE TIME IN FAVOUR OF POETS
 * ****************************************************/
 
/*****************************************************
* Pre-processor Definitions
* ***************************************************/
 
 // RdyFlags
#define PREVA   (1u << 0u)
#define NEXTA   (1u << 1u)
#define PREVB   (1u << 2u)
#define NEXTB   (1u << 3u)

// Host-bound Messages
#define FLINHQ  (1u << 4u)
#define BLINHQ  (1u << 5u)
#define FWDHQ   (1u << 6u)
#define BWDHQ   (1u << 7u)

// Algorithm Functional Messages
#define FWDQ    (1u << 8u)
#define BWDQ    (1u << 9u)
#define FWDUQ   (1u << 10u)
#define BWDUQ   (1u << 11u)

/*****************************************************
* Main Function
* ***************************************************/

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
    
    // Received values for each panel / leg
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
    
    // Declared and intialised seperately to avoid memset error on compilation
    
    float alpha[NOOFSTATEPANELS][NOOFLEGS];
    float beta[NOOFSTATEPANELS][NOOFLEGS];
    float alphaLin[NOOFSTATEPANELS][NOOFLEGS][LINRATIO - 1u];
    float betaLin[NOOFSTATEPANELS][NOOFLEGS][LINRATIO - 1u];
    
    float prevAlpha[NOOFSTATEPANELS][NOOFLEGS];
    float nextAlpha[NOOFSTATEPANELS][NOOFLEGS];
    float prevBeta[NOOFSTATEPANELS][NOOFLEGS];
    float nextBeta[NOOFSTATEPANELS][NOOFLEGS];
    
    uint16_t rdyFlags[NOOFSTATEPANELS][NOOFLEGS];
    
    for (uint32_t statePanel = 0u; statePanel < NOOFSTATEPANELS; statePanel++) {
        for (uint32_t leg = 0u; leg < NOOFLEGS; leg++) {
            
            alpha[statePanel][leg] = 0.0f;
            beta[statePanel][leg] = 0.0f;
            
            prevAlpha[statePanel][leg] = 0.0f;
            nextAlpha[statePanel][leg] = 0.0f;
            prevBeta[statePanel][leg] = 0.0f;
            nextBeta[statePanel][leg] = 0.0f;
            
            rdyFlags[statePanel][leg] = 0.0f;
            
        }
    }
    
    for (uint32_t statePanel = 0u; statePanel < NOOFSTATEPANELS; statePanel++) {
        for (uint32_t leg = 0u; leg < NOOFLEGS; leg++) {
            for (uint32_t linVal = 0u; linVal < (LINRATIO - 1u); linVal++) {
                
                alphaLin[statePanel][leg][linVal] = 0.0f;
                betaLin[statePanel][leg][linVal] = 0.0f;
                
            }
        }
    }
        
    
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
    
    */
    
    /***************************************************
    * STARTUP FOR FORWARD ALGORITHM
    ****************************************************/
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
            //msgHost->val = HWColNo;

            tinselSend(host, msgHost);
        
        }
        
        // If we are also the first hardware row (Core 0, Thread 0) start the performance counter
        if (HWRowNo == 0u) {
            tinselPerfCountReset();
            tinselPerfCountStart();
        }
    
    }
    
    /***************************************************
    * STARTUP FOR BACKWARD ALGORITHM
    ****************************************************/
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
            //msgHost->val = HWColNo;

            tinselSend(host, msgHost);
            
            prevBeta[y][NOOFLEGS - 1u] = beta[y][NOOFLEGS - 1u];
            rdyFlags[y][NOOFLEGS - 1u] |= PREVB;
            
            tinselWaitUntil(TINSEL_CAN_SEND);
            msgOut->msgType = BWDLIN;
            msgOut->match = match[y][NOOFLEGS - 1u];
            msgOut->leg = NOOFLEGS - 1u;
            msgOut->stateNo = (y * NOOFHWROWS) + HWRowNo;
            msgOut->val = beta[y][NOOFLEGS - 1u];
            
            // Propagate to previous column
            tinselSend(prevThread, msgOut);
        
        }
        
    }
    
        
    uint8_t fwdRecCnt[NOOFLEGS] = {0u};
    uint8_t bwdRecCnt[NOOFLEGS] = {0u};
    
    uint32_t fstStatePanel = 0u;
    uint32_t lstStatePanel = 0u;
    
    uint32_t sendsQueued = 0u;
    
    /***************************************************
    * MAIN NODE HANDLER LOOP
    ****************************************************/
    
    while (1u) {
        
        /***************************************************
        * Check if messages are queued and we can send them
        ****************************************************/
        
        if ((sendsQueued > 0u) && tinselCanSend()) {
            
            for (uint32_t i = 0u; i < NOOFSTATEPANELS; i++) {
                for (uint32_t l = 0u; l < NOOFLEGS; l++) {
                    
                    // CHECK FOR ALGORITHM FUNCTIONAL MESSAGES
                    
                    // Check for forward multicast messages
                    if ((rdyFlags[i][l] & FWDQ) && tinselCanSend()) {
                        
                        // If we are an intermediate node propagate the alpha to the next hardware column
                        if (HWColNo != (NOOFHWCOLS - 1u)) {
                            
                            // Get pointers to mailbox message slot
                            volatile ImpMessage* msgOut = tinselSendSlot();
                            
                            tinselWaitUntil(TINSEL_CAN_SEND);
                            msgOut->msgType = FORWARD;
                            msgOut->leg = l;
                            msgOut->stateNo = (i * NOOFHWROWS) + HWRowNo;
                            msgOut->val = alpha[i][l];
                            
                            tinselKeySend(fwdKey, msgOut);
                            
                        }
                        // If we are the last hardware column AND we are not the final leg, increase the leg and propagate to the first hardware column
                        else if (l != NOOFLEGS - 1u) {
                            
                            // Get pointers to mailbox message slot
                            volatile ImpMessage* msgOut = tinselSendSlot();
                            
                            tinselWaitUntil(TINSEL_CAN_SEND);
                            msgOut->msgType = FORWARD;
                            msgOut->leg = l + 1u;
                            msgOut->stateNo = (i * NOOFHWROWS) + HWRowNo;
                            msgOut->val = alpha[i][l];
                            
                            tinselKeySend(fwdKey, msgOut);
                        }
                        
                        sendsQueued--;
                        
                        rdyFlags[i][l] &= (~FWDQ);
                        
                    }
                    
                    // Check for backward multicast messages
                    if ((rdyFlags[i][l] & BWDQ) && tinselCanSend()) {
                        
                        // If we are not the first hardware column
                        if (HWColNo != 0u) {
                            
                            // Get pointers to mailbox message slot
                            volatile ImpMessage* msgOut = tinselSendSlot();

                            tinselWaitUntil(TINSEL_CAN_SEND);
                            msgOut->msgType = BACKWARD;
                            msgOut->leg = l;
                            msgOut->match = match[i][l];
                            msgOut->stateNo = (i * NOOFHWROWS) + HWRowNo;
                            msgOut->val = beta[i][l];
                            
                            // Propagate to previous column
                            tinselKeySend(bwdKey, msgOut);

                        }
                        // If we are the first hardware column and we are not the first leg
                        else if (l != 0u) {
                            
                            // Get pointers to mailbox message slot
                            volatile ImpMessage* msgOut = tinselSendSlot();

                            tinselWaitUntil(TINSEL_CAN_SEND);
                            msgOut->msgType = BACKWARD;
                            msgOut->leg = l - 1u;
                            msgOut->match = match[i][l];
                            msgOut->stateNo = (i * NOOFHWROWS) + HWRowNo;
                            msgOut->val = beta[i][l];
                            
                            // Propagate to previous column
                            tinselKeySend(bwdKey, msgOut);
                            
                        }
                        
                        sendsQueued--;
                        
                        rdyFlags[i][l] &= (~BWDQ);
                        
                    }
                    
                    // Check for forward unicast messages
                    if ((rdyFlags[i][l] & FWDUQ) && tinselCanSend()) {
                        
                        // If we are not the first hardware column
                        if (HWColNo != 0u) {
                        
                            // Get pointers to mailbox message slot
                            volatile ImpMessage* msgOut = tinselSendSlot();
                            
                            tinselWaitUntil(TINSEL_CAN_SEND);
                            msgOut->msgType = FWDLIN;
                            msgOut->leg = l;
                            msgOut->stateNo = (i * NOOFHWROWS) + HWRowNo;
                            msgOut->val = alpha[i][l];
                            
                            tinselSend(prevThread, msgOut);
                        
                        }
                        // If we are the first hardware column AND we are not the first leg, decrease the leg and propagate to the last hardware column
                        else if (l != 0u) {
                            
                            // Get pointers to mailbox message slot
                            volatile ImpMessage* msgOut = tinselSendSlot();
                            
                            tinselWaitUntil(TINSEL_CAN_SEND);
                            msgOut->msgType = FWDLIN;
                            msgOut->leg = l - 1u;
                            msgOut->stateNo = (i * NOOFHWROWS) + HWRowNo;
                            msgOut->val = alpha[i][l];
                            
                            tinselSend(prevThread, msgOut);
                        }
                        
                        sendsQueued--;
                        
                        rdyFlags[i][l] &= (~FWDUQ);
   
                    }
                    
                    // Check for backward unicast messages
                    if ((rdyFlags[i][l] & BWDUQ) && tinselCanSend()) {
                     
                        // If we are not the first hardware column
                        if (HWColNo != 0u) {
                            
                            // Get pointers to mailbox message slot
                            volatile ImpMessage* msgOut = tinselSendSlot();
                            
                            // Propagate beta to previous thread as prev beta
                            
                            tinselWaitUntil(TINSEL_CAN_SEND);
                            msgOut->msgType = BWDLIN;
                            msgOut->leg = l;
                            msgOut->match = match[i][l];
                            msgOut->stateNo = (i * NOOFHWROWS) + HWRowNo;
                            msgOut->val = beta[i][l];
                            
                            // Propagate to previous column
                            tinselSend(prevThread, msgOut);

                        }
                        // If we are the first hardware column and we are not the first leg
                        else if (l != 0u) {
                            
                            // Get pointers to mailbox message slot
                            volatile ImpMessage* msgOut = tinselSendSlot();
                            
                            // Propagate beta to previous thread as prev beta
                            
                            tinselWaitUntil(TINSEL_CAN_SEND);
                            msgOut->msgType = BWDLIN;
                            msgOut->leg = l - 1u; //JPM DOUBLE CHECK THIS
                            msgOut->match = match[i][l];
                            msgOut->stateNo = (i * NOOFHWROWS) + HWRowNo;
                            msgOut->val = beta[i][l];
                            
                            // Propagate to previous column
                            tinselSend(prevThread, msgOut);
                            
                        }
                        
                        sendsQueued--;
                        
                        rdyFlags[i][l] &= (~BWDUQ);
   
                    }
                    
                    // CHECK FOR HOST-BOUND MESSAGES
                    
                    // Check for forward host messages
                    if ((rdyFlags[i][l] & FWDHQ) && tinselCanSend()) {
                        
                        // Send to host
                        volatile HostMessage* msgHost = tinselSendSlot();

                        tinselWaitUntil(TINSEL_CAN_SEND);
                        msgHost->msgType = FORWARD;
                        msgHost->observationNo = (HWColNo * LINRATIO) + ((l * NOOFHWCOLS) * LINRATIO);
                        msgHost->stateNo = (i * NOOFHWROWS) + HWRowNo;
                        msgHost->val = alpha[i][l];
                        //msgHost->val = HWColNo;

                        tinselSend(host, msgHost);
                        
                        sendsQueued--;
                        
                        rdyFlags[i][l] &= (~FWDHQ);
                        
                    }
                    
                    // Check for backward host messages
                    if ((rdyFlags[i][l] & BWDHQ) && tinselCanSend()) {
                        
                        // Send to host
                        volatile HostMessage* msgHost = tinselSendSlot();

                        tinselWaitUntil(TINSEL_CAN_SEND);
                        msgHost->msgType = BACKWARD;
                        msgHost->observationNo = (HWColNo * LINRATIO) + ((l * NOOFHWCOLS) * LINRATIO);
                        msgHost->stateNo = (i * NOOFHWROWS) + HWRowNo;
                        msgHost->val = beta[i][l];
                        //msgHost->val = HWColNo;

                        tinselSend(host, msgHost);
                        
                        sendsQueued--;
                        
                        rdyFlags[i][l] &= (~BWDHQ);
                        
                    }
                        
                    // Check for forward linear interpolation host messages
                    if ((rdyFlags[i][l] & FLINHQ) && tinselCanSend()) {
                            
                        for (uint32_t x = 0u; x < (LINRATIO - 1u); x++) {
                                
                            // Send to host
                            volatile HostMessage* msgHost = tinselSendSlot();

                            tinselWaitUntil(TINSEL_CAN_SEND);
                            msgHost->msgType = FWDLIN;
                            msgHost->observationNo = ((l * NOOFHWCOLS) * LINRATIO) + (HWColNo * LINRATIO) + 1u + x;
                            msgHost->stateNo = (i * NOOFHWROWS) + HWRowNo;
                            msgHost->val = alphaLin[i][l][x];
                            //msgHost->val = 1u;

                            tinselSend(host, msgHost);
                                
                            sendsQueued--;
                            
                        }
                            
                        rdyFlags[i][l] &= (~FLINHQ);
                        
                    }
                    
                    // Check for backward linear interpolation host messages
                    if ((rdyFlags[i][l] & BLINHQ) && tinselCanSend()) {
                            
                        for (uint32_t x = 0u; x < (LINRATIO - 1u); x++) {
                            
                            // Send to host
                            volatile HostMessage* msgHost = tinselSendSlot();

                            tinselWaitUntil(TINSEL_CAN_SEND);
                            msgHost->msgType = BWDLIN;
                            msgHost->observationNo = ((l * NOOFHWCOLS) * LINRATIO) + (HWColNo * LINRATIO) + 1u + x;
                            msgHost->stateNo = (i * NOOFHWROWS) + HWRowNo;
                            msgHost->val = betaLin[i][l][(LINRATIO - 2u) - x];
                            //msgHost->val = 1u;

                            tinselSend(host, msgHost);
                            
                            sendsQueued--;
                        
                        }
                        
                        rdyFlags[i][l] &= (~BLINHQ);
                        
                    }
                    
                    
                }
            }
            
            
            
        }
        
        /***************************************************
        * Check if there are messages to be received
        ****************************************************/

        if (tinselCanRecv()) {
            
            volatile ImpMessage* msgIn = tinselRecv();
            
            uint32_t index = (uint32_t)(msgIn->stateNo / NOOFHWROWS);
            
            // Clear backward linear interpolation check and state panel selection
            fstStatePanel = 0u;
            lstStatePanel = 0u;
            
            
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
                        
                        // Handle forward multicast message send
                        if (tinselCanSend()) {
                            
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
                            
                        }
                        else {
                            
                            // Set flags for forward multicast message
                            rdyFlags[y][msgIn->leg] |= FWDQ;
                            sendsQueued++;
                            
                        }
                        
                        // Handle forward unicast message send
                        if (tinselCanSend()) {
                            
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
                            
                        }
                        else {
                            
                            // Set flags for forward unicast message
                            rdyFlags[y][msgIn->leg] |= FWDUQ;
                            sendsQueued++; 
                        
                        }

                        // Handle forward hostbound message send
                        if (tinselCanSend()) {
                        
                            // Send to host
                            volatile HostMessage* msgHost = tinselSendSlot();

                            tinselWaitUntil(TINSEL_CAN_SEND);
                            msgHost->msgType = FORWARD;
                            msgHost->observationNo = (HWColNo * LINRATIO) + ((msgIn->leg * NOOFHWCOLS) * LINRATIO);
                            msgHost->stateNo = (y * NOOFHWROWS) + HWRowNo;
                            msgHost->val = alpha[y][msgIn->leg];
                            //msgHost->val = HWColNo;

                            tinselSend(host, msgHost);
                        
                        }
                        else {
                            
                            // Set flags for forward host message
                            rdyFlags[y][msgIn->leg] |= FWDHQ;
                            sendsQueued++;
                            
                        }
                        
                        
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
                        
                        // Set backward linear interpolation check
                        fstStatePanel = 0u;
                        lstStatePanel = NOOFSTATEPANELS;
                        
                        // Handle backward multicast message send
                        if (tinselCanSend()) {
                            
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
                                
                            }
                            
                        }
                        else {
                            
                            // Set flags for backward message
                            rdyFlags[y][msgIn->leg] |= BWDQ;
                            sendsQueued++;
                            
                        }
                        
                        // Handle backward unicast message send
                        if (tinselCanSend()) {
                            
                            // If we are not the first hardware column
                            if (HWColNo != 0u) {
                                
                                // Get pointers to mailbox message slot
                                volatile ImpMessage* msgOut = tinselSendSlot();
                                
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
                            
                        }
                        else {
                            
                            // Set flags for backward message
                            rdyFlags[y][msgIn->leg] |= BWDUQ;
                            sendsQueued++;
                        
                        }
                        
                        // Handle backward host message send
                        if (tinselCanSend()) {
                            
                            // Send to host
                            volatile HostMessage* msgHost = tinselSendSlot();

                            tinselWaitUntil(TINSEL_CAN_SEND);
                            msgHost->msgType = BACKWARD;
                            msgHost->observationNo = (HWColNo * LINRATIO) + ((msgIn->leg * NOOFHWCOLS) * LINRATIO);
                            msgHost->stateNo = (y * NOOFHWROWS) + HWRowNo;
                            msgHost->val = beta[y][msgIn->leg];
                            //msgHost->val = HWColNo;

                            tinselSend(host, msgHost);
                            
                        }
                        else {
                            
                            // Set flags for backward host message
                            rdyFlags[y][msgIn->leg] |= BWDHQ;
                            sendsQueued++;
                            
                        }
                        
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
                
                // Handle forward linear interpolation host message send
                if (tinselCanSend()) {
                
                    for (uint32_t x = 0u; x < (LINRATIO - 1u); x++) {
                            
                        // Send to host
                        volatile HostMessage* msgHost = tinselSendSlot();

                        tinselWaitUntil(TINSEL_CAN_SEND);
                        msgHost->msgType = FWDLIN;
                        msgHost->observationNo = ((msgIn->leg * NOOFHWCOLS) * LINRATIO) + (HWColNo * LINRATIO) + 1u + x;
                        msgHost->stateNo = (index * NOOFHWROWS) + HWRowNo;
                        msgHost->val = alphaLin[index][msgIn->leg][x];
                        //msgHost->val = 1u;

                        tinselSend(host, msgHost);
                        
                    }
                
                }
                else {
                    
                    // Set flags to transmit forward linear interpolation results back to the host
                    rdyFlags[index][msgIn->leg] |= FLINHQ;
                    sendsQueued += (LINRATIO - 1u);
                    
                }
                
                
                // Clear the ready flags to prevent re-transmission
                rdyFlags[index][msgIn->leg] &= (~PREVA);
                rdyFlags[index][msgIn->leg] &= (~NEXTA);
                
            }
            
            // Handle backward messages
            if (msgIn->msgType == BWDLIN) {
            
                nextBeta[index][msgIn->leg] = msgIn->val;
                rdyFlags[index][msgIn->leg] |= NEXTB;
                
                // Set backward linear interpolation check
                fstStatePanel = index;
                lstStatePanel = index + 1u;

            }
            
            
            for (uint32_t y = fstStatePanel; y < lstStatePanel; y++) {
                
                // If we have received both values for linear interpolation
                if ((rdyFlags[y][msgIn->leg] & PREVB) && (rdyFlags[y][msgIn->leg] & NEXTB)) {
                    
                    float totalDiff = nextBeta[y][msgIn->leg] - prevBeta[y][msgIn->leg];
                    
                    for (uint32_t x = 0u; x < (LINRATIO - 1u); x++) {
                        
                        betaLin[y][msgIn->leg][x] = nextBeta[y][msgIn->leg] - ((dmLocal[(LINRATIO - 2u) - x][msgIn->leg] / totalDistance[msgIn->leg]) * totalDiff);
                        nextBeta[y][msgIn->leg] = betaLin[y][msgIn->leg][x];
                        
                    }
                    
                    // Handle backward linear interpolation host message send
                    if (tinselCanSend()) {
                        
                        for (uint32_t x = 0u; x < (LINRATIO - 1u); x++) {
                            
                            // Send to host
                            volatile HostMessage* msgHost = tinselSendSlot();

                            tinselWaitUntil(TINSEL_CAN_SEND);
                            msgHost->msgType = BWDLIN;
                            msgHost->observationNo = ((msgIn->leg * NOOFHWCOLS) * LINRATIO) + (HWColNo * LINRATIO) + 1u + x;
                            msgHost->stateNo = (y * NOOFHWROWS) + HWRowNo;
                            msgHost->val = betaLin[y][msgIn->leg][(LINRATIO - 2u) - x];
                            //msgHost->val = 1u;

                            tinselSend(host, msgHost);
                        
                        }
                        
                    }
                    else {
                        
                        // Set flags to transmit backward linear interpolation results back to the host    
                        rdyFlags[y][msgIn->leg] |= BLINHQ;
                        sendsQueued += (LINRATIO - 1u);
                    
                    }
                    
                    
                    // Can we stop the performance counter?
                    if ( (HWColNo == 0u) && (HWRowNo == 0u) && (msgIn->leg == (NOOFLEGS - 1u)) && (y == (NOOFSTATEPANELS - 1u)) ) {
                        uint32_t countLower = tinselCycleCount();
                        uint32_t countUpper = tinselCycleCountU();
                        
                        // Send to host
                        volatile HostMessage* msgHost = tinselSendSlot();

                        tinselWaitUntil(TINSEL_CAN_SEND);
                        msgHost->msgType = COUNTS;
                        msgHost->observationNo = countLower;
                        msgHost->stateNo = countUpper;

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
    
    }
    return 0;
}

