#define TINSEL (1u)
#define PRINTDEBUG (1u)

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
 
const uint8_t ROWSPERCORE = 2u;
const uint32_t XKEYS = (TinselBoardsPerBox - 1u) * TinselCoresPerBoard * ROWSPERCORE;
//const uint8_t YKEYS = 8u;

int main()
{
    // Create the hostlink (with a single box dimension by default)
    HostLink hostLink;

    // Creat the programmable router mesh with single box dimensions
    ProgRouterMesh progRouterMesh(TinselMeshXLenWithinBox, TinselMeshYLenWithinBox);

    // Load the correct code into the cores
    hostLink.boot("code.v", "data.v");
    
    // Generate and transmit pre-execution data
    
    // Generate multicast keys
    
    // Array to store keys
    
    uint32_t fwdColumnKey[XKEYS] = {0u};
    uint32_t bwdColumnKey[XKEYS] = {0u};
    
    PRoutingDest dest;
    PRoutingDestMRM mrmRecord;
    Seq<PRoutingDest> fwdDests;
    //Seq<PRoutingDest> fwdDestsRow1;
    Seq<PRoutingDest> bwdDests;
    //Seq<PRoutingDest> bwdDestsRow1;
    
    dest.kind = PRDestKindMRM;
    
    mrmRecord.key = 0u;
    
    //const uint32_t row0 = 0x00FF00FFu;
    //const uint32_t row1 = 0xFF00FF00u;
    
    for (uint8_t board = 0u; board < (TinselBoardsPerBox - 1u); board++) {
    
        for (uint8_t mailbox = 0u; mailbox < TinselMailboxesPerBoard; mailbox++) {
            
            for (uint8_t row = 0u; row < (TinselCoresPerMailbox * ROWSPERCORE); row++) {
            
                // FORWARD KEYS
                
                // If we are not the last row in the mailbox
                if ( (row != ((TinselCoresPerMailbox * ROWSPERCORE) - 1u)) * (mailbox != TinselMailboxesPerBoard - 1u) ) {
                    
                    // Construct destination the mailbox
                    dest.mbox = boardPath[board][1u];
                    dest.mbox = (dest.mbox << TinselMeshXBits) + boardPath[board][0u];
                    dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxPath[mailbox][1u];
                    dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + mailboxPath[mailbox][0u];

                    // Construct destination threads
                    uint64_t threadMask = (0xFF);
                    mrmRecord.threadMaskLow = uint32_t(threadMask << row);
                    mrmRecord.threadMaskHigh = uint32_t((threadMask << row) >> 32u);
                    dest.mrm = mrmRecord;

                
                }
                else {
                    
                    // If we arent in the last row
                    if (!((board == TinselBoardsPerBox - 2u) && (mailbox == TinselMailboxesPerBoard - 1u) && (row == (TinselCoresPerMailbox * ROWSPERCORE) - 1u))) {
                        
                        // Construct destination the mailbox
                        dest.mbox = boardPath[board + 1u][1u];
                        dest.mbox = (dest.mbox << TinselMeshXBits) + boardPath[board + 1u][0u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshYBits) + mailboxPath[0u][1u];
                        dest.mbox = (dest.mbox << TinselMailboxMeshXBits) + mailboxPath[0u][0u];

                        // Construct destination threads
                        mrmRecord.threadMaskLow = 0xFF;
                        mrmRecord.threadMaskHigh = 0u;
                        dest.mrm = mrmRecord;
                        
                    }
                    
                }
                
                fwdDests.append(dest);
                
                // If we arent in the last row
                if (!((board == TinselBoardsPerBox - 2u) && (mailbox == TinselMailboxesPerBoard - 1u) && (row == (TinselCoresPerMailbox * ROWSPERCORE) - 1u))) {

                    // Create forward key
                    fwdColumnKey[(board * TinselCoresPerMailbox * ROWSPERCORE * TinselMailboxesPerBoard) + (mailbox * TinselCoresPerMailbox * ROWSPERCORE) + row] = progRouterMesh.addDestsFromBoardXY(boardPath[board][0u], boardPath[board][1u], &fwdDests);
                    //fwdColumnKey[(board * TinselCoresPerMailbox * ROWSPERCORE * TinselMailboxesPerBoard) + (mailbox * TinselCoresPerMailbox * ROWSPERCORE) + row] = 1u;
                
                }
                
                //printf("Row: ");
                
                //printf("%d\n", ((board * TinselCoresPerMailbox * ROWSPERCORE * TinselMailboxesPerBoard) + (mailbox * TinselCoresPerMailbox * ROWSPERCORE) + row));
                
                fwdDests.clear();
            
            }
            
        }
    
    }
    
    // Trigger threads

    /*
    printf("Starting\n");
    hostLink.go();

    printf("Sending ping\n");
    uint32_t ping[1 << TinselLogWordsPerMsg];
    ping[0] = 100;
    hostLink.send(0, 1, ping);

    printf("Waiting for response\n");
    hostLink.recv(ping);
    printf("Got response %x\n", ping[0]);
    */
    
    
#ifdef PRINTDEBUG    
    printf("\nKeys. The last key is not used:\n\n");
    
        for (uint32_t x = 0u; x < XKEYS; x++) {
            
            if (x != XKEYS-1) {
                printf("%X, ", fwdColumnKey[x]);
            }
            else {
                printf("%X\n", fwdColumnKey[x]);
            }
            
        }
#endif 

    return 0;
}
