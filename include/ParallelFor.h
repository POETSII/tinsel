#ifndef polite_parallel_for_hpp
#define polite_parallel_for_hpp

#include <cstdint>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range2d.h>

#ifdef POLITE_NO_PARFOR
const bool POLite_AllowParallelFor=false;
#else
const bool POLite_AllowParallelFor=true;
#endif

enum struct ParallelFlag
{
  Yes=+1,
  Default=0,
  No=-1
};

template<class TR=unsigned, class TF>
void parallel_for_with_grain(ParallelFlag flag, TR begin, TR end, TR grain_size, TF f)
{
  auto n=end-begin;
  if(n==0){
    return;
  }
  if(n >= 2*grain_size && POLite_AllowParallelFor && flag >= ParallelFlag::Default){
    tbb::parallel_for<tbb::blocked_range<TR>>( {begin, end, grain_size},  [&](const tbb::blocked_range<TR> &r){
      for(TR i=r.begin(); i<r.end(); i++){
        f(i);
      }
    });
  }else{
    #ifndef NDEBUG
    TR off=begin + rand()%(end-begin);
    for(TR i=begin; i<end; i++){
      f(off);
      ++off;
      if(off==end){
        off=begin;
      }
    }  
    #else
    for(TR i=begin; i<end; i++){
      f(i);
    }
    #endif
  }
}

template<class TR=unsigned, class TF>
void parallel_for_with_grain(TR begin, TR end, TR grain_size, TF f)
{ parallel_for_with_grain(ParallelFlag::Default, begin, end, grain_size, f); }

template<class TR=unsigned, class TF>
void parallel_for_blocked(ParallelFlag flag, TR begin, TR end, TR grain_size, TF f)
{
  auto n=end-begin;
  if(n==0){
    return;
  }
  if(n >= 2*grain_size && POLite_AllowParallelFor && flag>=ParallelFlag::Default){
    tbb::parallel_for<tbb::blocked_range<TR>>( {begin, end, grain_size},  [&](const tbb::blocked_range<TR> &r){
      f(r.begin(), r.end());
    });
  }else{
    #ifndef NDEBUG
    TR done=0;
    TR off=(begin+19937)%n;
    while(done < n){
      TR todo=n-done;
      todo=std::min<TR>(grain_size, std::min<TR>(todo, n-off));
      assert(todo>0);
      f(off, off+todo);
      done+=todo;
      off+=todo;
      if(off==n){
        off=0;
      }
    }
    assert(done==n);
    #else
    f(begin, end);
    #endif
  }
}

template<class TR=unsigned, class TF>
void parallel_for_blocked(TR begin, TR end, TR grain_size, TF f)
{
  parallel_for_blocked(ParallelFlag::Default, begin, end, grain_size, f);
}

template<class TR=unsigned, class TF>
void parallel_for_2d_with_grain(ParallelFlag flag, TR begin0, TR end0, TR grain_size0, TR begin1, TR end1, TR grain_size1, TF f)
{
  auto n0=end0-begin0;
  auto n1=end1-begin1;
  if(n1==0 || n0==0){
    return;
  }
  if(n1 >= 2*grain_size1 && n0 >= 2*grain_size0 && POLite_AllowParallelFor && flag >= ParallelFlag::Default){
    tbb::parallel_for<tbb::blocked_range2d<TR>>( {begin0, end0, grain_size0, begin1, end1, grain_size1},  [&](const tbb::blocked_range2d<TR> &r){
      for(TR i=r.rows().begin(); i<r.rows().end(); i++){
        for(TR j=r.cols().begin(); j<r.cols().end(); j++){
          f(i, j);
        }
      }
    });
  }else{
    #ifndef NDEBUG
    TR off0=begin0 + rand()%(end0-begin0);
    for(TR i=begin0; i<end0; i++){
      TR off1=begin1 + rand()%(end1-begin1);
      for(TR i=begin0; i<end0; i++){
        f(off0, off1);
        ++off1;
        if(off1==end1){
          off1=begin1;
        }
      }
      ++off0;
      if(off0==end0){
        off0=begin0;
      }
    }  
    #else
    for(TR i=begin0; i<end0; i++){
      for(TR j=begin1; j<end1; j++){
        f(i,j);
      }
    }
    #endif
  }
}

template<class TR=unsigned, class TF>
void parallel_for_2d_with_grain(TR begin0, TR end0, TR grain_size0, TR begin1, TR end1, TR grain_size1, TF f)
{
  parallel_for_2d_with_grain(ParallelFlag::Default, begin0, end0, grain_size0, begin1, end1, grain_size1, f);
}

#endif

