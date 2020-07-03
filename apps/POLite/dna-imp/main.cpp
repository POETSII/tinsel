/*****************************************************
 * Genomic Imputation - Single Thread Version
 * ***************************************************
 * USAGE:
 * To Be Completed ...
 * 
 * PLEASE NOTE:
 * To Be Completed ...
 * 
 * ssh jordmorr@ayres.cl.cam.ac.uk
 * scp -r C:\Users\drjor\Documents\tinsel\apps\POLite\dna-imp jordmorr@ayres.cl.cam.ac.uk:~/tinsel/apps/POLite
 * scp jordmorr@ayres.cl.cam.ac.uk:~/tinsel/apps/POLite/dna-imp/results.csv C:\Users\drjor\Documents\tinsel\apps\POLite\dna-imp
 * ****************************************************/

#include "model.h"

#include <iostream>
#include <chrono>
#include <cassert>

int main()
{
    static float fwd[NOOFSTATES][NOOFOBS] = {0.0f};
    static float bwd[NOOFSTATES][NOOFOBS] = {0.0f};
    static float hmm[NOOFSTATES][NOOFOBS] = {0.0f};
    static int32_t i, j, k, l;
    static float prev_a_sum = 0.0f;
    static float p_fwd = 0.0f;
    static float p_bwd = 0.0f;
    
    // Record start time
    auto start = std::chrono::high_resolution_clock::now();
    
    // Forward part of the algorithm
    
    // For every observation
    for (i = 0; i < NOOFOBS; ++i) {
        
        for (j = 0; j < NOOFSTATES; ++j) {
            
            if (i == 0) {
                
                prev_a_sum = init_prob[j];
                
            }
            else {
                
                prev_a_sum = 0.0f;
                for (k = 0; k < NOOFSTATES; ++k) {
                    
                    prev_a_sum += fwd[k][i-1] * trans_prob[k][j];
                    
                }
                
            }
            
            fwd[j][i] = emis_prob[j][observation[i]] * prev_a_sum;
        }
        
    }
    
    // Backward part of the algorithm
    
    for (i = NOOFOBS-1; i >= 0; --i) {
     
        for (j = 0; j < NOOFSTATES; ++j) {
            
            if (i == NOOFOBS-1) {
                
                for (k = 0; k < NOOFSTATES; ++k) {
                    
                    bwd[j][i] = 1;
                    
                }
                
            }
            else {
                
                for (k = 0; k < NOOFSTATES; ++k) {
                
                    bwd[j][i] += trans_prob[j][k] * emis_prob[k][observation[i+1]] * bwd[k][i+1];
                
                }
                
            }
            
        }
   
    }
    
    // Calculate Posterior Probabiltiies
    for (i = 0; i < NOOFOBS; ++i) {
        
        for (j = 0; j < NOOFSTATES; ++j) {
            
            hmm[j][i] = fwd[j][i] * bwd[j][i];
            
        }
        
    }
            
    // Record end time
    auto finish = std::chrono::high_resolution_clock::now();        
 
    std::chrono::duration<double> elapsed = finish - start;
    
    /*

    std::cout << "Forward Algorithm:\n";

    for(i = 0; i < NOOFSTATES; ++i) {
        
        for(j = 0; j < NOOFOBS; ++j) {
            
            std::cout << fwd[i][j] << " ";
            
        }
        
        std::cout << "\n";
    }
    
    std::cout << "Backward Algorithm:\n";
    
    for(i = 0; i < NOOFSTATES; ++i) {
        
        for(j = 0; j < NOOFOBS; ++j) {
            
            std::cout << bwd[i][j] << " ";
            
        }
        
        std::cout << "\n";
    }
    
    std::cout << "Posterior Probabilities:\n";

    for(i = 0; i < NOOFSTATES; ++i) {
        
        for(j = 0; j < NOOFOBS; ++j) {
            
            std::cout << hmm[i][j] << " ";
            
        }
        
        std::cout << "\n";
    }
    
    std::cout << "P(O|lambda) = " << p_fwd << "\n";
    
    std::cout << "Time = " << elapsed.count() << "\n";
    
    */
     
    std::cout << elapsed.count();

    
    return 0;
}