#include "matrices.h"

#include <iostream>
#include <chrono>

int main()
{
    static int matrixRes[MATALEN][MATAWID], i, j, k;

    // Initializing elements of matrix mult to 0.
    for(i = 0; i < MATALEN; ++i)
        for(j = 0; j < MATAWID; ++j)
        {
            matrixRes[i][j]=0;
        }
        
    // Record start time
    auto start = std::chrono::high_resolution_clock::now();

    // Multiplying matrix a and b and storing in array mult.
    for(i = 0; i < MATALEN; ++i)
        for(j = 0; j < MATBWID; ++j)
            for(k = 0; k < MATAWID; ++k)
            {
                matrixRes[i][j] += matrixA[i][k] * matrixB[k][j];
            }
            
    // Record end time
    auto finish = std::chrono::high_resolution_clock::now();        
            
    // Displaying the multiplication of two matrix.
    for(i = 0; i < MATALEN; ++i)
        for(j = 0; j < MATBWID; ++j)
        {
            std::cout << matrixRes[i][j] << ' ';
            if(j == MATBWID - 1)
                std::cout << std::endl;
        }
 
    std::chrono::duration<double> elapsed = finish - start;

    std::cout << elapsed.count();

    return 0;
}