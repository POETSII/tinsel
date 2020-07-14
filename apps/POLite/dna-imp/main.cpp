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
#include <math.h>

float emission_prob(int32_t current_ob, uint8_t ref_label);

int main()
{
    static float fwd[NOOFSTATES][NOOFOBS] = {0.0f};
    static float bwd[NOOFSTATES][NOOFOBS] = {0.0f};
    static float hmm[NOOFSTATES][NOOFOBS] = {0.0f};
    static int32_t i, j, k, l;
    static float prev_a_sum = 0.0f;
    static float emis_prob = 0.0f;
    static float tau_m = 0.0f;
    static float allele_probs[NOOFOBS][2] = {0.0f};
    static float posterior_total = 0.0f;
    static float genetic_distance = 0.0f;
    
    // Record start time
    auto start = std::chrono::high_resolution_clock::now();
    
    // Forward part of the algorithm
    
    // For every observation in the target haplotype
    for (i = 0; i < NOOFTARGMARK; ++i) {
        
        //calculate genetic distance between target haplotype observations
        genetic_distance = 0.0f;
        
        for (k = (observation[i-1][0] + 1); k <= observation[i][0]; ++k) {
            
            genetic_distance += dm[k];
            
        }
        
        for (j = 0; j < NOOFSTATES; ++j) {
            
            if (observation[i][0] == 0) {
                
                prev_a_sum = 1.0f / NOOFSTATES;
                
            }
            else {
                
                tau_m = (1 - exp((-4 * NE * genetic_distance) / NOOFSTATES));
                prev_a_sum = 0.0f;
                
                for (k = 0; k < NOOFSTATES; ++k) {
                    
                    if (j != k) {
                        
                        prev_a_sum += fwd[k][observation[i-1][0]] * (tau_m / NOOFSTATES);
                    
                    }
                    else {
                        
                        prev_a_sum += fwd[k][observation[i-1][0]] * ((1 - tau_m) + (tau_m / NOOFSTATES));
                        
                    }
                    
                }
                
            }
            
            emis_prob = emission_prob(observation[i][0], hmm_labels[j][observation[i][0]]);
            fwd[j][observation[i][0]] = emis_prob * prev_a_sum;
            
        }
        
        if (i != 0) {
        
            // Calculate intermediate observations using linear interpolation
            for (k = (observation[i-1][0] + 1); k <= (observation[i][0] - 1); ++k) {
            
                for (l = 0; l < NOOFSTATES; ++l) {
                    
                    
                    //FOR DEBUG ONLY
                    //static float current_dm = 0.0f;
                    //static float last_hmm_alpha = 0.0f;
                    //static float curr_hmm_alpha = 0.0f;
                    
                    //static float distance_ratio = 0.0f;
                    //static float overall_dist = 0.0f;
                    
                    //current_dm = dm[k];
                    //last_hmm_alpha = fwd[l][observation[i-1][0]];
                    //curr_hmm_alpha = fwd[l][observation[i][0]];
                    //distance_ratio = current_dm / genetic_distance;
                    //overall_dist = last_hmm_alpha - curr_hmm_alpha;
                    
                    //fwd[l][k] = fwd[l][k-1] - ((distance_ratio) * (overall_dist));
                    
                    
                    fwd[l][k] = fwd[l][k-1] - ((dm[k] / genetic_distance) * (fwd[l][observation[i-1][0]] - fwd[l][observation[i][0]]));
                    
                }
                
            }
        
        }
        
    }
    
    
    // Backward part of the algorithm
    
    for (i = NOOFTARGMARK-1; i >= 0; --i) {
        
        
        if (i != NOOFTARGMARK-1) {
            
            //calculate genetic distance between target haplotype observations
            genetic_distance = 0.0f;
            
            for (k = (observation[i][0] + 1); k <= observation[i+1][0]; ++k) {
                
                genetic_distance += dm[k];
                
            }
        
        }
     
        for (j = 0; j < NOOFSTATES; ++j) {
            
            if (i == NOOFTARGMARK-1) {
                
                for (k = 0; k < NOOFSTATES; ++k) {
                    
                    bwd[j][observation[i][0]] = 1;
                    
                }
                
            }
            else {
                
                tau_m = (1 - exp((-4 * NE * genetic_distance) / NOOFSTATES));
                
                for (k = 0; k < NOOFSTATES; ++k) {
                    
                    emis_prob = emission_prob(observation[i+1][0], hmm_labels[k][observation[i+1][0]]);
                    
                    if (j != k) {
                        
                        bwd[j][observation[i][0]] += (tau_m / NOOFSTATES) * emis_prob * bwd[k][observation[i+1][0]];
                    
                    }
                    else {
                        
                        bwd[j][observation[i][0]] += ((1 - tau_m) + (tau_m / NOOFSTATES)) * emis_prob * bwd[k][observation[i+1][0]];
                        
                    }
                
                    
                
                }
                
            }
            
        }
        
        if (i != NOOFTARGMARK-1) {
        
            // Calculate intermediate observations using linear interpolation
            for (k = (observation[i+1][0] - 1); k >= (observation[i][0] + 1); --k) {
            
                for (l = 0; l < NOOFSTATES; ++l) {
                    

                    //FOR DEBUG ONLY
                    //static float current_dm = 0.0f;
                    //static float last_hmm_alpha = 0.0f;
                    //static float curr_hmm_alpha = 0.0f;
                    
                    //static float distance_ratio = 0.0f;
                    //static float overall_dist = 0.0f;
                    
                    //current_dm = dm[k];
                    //last_hmm_alpha = bwd[l][observation[i+1][0]];
                    //curr_hmm_alpha = bwd[l][observation[i][0]];
                    //distance_ratio = current_dm / genetic_distance;
                    //overall_dist = last_hmm_alpha - curr_hmm_alpha;
                    
                    //bwd[l][k] = bwd[l][k+1] - ((distance_ratio) * (overall_dist));
 
                    bwd[l][k] = bwd[l][k+1] - ((dm[k] / genetic_distance) * (bwd[l][observation[i+1][0]] - bwd[l][observation[i][0]]));
                    
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
    
    // Calculate allele probabilities
    for (i = 0; i < NOOFOBS; ++i) {
        
        posterior_total = 0.0f;
        
        for (j = 0; j < NOOFSTATES; ++j) {
            
            posterior_total += hmm[j][i];
            
        }
        
        for (j = 0; j < NOOFSTATES; ++j) {
            
            if (hmm_labels[j][i] == 0u) {
                
                allele_probs[i][0] += hmm[j][i];
                
            }
            else {
                
                allele_probs[i][1] += hmm[j][i];
                
            }
            
        }
        
        allele_probs[i][0] = allele_probs[i][0] / posterior_total;
        allele_probs[i][1] = allele_probs[i][1] / posterior_total;
        
    }
            
    // Record end time
    auto finish = std::chrono::high_resolution_clock::now();        
 
    std::chrono::duration<double> elapsed = finish - start;
  
#ifdef PRINTDIAG

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
    
    std::cout << "Allele Probabilities:\n";
    
    std::cout << "Maj:";
    
    for(j = 0; j < NOOFOBS; ++j) {
            
        std::cout << allele_probs[j][0] << " | ";
            
    }
    
    std::cout << "\n";
    
    std::cout << "Min:";
    
    for(j = 0; j < NOOFOBS; ++j) {
            
        std::cout << allele_probs[j][1] << " | ";
            
    }
    
    std::cout << "\n";
    
    std::cout << "Time = " << elapsed.count() << "\n";


#else
     
    std::cout << elapsed.count();
    
#endif
    
    return 0;
}

float emission_prob(int32_t current_ob, uint8_t ref_label) {
    
    uint32_t i;
    float emis_prob = 1.0f;
    
    for(i = 0; i < NOOFTARGMARK; ++i) {
        
        if (observation[i][0] == uint32_t(current_ob)) {
            
            if (observation[i][1] == uint32_t(ref_label)) {
                
                emis_prob = 1.0f - (1.0f / ERRORRATE);
                
            }
            else {
                emis_prob = 1.0f / ERRORRATE;
            }
            
        }

    }
    
    return emis_prob;
    
}