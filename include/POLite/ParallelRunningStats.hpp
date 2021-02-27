#ifndef parallel_running_stats_hpp
#define parallel_running_stats_hpp

#include <cstdint>
#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cfloat>

#include <quadmath.h>

/*! Maintains an array of N double running stats.
    The intent is to allow for fast heirarchical stats, e.g. in the following form:

    \code{.cpp}
    void f()
    {
        
        ParallelRunningStats<3> root;
        parallel_for_blocked(0, n, [](unsigned begin, unsigned end){
            ParallelRunningStats<r> child;
            for(unsigned i=begin; i<end; i++){
                child += { x1, x2, x3 };
            }
            root += child; // Parallel merge
        });
        // Do something with stats
        root.walk_stats({"x1","x2","x3"},[](const char *name, const char *stat, double){
            printf("%s,%s,%g\n", name, stat, double);
        });
    }
    \endcode

    This is mainly intended for sequences of roughly similar magnitude non-negative numbers.
    If the sequence is too wild then cancellation may result in an inaccurate mean or
    an erroneously. Even mild cancellation over large numbers may lead to erroneous
    skewness and kurtosis, so take them with a pinch of salt.

    The skewness and kurtosis come from the definitions in boost::accumulator::skewness_impl and   boost::accumulator::kurtosis_impl .

    If it ever becomes a problem, there are much more robust methods for calculating
    online skewness and kurtosis (and the variance), e.g.:
    https://www.johndcook.com/blog/skewness_kurtosis/
*/
template<unsigned N=1>
struct ParallelRunningStats
{
private:
    struct stats_t
    {
        double m_min[N];
        double m_max[N];
        double m_sums[N][5]; // The count is just m_sums[0]

        void operator+=(const std::array<double,N> &xs)
        {
            for(unsigned i=0; i<N; i++){
                double x=xs[i];
                m_min[i]=std::min(m_min[i], x);
                m_max[i]=std::max(m_max[i], x);
                double acc=1;
                m_sums[i][0] += acc;
                for(unsigned j=1; j<=4; j++){
                    acc *= x;
                    m_sums[i][j] += acc;
                }                    
            }
        }

        void operator+=(const stats_t &o)
        {
            for(int i=0; i<N; i++){
                m_min[i]=std::min(m_min[i], o.m_min[i]);
                m_max[i]=std::max(m_max[i], o.m_max[i]);
                for(int j=0; j<=4; j++){
                    m_sums[i][j]+=o.m_sums[i][j];
                }
            }
        }
    };

    std::atomic<stats_t *> m_pStats;
    stats_t m_stg;
    std::atomic<uint64_t> m_n;

    using acc_t = __float128;

    double sum_impl(acc_t &acc)
    {
        return (double)acc;
    }

    template<class THead, class... TTail>
    double sum_impl(acc_t &acc, THead head, TTail ...tail)
    {
        acc += head;
        return sum_impl(acc, tail...);
    }

    template<class ...TArgs>
    double sum(TArgs ...args)
    {
        acc_t acc=0;
        return sum_impl(acc, args...);
    }
public:
    ParallelRunningStats()
    {
        for(unsigned i=0; i<N; i++){
            m_stg.m_min[i]=DBL_MAX;
            m_stg.m_max[i]=DBL_MIN;
            for(unsigned j=0; j<=4; j++){
                m_stg.m_sums[i][j]=0;
            }
        }
        m_pStats.store(&m_stg);
        m_n.store(0);
    }

    ParallelRunningStats(const ParallelRunningStats &) = delete;
    ParallelRunningStats(ParallelRunningStats &&) = delete;

    ParallelRunningStats &operator=(const ParallelRunningStats &) = delete;
    ParallelRunningStats &operator=(ParallelRunningStats &&) = delete;


    // Never called in parallel context
    void operator+=(const std::array<double,N> &x)
    {
        m_stg += x;
        m_n.fetch_add(1);
    }

    // Never called in parallel context
    void operator+=(double x)
    {
        static_assert(N==1);
        m_stg += std::array<double,1>({x});
        m_n.fetch_add(1);
    }

    // Can be called in parallel context for "this".
    void operator+=(const ParallelRunningStats &o)
    {
        stats_t *dst=0;

        while(dst==0){
            while(m_pStats.load(std::memory_order_relaxed) == nullptr){
                __builtin_ia32_pause();
            }
            dst=m_pStats.exchange(dst, std::memory_order_acquire);
        }

        (*dst) += o.m_stg;

        m_pStats.store(dst, std::memory_order_release);
        m_n.fetch_add(o.m_n.load());
    }

    void walk_stats(const char *name, const std::function<void(const char *name, const char *stat, double val)> &cb){
        static_assert(N==1);
        walk_stats(std::array<const char *,1>{name}, cb);
    };

    // Cannot be called in parallel context
    void walk_stats(const std::array<const char *,N> &names, const std::function<void(const char *name, const char *stat, double val)> &cb){
        stats_t *pp=m_pStats.load();
        if(pp!=&m_stg){
            throw std::runtime_error("cant walk stats while update is happening.");
        }

        assert(m_n.load()==m_stg.m_sums[0][0]);

        for(unsigned i=0; i<N; i++){
            double count=m_stg.m_sums[i][0];
            cb( names[i], "count", count );
            if(count==0){
                cb( names[i], "min", nan("") );
                cb( names[i], "max", nan("") );
                cb( names[i], "mean", nan("") );
                cb( names[i], "stddev", nan("") );
                cb( names[i], "skewness", nan("") );
                cb( names[i], "kurtosis", nan("") );
                continue;
            }
            
            cb( names[i], "min", m_stg.m_min[i] );
            cb( names[i], "max", m_stg.m_max[i] );

            acc_t inv_count = 1.0 / acc_t(count);
            acc_t moments[5];
            for(int j=1; j<=4; j++){
                moments[j] = m_stg.m_sums[i][j] * inv_count;
            }

            acc_t mean=moments[1];
            cb( names[i], "mean", (double)mean );

            if(count < 2){
                cb( names[i], "stddev", nan("") );
                cb( names[i], "skewness", nan("") );
                cb( names[i], "kurtosis", nan("") );
            }
            
            acc_t variance = moments[2]-mean*mean;
            if(variance <= 0){
                cb( names[i], "skewness", nan("") );
                cb( names[i], "skewness", nan("") );
                cb( names[i], "kurtosis", nan("") );
                continue;
            }else{
                acc_t stddev = sqrtq(variance);
                cb( names[i], "stddev", stddev );

                acc_t skewness = sum( moments[3] , - 3. * moments[2] * mean , + 2. * mean * mean * mean ) / ( variance  * stddev );
                cb( names[i], "skewness", skewness);

                // This is excess kurtosis, so normal==0
                acc_t kurtosis =
                    sum(
                        moments[4],
                        - 4. * moments[3] * mean,
                        + 6. * moments[2] * mean * mean,
                        - 3. * mean * mean* mean * mean
                    )
                    /
                    ( variance * variance ) - 3.;
                cb( names[i], "kurtosis", kurtosis);
            }

        }
    }
};

#endif
