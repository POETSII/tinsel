#ifndef latency_hpp
#define latency_hpp

typedef struct _job_msg_t
{
    uint32_t dest_mbox;
    uint32_t source_mbox;
    uint32_t reps;
    uint32_t cycle_count;
} job_msg_t;
static_assert(sizeof(job_msg_t)==1<<TinselLogBytesPerFlit);

typedef struct _job_msg_full_t
    : job_msg_t
{
    uint8_t pad[(TinselMaxFlitsPerMsg-1)<<TinselLogBytesPerFlit];
} job_msg_full_t;
static_assert(sizeof(job_msg_full_t)==(TinselMaxFlitsPerMsg<<TinselLogBytesPerFlit));

#endif

