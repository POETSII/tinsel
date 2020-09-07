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
 
const uint32_t XKEYS = (TinselBoardsPerBox - 1u) * TinselCoresPerBoard * NOOFHWCOLSPERCORE;
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
            
            for (uint8_t col = 0u; col < (TinselCoresPerMailbox * NOOFHWCOLSPERCORE); col++) {
                
                //Global Column Number
                uint32_t globalColumn = (board * TinselCoresPerMailbox * NOOFHWCOLSPERCORE * TinselMailboxesPerBoard) + (mailbox * TinselCoresPerMailbox * NOOFHWCOLSPERCORE) + col;
                
                //Base Thread
                uint32_t prevBaseThread = 0u;
            
                // FORWARD KEYS
                
                // If we are the last col on the current board
                if ( (col == ((TinselCoresPerMailbox * NOOFHWCOLSPERCORE) - 1u)) && (mailbox == TinselMailboxesPerBoard - 1u) ) {
                    
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
                else if ( col == ((TinselCoresPerMailbox * NOOFHWCOLSPERCORE) - 1u) ) {
                    
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
                        
                        prevBaseThread = (dest.mbox << TinselLogThreadsPerMailbox) + (7u * 8u);

                        // Construct destination threads
                        mrmRecord.threadMaskLow = 0x0;
                        mrmRecord.threadMaskHigh = 0xFF000000;
#ifdef PRINTDEBUG
                        printf("Global Col: %d, ", (globalColumn));
                        printf("boardX: %d, boardY: %d, mailboxX: %d, mailboxY: %d, col: %d, ThreadMaskLow: %X, ThreadMaskHigh: %X\n", boardPath[board - 1u][0u], boardPath[board - 1u][1u], mailboxPath[TinselCoresPerBoard - 1u][0u], mailboxPath[TinselCoresPerBoard - 1u][1u], col, mrmRecord.threadMaskLow, mrmRecord.threadMaskHigh);
#endif                        
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
                        
                        prevBaseThread = (dest.mbox << TinselLogThreadsPerMailbox) + (7u * 8u);
                        
                        // Construct destination threads
                        mrmRecord.threadMaskLow = 0x0;
                        mrmRecord.threadMaskHigh = 0xFF000000;
#ifdef PRINTDEBUG
                        printf("Global Col: %d, ", (globalColumn));
                        printf("boardX: %d, boardY: %d, mailboxX: %d, mailboxY: %d, col: %d, ThreadMaskLow: %X, ThreadMaskHigh: %X\n", boardPath[board][0u], boardPath[board][1u], mailboxPath[mailbox - 1u][0u], mailboxPath[mailbox - 1u][1u], col, mrmRecord.threadMaskLow, mrmRecord.threadMaskHigh);
#endif                        
                        dest.mrm = mrmRecord;
                    
                }
                // If we are just a regular col
                else {
                    
                        // Construct destination the same mailbox
                        dest.mbox = boardPath[board][1u];
                        dest.mbox = (dest.mbox << TinselMeshXBits) + boardPath[board][0u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxPath[mailbox][1u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + mailboxPath[mailbox][0u];
                        
                        prevBaseThread = (dest.mbox << TinselLogThreadsPerMailbox) + ((col - 1u) * 8u);
                        
                        // Construct destination threads for next col (hence + 8u)
                        uint64_t threadMask = 0xFF00000000000000;
                        mrmRecord.threadMaskLow = uint32_t(threadMask >> (((((TinselCoresPerMailbox * NOOFHWCOLSPERCORE)-1u) - col) + 1u) * 8u));
                        mrmRecord.threadMaskHigh = uint32_t((threadMask >> (((((TinselCoresPerMailbox * NOOFHWCOLSPERCORE)-1u) - col) + 1u) * 8u)) >> 32u);
#ifdef PRINTDEBUG
                        printf("Global Col: %d, ", (globalColumn));
                        printf("boardX: %d, boardY: %d, mailboxX: %d, mailboxY: %d, col: %d, ThreadMaskLow: %X, ThreadMaskHigh: %X\n", boardPath[board][0u], boardPath[board][1u], mailboxPath[mailbox][0u], mailboxPath[mailbox][1u], col, mrmRecord.threadMaskLow, mrmRecord.threadMaskHigh);
#endif                        
                        dest.mrm = mrmRecord;
                    
                }
                
                bwdDests.append(dest);
                
                
                // If we arent in the last col
                if (!((board == TinselBoardsPerBox - 2u) && (mailbox == TinselMailboxesPerBoard - 1u) && (col == (TinselCoresPerMailbox * NOOFHWCOLSPERCORE) - 1u))) {

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
#ifdef PRINTDEBUG
                    printf("Key: %X\n", bwdColumnKey[globalColumn]);
#endif
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
                    
                    // Caluclate total genetic distance
                    uint32_t currentIndex = (globalColumn - 1u) * LINRATIO;
                    float geneticDistance = 0.0f;
                    for (uint32_t x = 0u; x < LINRATIO; x++) {
                        
                        geneticDistance += dm[currentIndex + x];
                        
                    }
                    
                    tau_m0 = (1 - exp((-4 * NE * geneticDistance) / NOOFHWROWS));
                    same0 = (1 - tau_m0) + (tau_m0 / NOOFHWROWS);
                    diff0 = tau_m0 / NOOFHWROWS;
                    
                    
                }
                
                float tau_m1 = 0u;
                float same1 = 0u;
                float diff1 = 0u;
                
                float* same1Ptr = &same1;
                float* diff1Ptr = &diff1;
                
                uint32_t* same1UPtr = (uint32_t*) same1Ptr;
                uint32_t* diff1UPtr = (uint32_t*) diff1Ptr;
                
                if ((globalColumn != (NOOFTARGMARK - 1u))) {
                    
                    // Caluclate total genetic distance
                    uint32_t currentIndex = globalColumn * LINRATIO;
                    float geneticDistance = 0.0f;
                    for (uint32_t x = 0u; x < LINRATIO; x++) {
                        
                        geneticDistance += dm[currentIndex + x];
                        
                    }
                    
                    tau_m1 = (1 - exp((-4 * NE * geneticDistance ) / NOOFHWROWS));
                    same1 = (1 - tau_m1) + (tau_m1 / NOOFHWROWS);
                    diff1 = tau_m1 / NOOFHWROWS;
                    
                }
                
                for (uint32_t thread = 0u; thread < NOOFHWROWS; thread++) {
                    
                    //Create match value from model
                    
                    uint32_t match = 0u;
                    
                    // if markers match
                    if (hmm_labels[thread][globalColumn * LINRATIO] == observation[globalColumn * LINRATIO][1]) {
                        match = 1u;
                    }
                    
                    uint32_t prevThread = prevBaseThread + thread;
            
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
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, &match);
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, &fwdColumnKey[globalColumn]);
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, &bwdColumnKey[globalColumn]);
                    
                    // Forward transition propbabilties
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, same0UPtr);
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, diff0UPtr);
                    
                    // Backward transistion Probabilites
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, same1UPtr);
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, diff1UPtr);
                    
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, &prevThread);
                    
                    // Local genetic distances from map
                    float dmSingle = 0.0f;
                    float* dmPtr = &dmSingle;
                    uint32_t* dmUPtr = (uint32_t*) dmPtr;
                    
                    for (uint32_t x = 0u; x < LINRATIO; x++) {
                        
                        dmSingle = dm[(globalColumn * LINRATIO) + x];
                        hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, dmUPtr);
                        
                    }
                    
                
                }
                
            
            }
            
        }
    
    }
    
    // Write the keys to the routers
    progRouterMesh.write(&hostLink);
    
    printf("Launching\n");
    // Load the correct code into the cores
    hostLink.boot("code.v", "data.v");
    hostLink.go();
    
    HostMessage msg;

    float result[NOOFOBS][8u][2u] = {0.0f};
    uint32_t recCnt = 0u;
    
    //Create a file pointer
    FILE * fp1;
    /* open the file for writing*/
    fp1 = fopen ("resultsstream.csv","w");
    
    fprintf(fp1, "msgType,obsNo,stateNo,val\n");
    
    for (uint8_t msgType = 0u; msgType < 2; msgType++) {
        for (uint32_t y = 0u; y < 8u; y++) {
            for (uint32_t x = 0u; x < NOOFOBS; x++) {
                
                recCnt++;
                hostLink.recvMsg(&msg, sizeof(msg));
                
                if (msg.msgType < 2u) {
                    
                    result[msg.observationNo][msg.stateNo][msg.msgType] = msg.val;
                    fprintf(fp1, "%d,%d,%d,%e\n", msg.msgType, msg.observationNo, msg.stateNo, msg.val);
    
                }
                else {
                    if (msg.msgType == FWDLIN) {
                        result[msg.observationNo][msg.stateNo][0u] = msg.val;
                        fprintf(fp1, "0,%d,%d,%e\n", msg.observationNo, msg.stateNo, msg.val);
                    }
                    else {
                        result[msg.observationNo][msg.stateNo][1u] = msg.val;
                        fprintf(fp1, "0,%d,%d,%e\n", msg.observationNo, msg.stateNo, msg.val);
                    }
                }
            
            }
        
        }
    }
    
    fclose (fp1);
   
    //Create a file pointer
    FILE * fp;
    /* open the file for writing*/
    fp = fopen ("results.csv","w");

    fprintf(fp, "Forward Probabilities: \n");
    for (uint32_t y = 0u; y < 8u; y++) {
        for (uint32_t x = 0u; x < NOOFOBS; x++) {
        
            if (x != (NOOFOBS - 1u) ) {
                fprintf(fp, "%e,", result[x][y][0u]);
            }
            else {
                fprintf(fp, "%e", result[x][y][0u]);
            }
        
        }
        fprintf(fp, "\n");
    }

    fprintf(fp, "Backward Probabilities: \n");
    for (uint32_t y = 0u; y < 8u; y++) {
        for (uint32_t x = 0u; x < NOOFOBS; x++) {
        
            if (x != (NOOFOBS - 1u) ) {
                fprintf(fp, "%e,", result[x][y][1u]);
            }
            else {
                fprintf(fp, "%e", result[x][y][1u]);
            }
        
        }
        fprintf(fp, "\n");
    }

    /* close the file*/  
    fclose (fp);
  
/*          
    for (uint32_t x = 0u; x < NOOFTARGMARK; x++) {
        
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

    return 0;
}
