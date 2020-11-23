#define TINSEL (1u)
//#define PRINTDEBUG (1u)

#include "model.h"
#include <HostLink.h>
#include <math.h>
#include "../../include/POLite/ProgRouters.h"

//#define DEBUGRETURNS (1)

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
 * ssh jordmorr@byron.cl.cam.ac.uk
 * scp -r C:\Users\drjor\Documents\tinsel\apps\imputationhomog jordmorr@byron.cl.cam.ac.uk:~/tinsel/apps
 * scp jordmorr@byron.cl.cam.ac.uk:~/tinsel/apps/imputationhomog/results.csv C:\Users\drjor\Documents\tinsel\apps\imputationhomog
 * ****************************************************/
 
const uint32_t XKEYS = NOOFBOXES * (TinselBoardsPerBox - 1u) * TinselCoresPerBoard * NOOFHWCOLSPERCORE;
//const uint8_t YKEYS = 8u;

int main()
{
    // Create the hostlink (with a single box dimension by default)
    HostLink hostLink(2, 4);

    // Create the programmable router mesh with single box dimensions
    ProgRouterMesh progRouterMesh(6, 8);
    
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
	
    // Start Initialisation Timer
    struct timeval start_init, finish_init, diff_init;
    gettimeofday(&start_init, NULL);
    
    /********************************************************
     * HARDWARE LAYER SETUP
     * *****************************************************/
    
    for (uint8_t board = 0u; board < (NOOFBOXES * (TinselBoardsPerBox - 1u)); board++) {
    
        for (uint8_t mailbox = 0u; mailbox < TinselMailboxesPerBoard; mailbox++) {
            
            for (uint8_t col = 0u; col < (TinselCoresPerMailbox * NOOFHWCOLSPERCORE); col++) {
                
                //Global Column Number
                uint32_t HWColumn = (board * TinselCoresPerMailbox * NOOFHWCOLSPERCORE * TinselMailboxesPerBoard) + (mailbox * TinselCoresPerMailbox * NOOFHWCOLSPERCORE) + col;
                
                //printf("Global Col: %d Board: %d Mailbox: %d\n", HWColumn, board, mailbox);
                
                //Base Thread
                uint32_t prevBaseThread = 0u;
            
                // FORWARD KEYS
                
                // If we are the last col on the current board
                if ( (col == ((TinselCoresPerMailbox * NOOFHWCOLSPERCORE) - 1u)) && (mailbox == TinselMailboxesPerBoard - 1u) ) {
                    
                    // If we arent in the last col for the setup
                    if ( !(board == ((NOOFBOXES * (TinselBoardsPerBox - 1u)) - 1u)) ) {
                        
                        // Construct destination the mailbox for the next board
                        dest.mbox = boardPath[board + 1u][1u];
                        dest.mbox = (dest.mbox << TinselMeshXBits) + boardPath[board + 1u][0u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxPath[0u][1u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + mailboxPath[0u][0u];

                        // Construct destination threads
                        mrmRecord.threadMaskLow = 0xFF;
                        mrmRecord.threadMaskHigh = 0u;
                        //printf("Global Col: %d, ", (HWColumn));
                        //printf("boardX: %d, boardY: %d, mailboxX: %d, mailboxY: %d, col: %d, ThreadMaskLow: %X, ThreadMaskHigh: %X\n", boardPath[board + 1u][0u], boardPath[board + 1u][1u], mailboxPath[0u][0u], mailboxPath[0u][1u], col, mrmRecord.threadMaskLow, mrmRecord.threadMaskHigh);
                        dest.mrm = mrmRecord;
                        
                    }
                    // If we are the last col for the hw setup, loop back to the beginning
                    else {
                        // Construct destination the mailbox for the next board
                        dest.mbox = boardPath[0u][1u];
                        dest.mbox = (dest.mbox << TinselMeshXBits) + boardPath[0u][0u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxPath[0u][1u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + mailboxPath[0u][0u];

                        // Construct destination threads
                        mrmRecord.threadMaskLow = 0xFF;
                        mrmRecord.threadMaskHigh = 0u;
                        //printf("Global Col: %d, ", (HWColumn));
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
                        //printf("Global Col: %d, ", (HWColumn));
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
                        //printf("Global Col: %d, ", (HWColumn));
                        //printf("boardX: %d, boardY: %d, mailboxX: %d, mailboxY: %d, col: %d, ThreadMaskLow: %X, ThreadMaskHigh: %X\n", boardPath[board][0u], boardPath[board][1u], mailboxPath[mailbox][0u], mailboxPath[mailbox][1u], col, mrmRecord.threadMaskLow, mrmRecord.threadMaskHigh);
                        dest.mrm = mrmRecord;
                    
                }
                
                fwdDests.append(dest);
                
                // BACKWARD KEYS
                
                // If we are the first col on the current board
                if ( (col == 0u) && (mailbox == 0u) ) {
                    
                    // If we arent in the first col for the hw setup
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
                        printf("Global Col: %d, ", (HWColumn));
                        printf("boardX: %d, boardY: %d, mailboxX: %d, mailboxY: %d, col: %d, ThreadMaskLow: %X, ThreadMaskHigh: %X\n", boardPath[board - 1u][0u], boardPath[board - 1u][1u], mailboxPath[TinselCoresPerBoard - 1u][0u], mailboxPath[TinselCoresPerBoard - 1u][1u], col, mrmRecord.threadMaskLow, mrmRecord.threadMaskHigh);
#endif                        
                        dest.mrm = mrmRecord;
                        
                    }
                    // If we are the first col for the hw setup, loop back around to the last col
                    else {
                        
                        // Construct destination the mailbox for the last board
                        dest.mbox = boardPath[((TinselBoardsPerBox - 1u) * NOOFBOXES) - 1u][1u];
                        dest.mbox = (dest.mbox << TinselMeshXBits) + boardPath[((TinselBoardsPerBox - 1u) * NOOFBOXES) - 1u][0u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxPath[TinselMailboxesPerBoard - 1u][1u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + mailboxPath[TinselMailboxesPerBoard - 1u][0u];
                        
                        prevBaseThread = (dest.mbox << TinselLogThreadsPerMailbox) + (7u * 8u);

                        // Construct destination threads
                        mrmRecord.threadMaskLow = 0x0;
                        mrmRecord.threadMaskHigh = 0xFF000000;
#ifdef PRINTDEBUG
                        printf("Global Col: %d, ", (HWColumn));
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
                        printf("Global Col: %d, ", (HWColumn));
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
                        printf("Global Col: %d, ", (HWColumn));
                        printf("boardX: %d, boardY: %d, mailboxX: %d, mailboxY: %d, col: %d, ThreadMaskLow: %X, ThreadMaskHigh: %X\n", boardPath[board][0u], boardPath[board][1u], mailboxPath[mailbox][0u], mailboxPath[mailbox][1u], col, mrmRecord.threadMaskLow, mrmRecord.threadMaskHigh);
#endif                        
                        dest.mrm = mrmRecord;
                    
                }
                
                bwdDests.append(dest);
                
                  
                // Create forward key
                fwdColumnKey[HWColumn] = progRouterMesh.addDestsFromBoardXY(boardPath[board][0u], boardPath[board][1u], &fwdDests);
                //printf("Key: %X\n", fwdColumnKey[HWColumn]);
                //fwdColumnKey[HWColumn] = 1u;
                
                
                fwdDests.clear();
                

                // Create forward key
                bwdColumnKey[HWColumn] = progRouterMesh.addDestsFromBoardXY(boardPath[board][0u], boardPath[board][1u], &bwdDests);
#ifdef PRINTDEBUG
                printf("Key: %X\n", bwdColumnKey[HWColumn]);
#endif
                //fwdColumnKey[HWColumn] = 1u;
                

                
                bwdDests.clear();
                
                for (uint32_t thread = 0u; thread < NOOFHWROWS; thread++) {
                
                    uint32_t threadID = 0u;
                        
                    // Construct ThreadID
                    threadID = boardPath[board][1u];
                    threadID = (threadID << TinselMeshXBits) + boardPath[board][0u];
                    threadID = (threadID << TinselMailboxMeshYBits) + mailboxPath[mailbox][1u];
                    threadID = (threadID << TinselMailboxMeshXBits) + mailboxPath[mailbox][0u];
                    threadID = (threadID << TinselLogThreadsPerMailbox) + (col * 8u) + thread;
                    
                    uint8_t coreID = (mailboxPath[mailbox][1u] * TinselMailboxMeshXLen * TinselCoresPerMailbox) + (mailboxPath[mailbox][0u] * TinselCoresPerMailbox);

                    uint32_t baseAddress = tinselHeapBaseGeneric(threadID);
                    
                    hostLink.setAddr(boardPath[board][0u], boardPath[board][1u], coreID, baseAddress);
                    
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, &fwdColumnKey[HWColumn]);
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, &bwdColumnKey[HWColumn]);
                    
                    uint32_t prevThread = prevBaseThread + thread;
                    
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, &prevThread);
                    
                    // For debug store col number
                    //uint32_t obsNo = HWColumn;
                    
                    hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, &HWColumn);
                
                }
                
            }
            
        }
    
    }
    
    /********************************************************
     * HARDWARE ABSTRACTION LAYER SETUP
     * *****************************************************/
    
    for (uint32_t leg = 0u; leg < NOOFLEGS; leg++) {
        
        uint32_t legOffset = leg * NOOFHWCOLS;
    
        for (uint8_t board = 0u; board < (NOOFBOXES * (TinselBoardsPerBox - 1u)); board++) {
        
            for (uint8_t mailbox = 0u; mailbox < TinselMailboxesPerBoard; mailbox++) {
                
                for (uint8_t col = 0u; col < (TinselCoresPerMailbox * NOOFHWCOLSPERCORE); col++) {
                    
                    //Global Column Number
                    uint32_t globalColumn = ((board * TinselCoresPerMailbox * NOOFHWCOLSPERCORE * TinselMailboxesPerBoard) + (mailbox * TinselCoresPerMailbox * NOOFHWCOLSPERCORE) + col) + legOffset;
                    
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
                        
                        tau_m0 = (1 - exp((-4 * NE * geneticDistance) / NOOFSTATES));
                        same0 = (1 - tau_m0) + (tau_m0 / NOOFSTATES);
                        diff0 = tau_m0 / NOOFSTATES;
                        
                        
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
                        
                        tau_m1 = (1 - exp((-4 * NE * geneticDistance ) / NOOFSTATES));
                        same1 = (1 - tau_m1) + (tau_m1 / NOOFSTATES);
                        diff1 = tau_m1 / NOOFSTATES;
                        
                    }
                    
                    for (uint32_t thread = 0u; thread < NOOFHWROWS; thread++) {
                        
                        //Create match value from model
                        
                        uint32_t match[NOOFSTATEPANELS] = {0u};
                        
                        for (uint32_t x = 0u; x < NOOFSTATEPANELS; x++) {
                        
                            // if markers match
                            if (hmm_labels[(x * NOOFHWROWS) + thread][globalColumn * LINRATIO] == observation[globalColumn * LINRATIO][1]) {
                                match[x] = 1u;
                            }
                        
                        }
                
                        uint32_t threadID = 0u;
                        
                        // Construct ThreadID
                        threadID = boardPath[board][1u];
                        threadID = (threadID << TinselMeshXBits) + boardPath[board][0u];
                        threadID = (threadID << TinselMailboxMeshYBits) + mailboxPath[mailbox][1u];
                        threadID = (threadID << TinselMailboxMeshXBits) + mailboxPath[mailbox][0u];
                        threadID = (threadID << TinselLogThreadsPerMailbox) + (col * 8u) + thread;
                        
                        uint8_t coreID = (mailboxPath[mailbox][1u] * TinselMailboxMeshXLen * TinselCoresPerMailbox) + (mailboxPath[mailbox][0u] * TinselCoresPerMailbox);

                        uint32_t baseAddress = tinselHeapBaseGeneric(threadID);
                        
                        // Increment base address to account for data stored in previous legs and data stored in previous for loops 
                        baseAddress += ( ( ( leg * ( 4u + NOOFSTATEPANELS + LINRATIO ) ) * sizeof(uint32_t) )  + (4u * sizeof(uint32_t) ) );
                        //baseAddress += (4u * sizeof(uint32_t));
                        
                        hostLink.setAddr(boardPath[board][0u], boardPath[board][1u], coreID, baseAddress);
                        
                        // Forward transition propbabilties
                        hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, same0UPtr);
                        hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, diff0UPtr);
                        
                        // Backward transistion Probabilites
                        hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, same1UPtr);
                        hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, diff1UPtr);
                        
                        for (uint32_t x = 0u; x < NOOFSTATEPANELS; x++) {
                        
                            hostLink.store(boardPath[board][0u], boardPath[board][1u], coreID, 1u, &match[x]);
                        
                        }
                        
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
    
    }
    
    /********************************************************
     * WRITE MESH AND START GRAPH
     * *****************************************************/
    
    // Write the keys to the routers
    progRouterMesh.write(&hostLink);
    
    // Record init time
    gettimeofday(&finish_init, NULL);
    timersub(&finish_init, &start_init, &diff_init);
    double init_duration = (double) diff_init.tv_sec + (double) diff_init.tv_usec / 1000000.0;
    
    printf("Init Time = %0.5f\n", init_duration);
    printf("Launching\n");
    // Load the correct code into the cores
    hostLink.boot("code.v", "data.v");
    hostLink.go();
    
    // Start Processing Timer
    struct timeval start_proc, finish_proc, diff_proc;
    gettimeofday(&start_proc, NULL);
    
    /*
    HostMessage msg;
    float testResults[8u] = {0.0f};
    
    for (uint32_t i = 0u; i < 8u; i++) {
        
        hostLink.recvMsg(&msg, sizeof(msg));
        testResults[msg.stateNo] = msg.val;
        
    }

    for (uint32_t i = 0u; i < 8u; i++) {
        printf("%.15f\n", testResults[i]);
    }
    */

    /********************************************************
     * RECEIVE RESULTS
     * *****************************************************/
    
    HostMessage msg;
    
    static float result[NOOFOBS][NOOFSTATES][2u];
    
    for (uint8_t msgType = 0u; msgType < 2; msgType++) {
        for (uint32_t y = 0u; y < NOOFSTATES; y++) {
            for (uint32_t x = 0u; x < NOOFOBS; x++) {
                
                result[x][y][msgType] = 0.0f;
                
            }
        }
    }
    
    uint32_t recCnt = 0u;
    uint32_t lowerCnt = 0u;
    uint64_t upperCnt = 0u;
    double procTime = 0.0f;
    
    //Create a file pointer
    FILE * fp1;
    // open the file for writing
    fp1 = fopen ("resultsstream.csv","w");
    
    fprintf(fp1, "msgType,obsNo,stateNo,val\n");
    
    uint32_t expectedMessages = (NOOFTARGMARK * NOOFSTATES * 2u * NOOFTARGHAPLOTYPES) + 1u;
    //uint32_t expectedMessages = 1u;
    
    for (uint32_t recMsg = 0u; recMsg < expectedMessages; recMsg++) {
                
        recCnt++;
        hostLink.recvMsg(&msg, sizeof(msg));
        
        
        //if (recCnt % 10000u == 0u) {
            printf("%d\n", recCnt);
        //}

        
        if (msg.msgType < 2u) {
            
            result[msg.observationNo][msg.stateNo][msg.msgType] = msg.val;
            fprintf(fp1, "%d,%d,%d,%e\n", msg.msgType, msg.observationNo, msg.stateNo, msg.val);

        }
        else {
            if (msg.msgType == FWDLIN) {
                result[msg.observationNo][msg.stateNo][0u] = msg.val;
                fprintf(fp1, "0,%d,%d,%e\n", msg.observationNo, msg.stateNo, msg.val);
            }
            else if (msg.msgType == BWDLIN) {
                result[msg.observationNo][msg.stateNo][1u] = msg.val;
                fprintf(fp1, "1,%d,%d,%e\n", msg.observationNo, msg.stateNo, msg.val);
            }
            else {
                lowerCnt = msg.observationNo;
                upperCnt = msg.stateNo;
                
                procTime = ((upperCnt << 32) + lowerCnt) / (TinselClockFreq * 1000000.0);
            }
        }
        
                
#ifdef DEBUGRETURNS  

        if (recCnt > 111982000u) {
        
            // Temporary file write
            //Create a file pointer
            FILE * fp;
            // open the file for writing
            fp = fopen ("results.csv","w");
            
            uint32_t obsStart = 60000u;
            uint32_t obsEnd = 65000u;

            fprintf(fp, "Forward Probabilities: \n");
            for (uint32_t y = 0u; y < NOOFSTATES; y++) {
                for (uint32_t x = obsStart; x < obsEnd; x++) {
                
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
            for (uint32_t y = 0u; y < NOOFSTATES; y++) {
                for (uint32_t x = obsStart; x < obsEnd; x++) {
                
                    if (x != (NOOFOBS - 1u) ) {
                        fprintf(fp, "%e,", result[(NOOFOBS - 1) - x][y][1u]);
                    }
                    else {
                        fprintf(fp, "%e", result[(NOOFOBS - 1) - x][y][1u]);
                    }
                
                }
                fprintf(fp, "\n");
            }
            
            // close the file  
            fclose (fp);
            
            printf("File written\n");
            
        }
#endif            
    
    }
    
    // Record init time
    gettimeofday(&finish_proc, NULL);
    timersub(&finish_proc, &start_proc, &diff_proc);
    double proc_duration = (double) diff_proc.tv_sec + (double) diff_proc.tv_usec / 1000000.0;
    printf("Wall Proc Time = %0.5f\n", proc_duration);
    printf("Cycle Proc Time = %.15f\n", procTime);
    
    fclose (fp1);
    
    //Create a file pointer
    FILE * fp;
    // open the file for writing
    fp = fopen ("results.csv","w");

    fprintf(fp, "Forward Probabilities: \n");
    for (uint32_t y = 0u; y < NOOFSTATES; y++) {
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
    for (uint32_t y = 0u; y < NOOFSTATES; y++) {
        for (uint32_t x = 0u; x < NOOFOBS; x++) {
        
            if (x != (NOOFOBS - 1u) ) {
                fprintf(fp, "%e,", result[(NOOFOBS - 1) - x][y][1u]);
            }
            else {
                fprintf(fp, "%e", result[(NOOFOBS - 1) - x][y][1u]);
            }
        
        }
        fprintf(fp, "\n");
    }

    // close the file 
    fclose (fp);
    

#ifdef PRINTKEYS   
    printf("\nKeys. The last key is not used:\n\n");
    
        for (uint32_t x = 0u; x < XKEYS; x++) {
            
            //if (x != XKEYS-1) {
                printf("Global Col: %d, Key: %X\n", x, bwdColumnKey[x]);
            //}
            //else {
            //    printf("%X\n", fwdColumnKey[x]);
            //}
            
        }
#endif
    
    return 0;
}
