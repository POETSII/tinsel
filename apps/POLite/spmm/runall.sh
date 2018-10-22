make all
for i in {2..40}; do 
  ./build/run 6 11 $i --benchmark_repetitions=5
done
