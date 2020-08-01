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
    uint16_t mailboxLocalThreadID = 0u;
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
    
    uint32_t columnKey[XKEYS][YKEYS][ROWSPERCORE] = {0u};
    
    PRoutingDest dest;
    PRoutingDestMRM mrmRecord;
    Seq<PRoutingDest> destsRow0;
    Seq<PRoutingDest> destsRow1;
    
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
                                                                                    
                        // Construct destination the mailbox
                        dest.mbox = destBoardY;
                        dest.mbox = (dest.mbox << TinselMeshXBits) + boardX;
                        dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxY;
                        dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + mailboxX;
                        
                        // Construct destination threads for row 1
                        mrmRecord.threadMaskLow = row1;
                        mrmRecord.threadMaskHigh = 0u;
                        dest.mrm = mrmRecord;
#ifdef PRINTDEBUG                        
                        if ((mailboxX == 0u) && (boardX == 0u) && (boardY == 0u)) {
                            printf("BoardX: %d BoardY: %d mailboxX: %d mailboxY: %d threads: %X added to key 1\n", boardX, destBoardY, mailboxX, mailboxY, row1);
                        }
#endif                        
                        destsRow1.append(dest);                       
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
                            destsRow0.append(dest);
                        
                        }
                    
                    }
                }
                
                // Create key row0 -> row1
                columnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][0u] = progRouterMesh.addDestsFromBoardXY(boardX, boardY, &destsRow1);
                
                destsRow1.clear();
                
                if (!((mailboxX == TinselMailboxMeshXLen - 1u) && (boardX == TinselMeshXLenWithinBox - 1u))) {
                    // Create key row1 -> row0 next mailbox
                    columnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][1u] = progRouterMesh.addDestsFromBoardXY(boardX, boardY, &destsRow0);
                }
                
                destsRow0.clear();
            
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
                    
                    uint32_t observationNo = (boardX * (TinselMailboxMeshXLen) * ((TinselThreadsPerMailbox/2u)/16u)) + (mailboxX * ((TinselThreadsPerMailbox/2u)/16u));
                    
                    // Transition probabilites for same and different haplotypes for both observations
                    
                    float tau_m0;
                    float same0;
                    float diff0;
                    
                    float* same0Ptr;
                    float* diff0Ptr;
                    
                    uint32_t* same0UPtr;
                    uint32_t* diff0UPtr;
                    
                    // Tau M Values
                    if (observationNo != 0u) {
                        
                        tau_m0 = (1 - exp((-4 * NE * dm[observationNo - 1u]) / NOOFSTATES));
                        same0 = tau_m0 / NOOFSTATES;
                        diff0 = (1 - tau_m0) + (tau_m0 / NOOFSTATES);

                        same0Ptr = &same0;
                        diff0Ptr = &diff0;
                        
                        same0UPtr = (uint32_t*) same0Ptr;
                        diff0UPtr = (uint32_t*) diff0Ptr;
                        
                        //printf("same trans prob = %.10f\n", *(float*)same0UPtr);
                        //printf("diff trans prob = %f\n", *(float*)diff0UPtr);
                        
                    }
                    
                    float tau_m1 = (1 - exp((-4 * NE * dm[observationNo]) / NOOFSTATES));
                    float same1 = tau_m1 / NOOFSTATES;
                    float diff1 = (1 - tau_m1) + (tau_m1 / NOOFSTATES);
                    
                    float* same1Ptr = &same1;
                    float* diff1Ptr = &diff1;
                    
                    uint32_t* same1UPtr = (uint32_t*) same1Ptr;
                    uint32_t* diff1UPtr = (uint32_t*) diff1Ptr;
                    
                    
                    for (mailboxLocalThreadID = 0u; mailboxLocalThreadID < 16u; mailboxLocalThreadID++) {
                    
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
                            hostLink.store(boardX, boardY, coreID, 1u, &columnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][0u]);
                            //printf("ThreadID: %X, Key: %X\n", threadID, columnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][0u]);
                            //printf("Observation No = %d\n", observationNo);
                            
                            //CALCULATE THE TAU
                            //CALCULATE THE TRANSITION PROBABILITIES
                            //STORE IT HERE
                            
                            if (observationNo != 0u) {
                                hostLink.store(boardX, boardY, coreID, 1u, same0UPtr);
                                hostLink.store(boardX, boardY, coreID, 1u, diff0UPtr);
                            }
                            
                        }
                        else {
                            hostLink.store(boardX, boardY, coreID, 1u, &columnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][1u]);
                            //printf("ThreadID: %X, Key: %X\n", threadID, columnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][1u]);
                            //printf("Observation No = %d\n", observationNo + 1u);
                            
                            //CALCULATE THE TAU
                            //CALCULATE THE TRANSITION PROBABILITIES
                            //STORE IT HERE
                            
                            hostLink.store(boardX, boardY, coreID, 1u, same1UPtr);
                            hostLink.store(boardX, boardY, coreID, 1u, diff1UPtr);
                        }
                    
                    }
                    
                    // Core 1 for the same mailbox...
                    coreID++;
                    
                    for (mailboxLocalThreadID = 16u; mailboxLocalThreadID < 32u; mailboxLocalThreadID++) {
                    
                        // Construct ThreadID
                        threadID = boardY;
                        threadID = (threadID << TinselMeshXBits) + boardX;
                        threadID = (threadID << TinselMailboxMeshYBits) + mailboxY;
                        threadID = (threadID << TinselMailboxMeshXBits) + mailboxX;
                        threadID = (threadID << TinselLogThreadsPerMailbox) + mailboxLocalThreadID;
                        
                        baseAddress = tinselHeapBaseGeneric(threadID);
                        
                        hostLink.setAddr(boardX, boardY, coreID, baseAddress);
                        
                        if (mailboxLocalThreadID < 24u ) {
                            hostLink.store(boardX, boardY, coreID, 1u, &columnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][0u]);
                            //printf("ThreadID: %X, Key: %X\n", threadID, columnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][0u]);
                            //printf("Observation No = %d\n", observationNo);
                            //CALCULATE THE TAU
                            //CALCULATE THE TRANSITION PROBABILITIES
                            //STORE IT HERE
                            hostLink.store(boardX, boardY, coreID, 1u, same0UPtr);
                            hostLink.store(boardX, boardY, coreID, 1u, diff0UPtr);
                        }
                        else {
                            hostLink.store(boardX, boardY, coreID, 1u, &columnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][1u]);
                            //printf("ThreadID: %X, Key: %X\n", threadID, columnKey[(boardX * TinselMailboxMeshXLen) + mailboxX][boardY][1u]);
                            //printf("Observation No = %d\n", observationNo + 1u);
                            //CALCULATE THE TAU
                            //CALCULATE THE TRANSITION PROBABILITIES
                            //STORE IT HERE
                            hostLink.store(boardX, boardY, coreID, 1u, same1UPtr);
                            hostLink.store(boardX, boardY, coreID, 1u, diff1UPtr);
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
    ImpMessage msg;
    
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
    
    for (x = 0u; x < 128u; x++) {
        //hostLink.recv(msg);
        hostLink.recvMsg(&msg, sizeof(msg));
        //printf("ThreadID: %X LocalID: %d Row: %d MailboxX: %d MailboxY: %d BoardX: %d BoardY: %d Message: %d\n", msg[0], msg[1], msg[2], msg[3], msg[4], msg[5], msg[6], msg[7]);
        printf("Observation: %d has returned value: %.10f\n", msg.threadID, msg.val);
    }
    
#ifdef PRINTDEBUG    
    printf("\nKeys. Two per column as there are two source Y board in a box. The last key is not used:\n\n");
    
    for (y = 0u; y < YKEYS; y++) {
        for (x = 0u; x < XKEYS; x++) {
            
            if (x != XKEYS-1) {
                printf("%X, ", columnKey[x][y][0]);
                printf("%X, ", columnKey[x][y][1]);
            }
            else {
                printf("%X, ", columnKey[x][y][0]);
                printf("%X\n", columnKey[x][y][1]);
            }
            
        }
    }
#endif    
    return 0;
}
