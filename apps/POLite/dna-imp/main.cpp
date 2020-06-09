#include "model.h"

#include <iostream>
#include <chrono>

int main()
{
    
    static float hmm[NOOFSTATES][NOOFOBS] = {0.0f};
    static uint32_t i, j, k, l;
    static float prev_a_sum = 0.0f;
    static float p_fwd = 0.0f;
    
    // Record start time
    auto start = std::chrono::high_resolution_clock::now();
    
    // For every observation
    for(i = 0; i < NOOFOBS; ++i) {
        
        for(j = 0; j < NOOFSTATES; ++j) {
            
            if (i == 0) {
                prev_a_sum = init_prob[j];
            }
            else {
                
                prev_a_sum = 0.0f;
                for (k = 0; k < NOOFSTATES; ++k) {
                    prev_a_sum += hmm[k][i-1] * trans_prob[k][j];
                }
            }
            
            hmm[j][i] = emis_prob[j][observation[i]] * prev_a_sum;
        }
    }
    
    for(l = 0; l < NOOFSTATES; ++l) {
        p_fwd += hmm[l][NOOFOBS-1];
    }
            
    // Record end time
    auto finish = std::chrono::high_resolution_clock::now();        
 
    std::chrono::duration<double> elapsed = finish - start;
    /*
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