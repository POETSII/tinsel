#define TINSEL (1u)
//#define PRINTDEBUG (1u)

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
 * scp -r C:\Users\drjor\Documents\tinsel\apps\imputationhomog jordmorr@fielding.cl.cam.ac.uk:~/tinsel/apps
 * scp jordmorr@fielding.cl.cam.ac.uk:~/tinsel/apps/imputationhomog/results.csv C:\Users\drjor\Documents\tinsel\apps\imputationhomog
 * ****************************************************/
 
const uint8_t COLSPERCORE = 2u;
const uint8_t THREADSPERCOL = 8U;
const uint32_t XKEYS = (TinselBoardsPerBox - 1u) * TinselCoresPerBoard * COLSPERCORE;
//const uint8_t YKEYS = 8u;

int main()
{
    // Create the hostlink (with a single box dimension by default)
    HostLink hostLink;

    // Creat the programmable router mesh with single box dimensions
    ProgRouterMesh progRouterMesh(TinselMeshXLenWithinBox, TinselMeshYLenWithinBox);
    
    // Generate and transmit pre-execution data
    
    // Generate multicast keys
    
    // Array to store keys
    
    uint32_t fwdColumnKey[XKEYS] = {0u};
    uint32_t bwdColumnKey[XKEYS] = {0u};
    
    PRoutingDest dest;
    PRoutingDestMRM mrmRecord;
    Seq<PRoutingDest> fwdDests;
    Seq<PRoutingDest> bwdDests;
    
    dest.kind = PRDestKindMRM;
    
    mrmRecord.key = 0u;
    
    for (uint8_t board = 0u; board < (TinselBoardsPerBox - 1u); board++) {
    
        for (uint8_t mailbox = 0u; mailbox < TinselMailboxesPerBoard; mailbox++) {
            
            for (uint8_t col = 0u; col < (TinselCoresPerMailbox * COLSPERCORE); col++) {
                
                //Global Column Number
                uint32_t globalColumn = (board * TinselCoresPerMailbox * COLSPERCORE * TinselMailboxesPerBoard) + (mailbox * TinselCoresPerMailbox * COLSPERCORE) + col;
            
                // FORWARD KEYS
                
                // If we are the last col on the current board
                if ( (col == ((TinselCoresPerMailbox * COLSPERCORE) - 1u)) && (mailbox == TinselMailboxesPerBoard - 1u) ) {
                    
                    // If we arent in the last col for the box
                    if ( !(board == TinselBoardsPerBox - 2u) ) {
                        
                        // Construct destination the mailbox for the next board
                        dest.mbox = boardPath[board + 1u][1u];
                        dest.mbox = (dest.mbox << TinselMeshXBits) + boardPath[board + 1u][0u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxPath[0u][1u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + mailboxPath[0u][0u];

                        // Construct destination threads
                        mrmRecord.threadMaskLow = 0xFF;
                        mrmRecord.threadMaskHigh = 0u;
                        //printf("Global Col: %d, ", (globalColumn));
                        //printf("boardX: %d, boardY: %d, mailboxX: %d, mailboxY: %d, col: %d, ThreadMaskLow: %X, ThreadMaskHigh: %X\n", boardPath[board + 1u][0u], boardPath[board + 1u][1u], mailboxPath[0u][0u], mailboxPath[0u][1u], col, mrmRecord.threadMaskLow, mrmRecord.threadMaskHigh);
                        dest.mrm = mrmRecord;
                        
                    }
                    
                }
                // If we are the last col in the current mailbox
                else if ( col == ((TinselCoresPerMailbox * COLSPERCORE) - 1u) ) {
                    
                        // Construct destination the next mailbox
                        dest.mbox = boardPath[board][1u];
                        dest.mbox = (dest.mbox << TinselMeshXBits) + boardPath[board][0u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxPath[mailbox + 1u][1u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + mailboxPath[mailbox + 1u][0u];
                        
                        // Construct destination threads
                        mrmRecord.threadMaskLow = 0xFF;
                        mrmRecord.threadMaskHigh = 0u;
                        //printf("Global Col: %d, ", (globalColumn));
                        //printf("boardX: %d, boardY: %d, mailboxX: %d, mailboxY: %d, col: %d, ThreadMaskLow: %X, ThreadMaskHigh: %X\n", boardPath[board][0u], boardPath[board][1u], mailboxPath[mailbox + 1u][0u], mailboxPath[mailbox + 1u][1u], col, mrmRecord.threadMaskLow, mrmRecord.threadMaskHigh);
                        dest.mrm = mrmRecord;
                    
                }
                // If we are just a regular col
                else {
                    
                        // Construct destination the same mailbox
                        dest.mbox = boardPath[board][1u];
                        dest.mbox = (dest.mbox << TinselMeshXBits) + boardPath[board][0u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxPath[mailbox][1u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + mailboxPath[mailbox][0u];
                        
                        // Construct destination threads for next col (hence + 8u)
                        uint64_t threadMask = 0xFF;
                        mrmRecord.threadMaskLow = uint32_t(threadMask << ((col + 1u) * 8u));
                        mrmRecord.threadMaskHigh = uint32_t((threadMask << ((col + 1u) * 8u)) >> 32u);
                        //printf("Global Col: %d, ", (globalColumn));
                        //printf("boardX: %d, boardY: %d, mailboxX: %d, mailboxY: %d, col: %d, ThreadMaskLow: %X, ThreadMaskHigh: %X\n", boardPath[board][0u], boardPath[board][1u], mailboxPath[mailbox][0u], mailboxPath[mailbox][1u], col, mrmRecord.threadMaskLow, mrmRecord.threadMaskHigh);
                        dest.mrm = mrmRecord;
                    
                }
                
                fwdDests.append(dest);
                
                // BACKWARD KEYS
                
                // If we are the first col on the current board
                if ( (col == 0u) && (mailbox == 0u) ) {
                    
                    // If we arent in the first col for the box
                    if ( !(board == 0u) ) {
                        
                        // Construct destination the mailbox for the previous board
                        dest.mbox = boardPath[board - 1u][1u];
                        dest.mbox = (dest.mbox << TinselMeshXBits) + boardPath[board - 1u][0u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxPath[TinselMailboxesPerBoard - 1u][1u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + mailboxPath[TinselMailboxesPerBoard - 1u][0u];

                        // Construct destination threads
                        mrmRecord.threadMaskLow = 0x0;
                        mrmRecord.threadMaskHigh = 0xFF000000;
                        printf("Global Col: %d, ", (globalColumn));
                        printf("boardX: %d, boardY: %d, mailboxX: %d, mailboxY: %d, col: %d, ThreadMaskLow: %X, ThreadMaskHigh: %X\n", boardPath[board - 1u][0u], boardPath[board - 1u][1u], mailboxPath[TinselCoresPerBoard - 1u][0u], mailboxPath[TinselCoresPerBoard - 1u][1u], col, mrmRecord.threadMaskLow, mrmRecord.threadMaskHigh);
                        dest.mrm = mrmRecord;
                        
                    }
                    
                }
                // If we are the first col in the current mailbox
                else if (col == 0u) {
                    
                        // Construct destination the next mailbox
                        dest.mbox = boardPath[board][1u];
                        dest.mbox = (dest.mbox << TinselMeshXBits) + boardPath[board][0u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxPath[mailbox - 1u][1u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + mailboxPath[mailbox - 1u][0u];
                        
                        // Construct destination threads
                        mrmRecord.threadMaskLow = 0x0;
                        mrmRecord.threadMaskHigh = 0xFF000000;
                        printf("Global Col: %d, ", (globalColumn));
                        printf("boardX: %d, boardY: %d, mailboxX: %d, mailboxY: %d, col: %d, ThreadMaskLow: %X, ThreadMaskHigh: %X\n", boardPath[board][0u], boardPath[board][1u], mailboxPath[mailbox - 1u][0u], mailboxPath[mailbox - 1u][1u], col, mrmRecord.threadMaskLow, mrmRecord.threadMaskHigh);
                        dest.mrm = mrmRecord;
                    
                }
                // If we are just a regular col
                else {
                    
                        // Construct destination the same mailbox
                        dest.mbox = boardPath[board][1u];
                        dest.mbox = (dest.mbox << TinselMeshXBits) + boardPath[board][0u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxPath[mailbox][1u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + mailboxPath[mailbox][0u];
                        
                        // Construct destination threads for next col (hence + 8u)
                        uint64_t threadMask = 0xFF00000000000000;
                        mrmRecord.threadMaskLow = uint32_t(threadMask >> (((((TinselCoresPerMailbox * COLSPERCORE)-1u) - col) + 1u) * 8u));
                        mrmRecord.threadMaskHigh = uint32_t((threadMask >> (((((TinselCoresPerMailbox * COLSPERCORE)-1u) - col) + 1u) * 8u)) >> 32u);
                        printf("Global Col: %d, ", (globalColumn));
                        printf("boardX: %d, boardY: %d, mailboxX: %d, mailboxY: %d, col: %d, ThreadMaskLow: %X, ThreadMaskHigh: %X\n", boardPath[board][0u], boardPath[board][1u], mailboxPath[mailbox][0u], mailboxPath[mailbox][1u], col, mrmRecord.threadMaskLow, mrmRecord.threadMaskHigh);
                        dest.mrm = mrmRecord;
                    
                }
                
                bwdDests.append(dest);
                
                
                // If we arent in the last col
                if (!((board == TinselBoardsPerBox - 2u) && (mailbox == TinselMailboxesPerBoard - 1u) && (col == (TinselCoresPerMailbox * COLSPERCORE) - 1u))) {

                    // Create forward key
                    fwdColumnKey[globalColumn] = progRouterMesh.addDestsFromBoardXY(boardPath[board][0u], boardPath[board][1u], &fwdDests);
                    //printf("Key: %X\n", fwdColumnKey[globalColumn]);
                    //fwdColumnKey[globalColumn] = 1u;
                
                }
                
                fwdDests.clear();
                
                // If we arent in the first col
                if (!((board == 0u) && (mailbox == 0u) && (col == 0u))) {

                    // Create forward key
                    bwdColumnKey[globalColumn] = progRouterMesh.addDestsFromBoardXY(boardPath[board][0u], boardPath[board][1u], &bwdDests);
                    printf("Key: %X\n", bwdColumnKey[globalColumn]);
                    //fwdColumnKey[globalColumn] = 1u;
                
                }
                
                bwdDests.clear();
                
                // Transition probabilites for same and different haplotypes for both observations
                
                float tau_m0 = 0u;
                float same0 = 0u;
                float diff0 = 0u;
                
                float* same0Ptr = &same0;
                float* diff0Ptr = &diff0;
                
                uint32_t* same0UPtr = (uint32_t*) same0Ptr;
                uint32_t* diff0UPtr = (uint32_t*) diff0Ptr;
                
                // Tau M Values
                if (globalColumn != 0u) {
                    
                    /*
                    // Caluclate total genetic distance
                    currentIndex = (observationPair - 1u) * LINRATIO;
                    geneticDistance = 0.0f;
                    for (uint32_t x = 0u; x < LINRATIO; x++) {
                        
                        geneticDistance += dm[currentIndex + x];
                        
                    }
                    */
                    
                    tau_m0 = (1 - exp((-4 * NE * dm[globalColumn - 1u]) / NOOFSTATES));
                    same0 = (1 - tau_m0) + (tau_m0 / NOOFSTATES);
                    diff0 = tau_m0 / NOOFSTATES;
                    
                    //printf("same trans prob = %.10f\n", *(float*)same0UPtr);
                    //printf("diff trans prob = %f\n", *(float*)diff0UPtr);
                    
                }
                
                /*
                // Caluclate total genetic distance
                currentIndex = (observationPair) * LINRATIO;
                geneticDistance = 0.0f;
                
                for (uint32_t x = 0u; x < LINRATIO; x++) {
                    
                    geneticDistance += dm[currentIndex + x];
                    
                }
                */
                
                float tau_m1 = 0u;
                float same1 = 0u;
                float diff1 = 0u;
                
                float* same1Ptr = &same1;
                float* diff1Ptr = &diff1;
                
                uint32_t* same1UPtr = (uint32_t*) same1Ptr;
                uint32_t* diff1UPtr = (uint32_t*) diff1Ptr;
                
                if ((globalColumn != (NOOFOBS - 1u))) {
                    
                    /*
                    // Caluclate total genetic distance
                    currentIndex = (globalColumn + 1u) * LINRATIO;
                    geneticDistance = 0.0f;
                    
                    for (uint32_t x = 0u; x < LINRATIO; x++) {
                        
                        geneticDistance += dm[currentIndex + x];
                        
                    }*/
                    
                    tau_m1 = (1 - exp((-4 * NE * dm[globalColumn] ) / NOOFSTATES));
                    same1 = (1 - tau_m1) + (tau_m1 / NOOFSTATES);
                    diff1 = tau_m1 / NOOFSTATES;
                    
                    //printf("same trans prob = %.10f\n", *(float*)same2UPtr);
                    //printf("diff trans prob = %f\n", *(float*)diff2UPtr);
                    
                }
                
                for (uint32_t thread = 0u; thread < THREADSPERCOL; thread++) {
                
                    uint32_t threadID = 0u;
                    
                    // Construct ThreadID
                    threadID = boardPath[board][1u];
                    threadID = (threadID << TinselMeshXBits) + boardPath[board][0u];
                    threadID = (threadID << TinselMailboxMeshYBits) + mailboxPath[mailbox][1u];
                    threadID = (threadID << TinselMailboxMeshXBits) + mailboxPath[mailbox][0u];
                    threadID = (threadID << TinselLogThreadsPerMailbox) + (col * 8u) + thread;
                    
                    uint8_t coreID = (mailboxPath[mailbox][1u] * TinselMailboxMeshXLen * TinselCoresPerMailbox) + (mailboxPath[mailbox][0u] * TinselCoresPerMailbox);
                    
                    // For debug store col number
                    uint32_t obsNo = globalColumn;

                    uint32_t baseAddress = tinselHeapBaseGeneric(threadID);
                    
                    hostLink.setAddr(boardPath[board][0u], boardPath[board][1u], coreID, baseAddress);
                    
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, &obsNo);
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, &fwdColumnKey[globalColumn]);
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, &bwdColumnKey[globalColumn]);
                    
                    // Backward transition propbabilties
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, same0UPtr);
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, diff0UPtr);
                    
                    // Forward transistion Probabilites
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, same1UPtr);
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, diff1UPtr);
                    
                
                }
                
            
            }
            
        }
    
    }
    
    // Write the keys to the routers
    progRouterMesh.write(&hostLink);
    
    // Load the correct code into the cores
    hostLink.boot("code.v", "data.v");
    hostLink.go();
    
    HostMessage msg;

    float result[NOOFOBS][8u];
     
    for (uint32_t y = 0u; y < 8u; y++) {
        for (uint32_t x = 0u; x < NOOFOBS; x++) {
        
            hostLink.recvMsg(&msg, sizeof(msg));
            
            result[msg.observationNo][msg.stateNo] = msg.val;
        
        }
    
    }
    
    for (uint32_t y = 0u; y < 8u; y++) {
        for (uint32_t x = 0u; x < NOOFOBS; x++) {
        
            
            printf("%e ", result[x][y]);
        
        }
        printf("\n");
    }
  
/*          
    for (uint32_t x = 0u; x < NOOFOBS; x++) {
        
        for (uint32_t y = 0u; y < 8u; y++) {

            printf("RETURNED -> Global Col: %d, Global Row: %d, Key: %X\n", x, y, result[x][y]);
            

        }

    }
*/

    //hostLink.recvMsg(&msg, sizeof(msg));
    //printf("ObsNo: %d, Key: %X \n", msg.observationNo, msg.stateNo);
    
#ifdef PRINTDEBUG   
    printf("\nKeys. The last key is not used:\n\n");
    
        for (uint32_t x = 0u; x < XKEYS; x++) {
            
            //if (x != XKEYS-1) {
                printf("Global Col: %d, Key: %X\n", x, fwdColumnKey[x]);
            //}
            //else {
            //    printf("%X\n", fwdColumnKey[x]);
            //}
            
        }
#endif
 
    //hostLink.recvMsg(&msg, sizeof(msg));

    //printf("ObsNo: %d, State: %d \n", msg.observationNo, msg.stateNo);

    return 0;
}
