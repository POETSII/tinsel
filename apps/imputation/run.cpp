// SPDX-License-Identifier: BSD-2-Clause
#define TINSEL (1u)

#include "model.h"
#include <HostLink.h>
#include <math.h>
#include "../../include/POLite/ProgRouters.h"

/*****************************************************
 * Genomic Imputation - Tinsel Version
 * ***************************************************
 * This code streams target haplotypes from the X86 side
 * USAGE:
 * To Be Completed ...
 * 
 * PLEASE NOTE:
 * To Be Completed ...
 * 
 * ssh jordmorr@fielding.cl.cam.ac.uk
 * scp -r C:\Users\drjor\Documents\tinsel\apps\imputation jordmorr@fielding.cl.cam.ac.uk:~/tinsel/apps
 * scp jordmorr@fielding.cl.cam.ac.uk:~/tinsel/apps/POLite/imputation/results.csv C:\Users\drjor\Documents\tinsel\apps\imputation
 * ****************************************************/
 
//#define PRINTDEBUG (1u) 
 
const uint8_t XKEYS = TinselMeshXLenWithinBox * TinselMailboxMeshXLen;
const uint8_t YKEYS = TinselMeshYLenWithinBox;
const uint8_t ROWSPERCORE = 2u;

int main()
{

    uint8_t boardX = 0u;
    uint8_t boardY = 0u;
    uint8_t mailboxX = 0u;
    uint8_t mailboxY = 0u;
    uint8_t coreID = 0u;
    uint8_t mailboxLocalThreadID = 0u;
    uint32_t threadID = 0u;
    
    HostLink hostLink;
    
    ProgRouterMesh progRouterMesh(TinselMeshXLenWithinBox, TinselMeshYLenWithinBox);
  
    for (boardY = 0u; boardY < TinselMeshYLenWithinBox; boardY++) {
        for (boardX = 0u; boardX < TinselMeshXLenWithinBox; boardX++) {
            for (coreID = 0u; coreID < TinselCoresPerBoard; coreID++) {
               
                // Write HMM into lower two cores in tile
                // This may be reduced to only two writes given the instruction memory sharing. Needs Investigating JPM.
                
                // Core 0 in tile
                hostLink.loadInstrsOntoCore("code_0.v", boardX, boardY, coreID);
                hostLink.loadDataViaCore("data_0.v", boardX, boardY, coreID);
                
                coreID++;
                
                // Core 1 shares instruction memory with core 0 and therefore does not require writing
                
                coreID++;
                
                // Write Linear Interpolation into upper two cores in tile

                // Core 2 in tile
                hostLink.loadInstrsOntoCore("code_1.v", boardX, boardY, coreID);
                hostLink.loadDataViaCore("data_1.v", boardX, boardY, coreID);
                
                coreID++;
                
                // Core 3 shares instruction memory with core 2 and therefore does not require writing
                // Final increment in coreID provided by for loop
 
            }
        }
    }
    
    // Generate and transmit pre-execution data
    
    

    // Generate multicast keys
    
    // Array to store keys
    
    uint32_t fwdColumnKey[XKEYS][YKEYS][ROWSPERCORE] = {0u};
    uint32_t bwdColumnKey[XKEYS][YKEYS][ROWSPERCORE] = {0u};
    
    PRoutingDest dest;
    PRoutingDestMRM mrmRecord;
    Seq<PRoutingDest> fwdDestsRow0;
    Seq<PRoutingDest> fwdDestsRow1;
    Seq<PRoutingDest> bwdDestsRow0;
    Seq<PRoutingDest> bwdDestsRow1;
    
    dest.kind = PRDestKindMRM;
    
    mrmRecord.key = 0u;
    
    const uint32_t row0 = 0x00FF00FFu;
    const uint32_t row1 = 0xFF00FF00u;
    uint8_t destBoardY = 0u;
    
#ifdef PRINTDEBUG    
    printf("\nSample threads for lower source Y board intended to go to the second row:\n\n");
#endif    

    // Iterate over source boards
    for (boardX = 0u; boardX < TinselMeshXLenWithinBox; boardX++) {
        for (mailboxX = 0u; mailboxX < TinselMailboxMeshXLen; mailboxX++) {
            for (boardY = 0u; boardY < TinselMeshYLenWithinBox; boardY++) {
                for (destBoardY = 0u; destBoardY < TinselMeshYLenWithinBox; destBoardY++) {
                    for (mailboxY = 0u; mailboxY < TinselMailboxMeshYLen; mailboxY++) {
                              
                        // FORWARD KEYS
                        // Construct destination the mailbox
                        dest.mbox = destBoardY;
                        dest.mbox = (dest.mbox << TinselMeshXBits) + boardX;
                        dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxY;
                        dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + mailboxX;
                        
                        // Construct destination threads for row 1
                        mrmRecord.threadMaskLow = row1;
                        mrmRecord.threadMaskHigh = 0u;
                        dest.mrm = mrmRecord;
                        
                        fwdDestsRow1.append(dest);   
                    
                        // Do not include last row
                        if (!((mailboxX == TinselMailboxMeshXLen - 1u) && (boardX == TinselMeshXLenWithinBox - 1u))) {                            
                            
                            // Construct destination the mailbox
                            
                            // If the mailbox is the last in the x-axis for the current board
                            if (mailboxX == TinselMailboxMeshXLen - 1u) {
                                dest.mbox = destBoardY;
                                dest.mbox = (dest.mbox << TinselMeshXBits) + boardX + 1u;
                                dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxY;
                                dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + 0u;
                            }
                            else {
                                dest.mbox = destBoardY;
                                dest.mbox = (dest.mbox << TinselMeshXBits) + boardX;
                                dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxY;
                                dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + mailboxX + 1u;                                   
                            }
                            
                            // Construct destination threads for row 0
                            mrmRecord.threadMaskLow = row0;
                            mrmRecord.threadMaskHigh = 0u;
                            dest.mrm = mrmRecord;
                            
                            // Append this information into the destination sequence
                            fwdDestsRow0.append(dest);
                        
                        }
                        
                        // BACKWARD KEYS
                        // Construct destination the mailbox
                        dest.mbox = destBoardY;
                        dest.mbox = (dest.mbox << TinselMeshXBits) + boardX;
                        dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxY;
                        dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + mailboxX;
                        
                        // Construct destination threads for row 0
                        mrmRecord.threadMaskLow = row0;
                        mrmRecord.threadMaskHigh = 0u;
                        dest.mrm = mrmRecord;
                        
                        bwdDestsRow0.append(dest);
                        
                        // Do not include first row
                        if (!((mailboxX == 0u) && (boardX == 0u))) {                            
                            
                            // Construct destination the mailbox
                            
                            // If the mailbox is the first in the x-axis for the current board
                            if (mailboxX == 0u) {
                                dest.mbox = destBoardY;
                                dest.mbox = (dest.mbox << TinselMeshXBits) + boardX - 1u;
                                dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxY;
                                dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + (TinselMailboxMeshXLen - 1u);
                            }
                            else {
                                dest.mbox = destBoardY;
                                dest.mbox = (dest.mbox << TinselMeshXBits) + boardX;
                                dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxY;
                                dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + (mailboxX - 1u);                                   
                            }
                            
                            // Construct destination threads for row 1
                            mrmRecord.threadMaskLow = row1;
                            mrmRecord.threadMaskHigh = 0u;
                            dest.mrm = mrmRecord;
                            
                            // Append this information into the destination sequence
                            bwdDestsRow1.append(dest);
                        
                        }
                    }
                }
                
                // Create forward key row0 -> row1
                fwdColumnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][0u] = progRouterMesh.addDestsFromBoardXY(boardX, boardY, &fwdDestsRow1);
                
                fwdDestsRow1.clear();
                
                // Create forward key row1 -> row0 next mailbox
                if (!((mailboxX == TinselMailboxMeshXLen - 1u) && (boardX == TinselMeshXLenWithinBox - 1u))) {
                    // Create key row1 -> row0 next mailbox
                    fwdColumnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][1u] = progRouterMesh.addDestsFromBoardXY(boardX, boardY, &fwdDestsRow0);
                }
                
                fwdDestsRow0.clear();
                
                // Create backward key row1 -> row0
                bwdColumnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][1u] = progRouterMesh.addDestsFromBoardXY(boardX, boardY, &bwdDestsRow0);
                
                bwdDestsRow0.clear();
                
                // Create backward key row0 -> row1 previous mailbox
                if (!((mailboxX == 0u) && (boardX == 0u))) {
                    // Create key row1 -> row0 previous mailbox
                    bwdColumnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][0u] = progRouterMesh.addDestsFromBoardXY(boardX, boardY, &bwdDestsRow1);
                }
                
                bwdDestsRow1.clear();
            
            }
            
        }
    }
    
    //printf("\nKeys as being written out of the x86 host machine:\n\n");
    
    uint32_t baseAddress = 0u;
    
    // Store Keys in local cores *JPM this could be merged with above loop structutre after confirming correct
    
    for (boardY = 0u; boardY < TinselMeshYLenWithinBox; boardY++) {
        for (boardX = 0u; boardX < TinselMeshXLenWithinBox; boardX++) {
            for (mailboxY = 0u; mailboxY < TinselMailboxMeshYLen; mailboxY++) {
                for (mailboxX = 0u; mailboxX < TinselMailboxMeshXLen; mailboxX++) {
                    
                    coreID = (mailboxY * TinselMailboxMeshXLen * TinselCoresPerMailbox) + (mailboxX * TinselCoresPerMailbox);
                    
                    uint32_t observationPair = (boardX * (TinselMailboxMeshXLen) * ((TinselThreadsPerMailbox/2u)/16u)) + (mailboxX * ((TinselThreadsPerMailbox/2u)/16u));
                    
                    // Variables to caluclate total genetic distance
                    uint32_t currentIndex = 0u;
                    float geneticDistance = 0.0f;
                    
                    // Transition probabilites for same and different haplotypes for both observations
                    
                    float tau_m0 = 0u;
                    float same0 = 0u;
                    float diff0 = 0u;
                    
                    float* same0Ptr = &same0;
                    float* diff0Ptr = &diff0;
                    
                    uint32_t* same0UPtr = (uint32_t*) same0Ptr;
                    uint32_t* diff0UPtr = (uint32_t*) diff0Ptr;
                    
                    // Tau M Values
                    if (observationPair != 0u) {
                        
                        // Caluclate total genetic distance
                        currentIndex = (observationPair - 1u) * LINRATIO;
                        geneticDistance = 0.0f;
                        for (uint32_t x = 0u; x < LINRATIO; x++) {
                            
                            geneticDistance += dm[currentIndex + x];
                            
                        }
                        
                        tau_m0 = (1 - exp((-4 * NE * geneticDistance) / NOOFSTATES));
                        same0 = (1 - tau_m0) + (tau_m0 / NOOFSTATES);
                        diff0 = tau_m0 / NOOFSTATES;
                        
                        //printf("same trans prob = %.10f\n", *(float*)same0UPtr);
                        //printf("diff trans prob = %f\n", *(float*)diff0UPtr);
                        
                    }
                    
                    // Caluclate total genetic distance
                    currentIndex = (observationPair) * LINRATIO;
                    geneticDistance = 0.0f;
                    
                    for (uint32_t x = 0u; x < LINRATIO; x++) {
                        
                        geneticDistance += dm[currentIndex + x];
                        
                    }
                    
                    float tau_m1 = (1 - exp((-4 * NE * geneticDistance) / NOOFSTATES));
                    float same1 = (1 - tau_m1) + (tau_m1 / NOOFSTATES);
                    float diff1 = tau_m1 / NOOFSTATES;
                    
                    float* same1Ptr = &same1;
                    float* diff1Ptr = &diff1;
                    
                    uint32_t* same1UPtr = (uint32_t*) same1Ptr;
                    uint32_t* diff1UPtr = (uint32_t*) diff1Ptr;
                    
                    float tau_m2 = 0u;
                    float same2 = 0u;
                    float diff2 = 0u;
                    
                    float* same2Ptr = &same2;
                    float* diff2Ptr = &diff2;
                    
                    uint32_t* same2UPtr = (uint32_t*) same2Ptr;
                    uint32_t* diff2UPtr = (uint32_t*) diff2Ptr;
                    
                    // THIS ASSUMES AN EVEN NUMBER OF OBSERVATIONS AND WILL NEED CHANGING IF ODD NUMBERS OF OBSERVATIONS ARE USED
                    if ((observationPair != (NOOFTARGMARK - 1u))) {
                        
                        // Caluclate total genetic distance
                        currentIndex = (observationPair + 1u) * LINRATIO;
                        geneticDistance = 0.0f;
                        
                        for (uint32_t x = 0u; x < LINRATIO; x++) {
                            
                            geneticDistance += dm[currentIndex + x];
                            
                        }
                        
                        tau_m2 = (1 - exp((-4 * NE * geneticDistance) / NOOFSTATES));
                        same2 = (1 - tau_m2) + (tau_m2 / NOOFSTATES);
                        diff2 = tau_m2 / NOOFSTATES;
                        
                        //printf("same trans prob = %.10f\n", *(float*)same2UPtr);
                        //printf("diff trans prob = %f\n", *(float*)diff2UPtr);
                        
                    }
                    
                    // Core 0 -> HMM
                    for (mailboxLocalThreadID = 0u; mailboxLocalThreadID < 16u; mailboxLocalThreadID++) {
                        
                        //printf("Obs No = %d, State No = %d\n", getObservationNumber(boardX, mailboxX, mailboxLocalThreadID), getStateNumber(boardY, mailboxY, mailboxLocalThreadID));
                        
                        uint32_t observationNo = getObservationNumber(boardX, mailboxX, mailboxLocalThreadID);
                        uint32_t stateNo = getStateNumber(boardY, mailboxY, mailboxLocalThreadID);
                        uint32_t match = 0u;
                        
                        // if markers match
                        if (hmm_labels[stateNo][observationNo] == observation[observationNo][1]) {
                            match = 1u;
                        }
                    
                        // Construct ThreadID
                        threadID = boardY;
                        threadID = (threadID << TinselMeshXBits) + boardX;
                        threadID = (threadID << TinselMailboxMeshYBits) + mailboxY;
                        threadID = (threadID << TinselMailboxMeshXBits) + mailboxX;
                        threadID = (threadID << TinselLogThreadsPerMailbox) + mailboxLocalThreadID;
                        
                        baseAddress = tinselHeapBaseGeneric(threadID);
                                                
                        
                        //printf("CoreID: %d, ThreadID: %X, baseAddress: %X\n", coreID, threadID, baseAddress);
                        
                        hostLink.setAddr(boardX, boardY, coreID, baseAddress);
                        
                        if (mailboxLocalThreadID < 8u ) {
                            hostLink.store(boardX, boardY, coreID, 1u, &fwdColumnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][0u]);
                            hostLink.store(boardX, boardY, coreID, 1u, &bwdColumnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][0u]);
                            hostLink.store(boardX, boardY, coreID, 1u, &match);
                            //printf("ThreadID: %X, Key: %X\n", threadID, fwdColumnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][0u]);
                            //printf("Observation No = %d\n", observationPair);
                            
                            // Forward transition propbabilties
                            hostLink.store(boardX, boardY, coreID, 1u, same0UPtr);
                            hostLink.store(boardX, boardY, coreID, 1u, diff0UPtr);
                            
                            // Backward Transistion Probabilites
                            hostLink.store(boardX, boardY, coreID, 1u, same1UPtr);
                            hostLink.store(boardX, boardY, coreID, 1u, diff1UPtr);                            
                            
                        }
                        else {
                            hostLink.store(boardX, boardY, coreID, 1u, &fwdColumnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][1u]);
                            hostLink.store(boardX, boardY, coreID, 1u, &bwdColumnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][1u]);
                            hostLink.store(boardX, boardY, coreID, 1u, &match);
                            //printf("ThreadID: %X, Key: %X\n", threadID, fwdColumnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][1u]);
                            //printf("Observation No = %d\n", observationPair + 1u);
                            
                            // Forward transition propbabilties
                            hostLink.store(boardX, boardY, coreID, 1u, same1UPtr);
                            hostLink.store(boardX, boardY, coreID, 1u, diff1UPtr);
                            
                            // Backward Transistion Probabilites
                            hostLink.store(boardX, boardY, coreID, 1u, same2UPtr);
                            hostLink.store(boardX, boardY, coreID, 1u, diff2UPtr); 
                        }
                    
                    }
                    
                    // // Core 1 -> HMM
                    coreID++;
                    
                    for (mailboxLocalThreadID = 16u; mailboxLocalThreadID < 32u; mailboxLocalThreadID++) {
                        
                        uint32_t observationNo = getObservationNumber(boardX, mailboxX, mailboxLocalThreadID);
                        uint32_t stateNo = getStateNumber(boardY, mailboxY, mailboxLocalThreadID);
                        uint32_t match = 0u;
                        
                        // if markers match
                        if (hmm_labels[stateNo][observationNo] == observation[observationNo][1]) {
                            match = 1u;
                        }
                    
                        // Construct ThreadID
                        threadID = boardY;
                        threadID = (threadID << TinselMeshXBits) + boardX;
                        threadID = (threadID << TinselMailboxMeshYBits) + mailboxY;
                        threadID = (threadID << TinselMailboxMeshXBits) + mailboxX;
                        threadID = (threadID << TinselLogThreadsPerMailbox) + mailboxLocalThreadID;
                        
                        baseAddress = tinselHeapBaseGeneric(threadID);
                        
                        hostLink.setAddr(boardX, boardY, coreID, baseAddress);
                        
                        if (mailboxLocalThreadID < 24u ) {
                            hostLink.store(boardX, boardY, coreID, 1u, &fwdColumnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][0u]);
                            hostLink.store(boardX, boardY, coreID, 1u, &bwdColumnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][0u]);
                            hostLink.store(boardX, boardY, coreID, 1u, &match);
                            //printf("ThreadID: %X, Key: %X\n", threadID, fwdColumnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][0u]);
                            //printf("Observation No = %d\n", observationPair);
                            
                            // Forward transition propbabilties
                            hostLink.store(boardX, boardY, coreID, 1u, same0UPtr);
                            hostLink.store(boardX, boardY, coreID, 1u, diff0UPtr);
                            
                            // Backward Transistion Probabilites
                            hostLink.store(boardX, boardY, coreID, 1u, same1UPtr);
                            hostLink.store(boardX, boardY, coreID, 1u, diff1UPtr); 
                        }
                        else {
                            hostLink.store(boardX, boardY, coreID, 1u, &fwdColumnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][1u]);
                            hostLink.store(boardX, boardY, coreID, 1u, &bwdColumnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][1u]);
                            hostLink.store(boardX, boardY, coreID, 1u, &match);
                            //printf("ThreadID: %X, Key: %X\n", threadID, fwdColumnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][1u]);
                            //printf("Observation No = %d\n", observationPair + 1u);

                            // Forward transition propbabilties
                            hostLink.store(boardX, boardY, coreID, 1u, same1UPtr);
                            hostLink.store(boardX, boardY, coreID, 1u, diff1UPtr);
                            
                            // Backward Transistion Probabilites
                            hostLink.store(boardX, boardY, coreID, 1u, same2UPtr);
                            hostLink.store(boardX, boardY, coreID, 1u, diff2UPtr);;
                        }
                    
                    }
                    
                    // Core 2 -> Linear Interpolation
                    coreID++;
                    
                    for (mailboxLocalThreadID = 32u; mailboxLocalThreadID < 48u; mailboxLocalThreadID++) {
                        
                        // Construct ThreadID
                        threadID = boardY;
                        threadID = (threadID << TinselMeshXBits) + boardX;
                        threadID = (threadID << TinselMailboxMeshYBits) + mailboxY;
                        threadID = (threadID << TinselMailboxMeshXBits) + mailboxX;
                        threadID = (threadID << TinselLogThreadsPerMailbox) + mailboxLocalThreadID;
                        
                        baseAddress = tinselHeapBaseGeneric(threadID);
                                                
                        
                        //printf("CoreID: %d, ThreadID: %X, baseAddress: %X\n", coreID, threadID, baseAddress);
                        
                        hostLink.setAddr(boardX, boardY, coreID, baseAddress);
                        
                        if (mailboxLocalThreadID < 40u ) {
                            
                            float dmSingle = 0.0f;
                            float* dmPtr = &dmSingle;
                            uint32_t* dmUPtr = (uint32_t*) dmPtr;
                            
                            for (uint32_t x = 0u; x < LINRATIO; x++) {
                                
                                dmSingle = dm[(observationPair * LINRATIO) + x];
                                hostLink.store(boardX, boardY, coreID, 1u, dmUPtr);
                                
                            }
                            
                        }
                        else {
                            
                            float dmSingle = 0.0f;
                            float* dmPtr = &dmSingle;
                            uint32_t* dmUPtr = (uint32_t*) dmPtr;
                            
                            for (uint32_t x = 0u; x < LINRATIO; x++) {
                                
                                dmSingle = dm[((observationPair + 1u) * LINRATIO) + x];
                                hostLink.store(boardX, boardY, coreID, 1u, dmUPtr);
                                
                            }
                            
                        }
                        
                    }
                    
                    // Core 3 -> Linear Interpolation
                    coreID++;
                    
                    for (mailboxLocalThreadID = 48u; mailboxLocalThreadID < 64u; mailboxLocalThreadID++) {
                        
                        // Construct ThreadID
                        threadID = boardY;
                        threadID = (threadID << TinselMeshXBits) + boardX;
                        threadID = (threadID << TinselMailboxMeshYBits) + mailboxY;
                        threadID = (threadID << TinselMailboxMeshXBits) + mailboxX;
                        threadID = (threadID << TinselLogThreadsPerMailbox) + mailboxLocalThreadID;
                        
                        baseAddress = tinselHeapBaseGeneric(threadID);
                                                
                        
                        //printf("CoreID: %d, ThreadID: %X, baseAddress: %X\n", coreID, threadID, baseAddress);
                        
                        hostLink.setAddr(boardX, boardY, coreID, baseAddress);
                        
                        if (mailboxLocalThreadID < 40u ) {
                            
                            float dmSingle = 0.0f;
                            float* dmPtr = &dmSingle;
                            uint32_t* dmUPtr = (uint32_t*) dmPtr;
                            
                            for (uint32_t x = 0u; x < LINRATIO; x++) {
                                
                                dmSingle = dm[(observationPair * LINRATIO) + x];
                                hostLink.store(boardX, boardY, coreID, 1u, dmUPtr);
                                
                            }
                            
                        }
                        else {
                            
                            float dmSingle = 0.0f;
                            float* dmPtr = &dmSingle;
                            uint32_t* dmUPtr = (uint32_t*) dmPtr;
                            
                            for (uint32_t x = 0u; x < LINRATIO; x++) {
                                
                                dmSingle = dm[((observationPair + 1u) * LINRATIO) + x];
                                hostLink.store(boardX, boardY, coreID, 1u, dmUPtr);
                                
                            }
                            
                        }
                        
                    }
            
                }
            }
        }
    }
    
    // Start Cores
    /*
    for (boardY = 0u; boardY < TinselMeshYLenWithinBox; boardY++) {
        for (boardX = 0u; boardX < TinselMeshXLenWithinBox; boardX++) {
            for (coreID = 0u; coreID < TinselCoresPerBoard; coreID++) {
               
                hostLink.startOne(boardX, boardY, coreID, 16);
            
            }
        }
    }
    */
    
    // Write the keys to the routers
    progRouterMesh.write(&hostLink);
    
    hostLink.startAll();
    
    // Trigger Application Execution in reverse order to avoid race condition
    /*
    for (boardX = 0u; boardX < TinselMeshXLenWithinBox; boardX++) {
        for (boardY = 0u; boardY < TinselMeshYLenWithinBox; boardY++) {
            for (coreID = 0u; coreID < TinselCoresPerBoard; coreID++) {
               
                hostLink.goOne(((TinselMeshXLenWithinBox - 1u) - boardX), boardY, ((TinselCoresPerBoard - 1u) - coreID));
            
            }
        }
    }
    */
    
    hostLink.go();
    
    
    // Construct Ping Message
    
    //uint32_t msg[1 << TinselLogWordsPerMsg];
    HostMessage msg;
    
    /*
    // Send Ping to all threads
    
    for (boardY = 0u; boardY < TinselMeshYLenWithinBox; boardY++) {
        for (boardX = 0u; boardX < TinselMeshXLenWithinBox; boardX++) {
            for (mailboxY = 0u; mailboxY < TinselMailboxMeshYLen; mailboxY++) {
                for (mailboxX = 0u; mailboxX < TinselMailboxMeshXLen; mailboxX++) {
                    for (mailboxLocalThreadID = 0u; mailboxLocalThreadID < TinselThreadsPerMailbox; mailboxLocalThreadID++) {
                
                        // Construct ThreadID
                        threadID = boardY;
                        threadID = (threadID << TinselMeshXBits) + boardX;
                        threadID = (threadID << TinselMailboxMeshYBits) + mailboxY;
                        threadID = (threadID << TinselMailboxMeshXBits) + mailboxX;
                        threadID = (threadID << TinselLogThreadsPerMailbox) + mailboxLocalThreadID;
                       
                        printf("Sending ping to board x %d, board y %d, mailbox x %d, mailbox y %d, local threadID %d\n", boardX, boardY, mailboxX, mailboxY, mailboxLocalThreadID);
                        hostLink.send(threadID, 1, ping);
            
                    }
                }
            }
        }
    }
    */
    
    // Receive responses from all threads
    /*
    for (boardY = 0u; boardY < TinselMeshYLenWithinBox; boardY++) {
        for (boardX = 0u; boardX < TinselMeshXLenWithinBox; boardX++) {
            for (mailboxY = 0u; mailboxY < TinselMailboxMeshYLen; mailboxY++) {
                for (mailboxX = 0u; mailboxX < TinselMailboxMeshXLen; mailboxX++) {
                    for (mailboxLocalThreadID = 0u; mailboxLocalThreadID < (TinselThreadsPerMailbox/2u); mailboxLocalThreadID++) {
                
                        hostLink.recv(msg);
                        printf("ThreadID: %X has Key: %X\n", msg[0], msg[1]);
                        //printf("ThreadID: %X LocalID: %d Row: %d MailboxX: %d MailboxY: %d BoardX: %d BoardY: %d\n", msg[0], msg[1], msg[2], msg[3], msg[4], msg[5], msg[6]);
            
                    }
                }
            }
        }
    }*/
    
    uint32_t x = 0u;
    uint32_t y = 0u;
    uint8_t msgType = 0u;
    
    float result[NOOFSTATES][NOOFOBS][2u] = {0.0f};
    
    for (msgType = 0u; msgType < 2; msgType++) {
        for (x = 0u; x < NOOFSTATES; x++) {
            for (y = 0u; y < NOOFTARGMARK; y++) {
                //hostLink.recv(msg);
                hostLink.recvMsg(&msg, sizeof(msg));
                //printf("ThreadID: %X LocalID: %d Row: %d MailboxX: %d MailboxY: %d BoardX: %d BoardY: %d Message: %d\n", msg[0], msg[1], msg[2], msg[3], msg[4], msg[5], msg[6], msg[7]);
                //printf("State No: %d has returned alpha: %0.10f\n", msg.observationNo, msg.alpha);
                if (msg.msgType < 2u) {
                    
                    result[msg.stateNo][msg.observationNo * (LINRATIO + 1u)][msg.msgType] = msg.val;
    
                }
            }
        }
    }
    
    printf("Forward Probabilities: \n");
    for (y = 0u; y < NOOFSTATES; y++) {
        for (x = 0u; x < NOOFOBS; x++) {
            printf("%.3e ", result[y][x][0u]);
        }
        printf("\n");
    }
    
    printf("Backward Probabilities: \n");
    for (y = 0u; y < NOOFSTATES; y++) {
        for (x = 0u; x < NOOFOBS; x++) {
            printf("%.3e ", result[y][x][1u]);
        }
        printf("\n");
    }
    
    /*
    float result[NOOFSTATES];

    for (y = 0u; y < NOOFSTATES; y++) {
        //hostLink.recv(msg);
        hostLink.recvMsg(&msg, sizeof(msg));
        //printf("ThreadID: %X LocalID: %d Row: %d MailboxX: %d MailboxY: %d BoardX: %d BoardY: %d Message: %d\n", msg[0], msg[1], msg[2], msg[3], msg[4], msg[5], msg[6], msg[7]);
        //printf("State No: %d has returned alpha: %0.10f\n", msg.observationNo, msg.alpha);
        result[msg.stateNo] = msg.val;
    }
    
    for (y = 0u; y < NOOFSTATES; y++) {
        printf("%.3e\n", result[y]);
        //printf("%d\n", result[y]);
    }
    */
    
    
#ifdef PRINTDEBUG    
    printf("\nKeys. Two per column as there are two source Y board in a box. The last key is not used:\n\n");
    
    for (y = 0u; y < YKEYS; y++) {
        for (x = 0u; x < XKEYS; x++) {
            
            if (x != XKEYS-1) {
                printf("%X, ", bwdColumnKey[x][y][0]);
                printf("%X, ", bwdColumnKey[x][y][1]);
            }
            else {
                printf("%X, ", bwdColumnKey[x][y][0]);
                printf("%X\n", bwdColumnKey[x][y][1]);
            }
            
        }
    }
#endif    
    return 0;
}
