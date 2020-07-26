// SPDX-License-Identifier: BSD-2-Clause
#include <HostLink.h>

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
    
    // Load Ping_0 onto Core 0
    hostLink.loadInstrsOntoCore("code_0.v", 0, 0, 0);
    hostLink.loadDataViaCore("data_0.v", 0, 0, 0);
    
    // Start 16 threads on Core 0
    hostLink.startOne(0, 0, 0, 16);
    
    // Trigger Execution on Core 0
    hostLink.goOne(0, 0, 0);
    
    
  
    /*
    // Write All Cores on All Boards in a single box
    for (boardY = 0u; boardY < TinselMeshYLenWithinBox; boardY++) {
        for (boardX = 0u; boardX < TinselMeshXLenWithinBox; boardX++) {
            for (coreID = 0u; coreID < TinselCoresPerBoard; coreID++) {
               
                // Write HMM into lower two cores in tile
                // This may be reduced to only two writes given the instruction memory sharing. Needs Investigating JPM.
                
                // Core 0 in tile
                hostLink.loadInstrsOntoCore("code_0.v", boardX, boardY, coreID);
                hostLink.loadDataViaCore("data_0.v", boardX, boardY, coreID);
                
                coreID++;
                
                // Core 1 in tile
                hostLink.loadInstrsOntoCore("code_0.v", boardX, boardY, coreID);
                hostLink.loadDataViaCore("data_0.v", boardX, boardY, coreID);
                
                coreID++;
                
                // Write Linear Interpolation into upper two cores in tile
                
                // Core 2 in tile
                hostLink.loadInstrsOntoCore("code_1.v", boardX, boardY, coreID);
                hostLink.loadDataViaCore("data_1.v", boardX, boardY, coreID);
                
                coreID++;
                
                // Core 3 in tile
                hostLink.loadInstrsOntoCore("code_1.v", boardX, boardY, coreID);
                hostLink.loadDataViaCore("data_1.v", boardX, boardY, coreID);
 
            }
        }
    }
    */

    printf("Starting\n");

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
    
    // Start all threads on all cores
    //hostLink.startAll();
    
    /*
    // Trigger Application Execution On All Cores In A Single Box
    
    for (boardY = 0u; boardY < TinselMeshYLenWithinBox; boardY++) {
        for (boardX = 0u; boardX < TinselMeshXLenWithinBox; boardX++) {
            for (coreID = 0u; coreID < TinselCoresPerBoard; coreID++) {
               
                hostLink.goOne(boardX, boardY, coreID);
            
            }
        }
    }
    */
    /*
    // Construct Ping Message

    uint32_t ping[1 << TinselLogWordsPerMsg];
    uint64_t total_count = 0u;
    uint64_t upper_count = 0u;
    double total_time = 0.0f;
    ping[0] = 100;
    
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
    
    // Receive responses from all threads
    
    for (boardY = 0u; boardY < TinselMeshYLenWithinBox; boardY++) {
        for (boardX = 0u; boardX < TinselMeshXLenWithinBox; boardX++) {
            for (mailboxY = 0u; mailboxY < TinselMailboxMeshYLen; mailboxY++) {
                for (mailboxX = 0u; mailboxX < TinselMailboxMeshXLen; mailboxX++) {
                    for (mailboxLocalThreadID = 0u; mailboxLocalThreadID < TinselThreadsPerMailbox; mailboxLocalThreadID++) {
                
                        hostLink.recv(ping);
                        upper_count = ping[1];
                        total_count = ((upper_count << 32) + ping[0]);
                        total_time = total_count / (TinselClockFreq * 1000000.0);
                        printf("Upper Count: %ld, Lower Count: %ld, Total Count: %ld, Total Time: %lf\n", upper_count, ping[0], total_count, total_time);
            
                    }
                }
            }
        }
    }
    */
    
    uint32_t msg[1 << TinselLogWordsPerMsg];

    hostLink.recv(msg);
    printf("Final Value: %d\n", msg[0]);

    return 0;
}
