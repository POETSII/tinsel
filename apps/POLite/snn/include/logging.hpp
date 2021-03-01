#ifndef snn_logging_hpp
#define snn_logging_hpp

#include <cassert>
#include <vector>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <mutex>

#include <unistd.h>

#include <sys/time.h>
#include <sys/resource.h>

#include "vsnprintf_to_string.h"

class LogContext;

class Logger
{
public:
    double now()
    {
        timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        return ts.tv_sec+1e-9*ts.tv_nsec - m_t0;
    }

private:
    
    struct region
    {
        int handle;
        bool is_leaf;
        std::string local_name;
        std::string full_name;
        double start_wall_clock;
        double start_user_time;
        double start_kernel_time;
    };

    double m_t0;
    FILE *m_dst=0;
    int m_dst_log_level=10;
    int m_stderr_log_level=2;
    std::vector<region> m_regions;
    int m_next_handle=0;

    std::string m_scope_name;

    std::mutex m_mutex;

    void do_exit_region()
    {
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        double t=now();
        double dt=t-m_regions.back().start_wall_clock;
        double user_time=(usage.ru_utime.tv_sec+1e-6*usage.ru_utime.tv_usec)-m_regions.back().start_user_time;
        double kernel_time=(usage.ru_stime.tv_sec+1e-6*usage.ru_stime.tv_usec)-m_regions.back().start_kernel_time;
        export_value("region_wall_time", dt, 2);
        export_value("region_kernel_time", kernel_time,2 );
        export_value("region_user_time", user_time, 2);
        double utilisation=(kernel_time+user_time)/dt;
        export_value("region_cpu_utilisation", utilisation, 2);
        if(m_dst){
            fprintf(m_dst, "%.6f,  EXIT, %-64s, %.6f\n", now(), m_scope_name.c_str(), dt);
        }
        fprintf(stderr, "%.6f,  EXIT, %-64s, %.6f\n", now(), m_scope_name.c_str(), dt);
        

        m_regions.pop_back();
        m_scope_name= m_regions.empty() ? "" : m_regions.back().full_name;
    }

    std::string strip(std::string x)
    {
        while(!x.empty() && isspace(x.back())){
            x.pop_back();
        }
        while(!x.empty() && isspace(x.front())){
            x.erase(x.begin());
        }
        int p;
        while((p=x.find_first_of(",\n\r"))!=std::string::npos){
            x[p]='_';
        }
        return x;
    }

    void add_system_info_lscpu()
    {
        enter_region("lscpu");

        char buffer[256]={0};
        FILE *f=popen("lscpu", "r");
        if(f){
            while(fgets(buffer, sizeof(buffer)-1, f)){
                std::string s(buffer);
                
                int colon=s.find(':');
                if(colon==std::string::npos){
                    continue;
                }

                std::string key=strip(s.substr(0, colon));
                std::string val=strip(s.substr(colon+1));

                if(!key.empty() && !val.empty()){
                    export_value(key.c_str(), val.c_str(), 2);
                }
            }

            fclose(f);
        }

        exit_region();
    }

    void add_system_info()
    {
        add_system_info_lscpu();
    }

    void attach_log_file(const std::string &dst)
    {
        assert(!m_dst);
        FILE *dst_h=fopen(dst.c_str(), "w");
        if(!dst_h){
            fprintf(stderr, "Couldnt open log file %s\n", dst.c_str());
            exit(1);
        }
        m_dst=dst_h;
    }

    void on_exit()
    {
        while(!m_regions.empty()){
            exit_region();
        }
        if(m_dst){
            fclose(m_dst);
        }
    }

    void log_impl(const char *type, int severity, const char *msg, va_list va)
    {
        if(severity <= m_dst_log_level || severity <= m_stderr_log_level){
            double t=now();

            char buffer[256]={0};

            vsnprintf(buffer, sizeof(buffer)-1, msg, va);

            if(severity < m_dst_log_level && m_dst){
                fprintf(m_dst, "%.6f, %-5s, %-64s, %s\n", t, type, m_scope_name.c_str(), msg);
            }
            if(severity < m_stderr_log_level){
                fprintf(stderr, "%.6f, %-5s, %-64s, %s\n", t, type, m_scope_name.c_str(), msg);
            }
        }
    }

public:
    Logger(std::string log_file_name)
        : m_t0(0)
        , m_dst(0)
    {
        attach_log_file(log_file_name);

        m_t0=now();
        enter_region("prog");

        enter_region("sys-info");
        char buffer[128]={0};
        gethostname(buffer, sizeof(buffer)-1);
        export_value("hostname", buffer, 2);
        add_system_info();
        exit_region();
    }

    ~Logger()
    {
        on_exit();
    }

    void log(int severity, const char *msg, ...)
    {
        va_list va;
        va_start(va,msg);
        log_impl("MSG", severity, msg, va);
        va_end(va);
    }

    void log_locked(int severity, const char *msg, ...)
    {
        std::unique_lock<std::mutex> lk(m_mutex);

        va_list va;
        va_start(va,msg);
        log_impl("MSG", severity, msg, va);
        va_end(va);
    }

    void export_value(const char *key, const std::string &value, int level)
    {
        if(level < m_dst_log_level && m_dst){
            fprintf(m_dst, "%.6f,   VAL, %-64s, %s\n", now(), (m_scope_name+"+"+key).c_str(), value.c_str());
        }
        if(level < m_stderr_log_level){
            
            fprintf(stderr, "%.6f,   VAL, %-64s, %s\n", now(),  (m_scope_name+"+"+key).c_str(), value.c_str());
        }
    }

    void export_value(const char *key, double value, int level)
    {
        if(level < m_dst_log_level || level < m_stderr_log_level){
            export_value(key, std::to_string(value), level);
        }
    }

    void export_value(const char *key, int64_t value, int level)
    {
        if(level < m_dst_log_level || level < m_stderr_log_level){
            export_value(key, std::to_string(value), level);
        }
    }

    void flush()
    {
        if(m_dst){
            fflush(m_dst);
        }
    }

    int enter_region(const char *name)
    {
        int h=m_next_handle++;
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        double t=now();
        m_regions.push_back({
            h, false, name, m_scope_name+"/"+name,
            t, usage.ru_utime.tv_sec+1e-6*usage.ru_utime.tv_usec, usage.ru_stime.tv_sec+1e-6*usage.ru_stime.tv_usec
        });
        m_scope_name=m_regions.back().full_name;
        if(m_dst){
            fprintf(m_dst, "%.6f, ENTER, %-64s\n", now(), m_scope_name.c_str());
        }
        fprintf(stderr, "%.6f, ENTER, %-64s\n", now(), m_scope_name.c_str());
        return h;
    }

    void exit_region(int h=-1)
    {
        assert(!m_regions.empty());
        if(m_regions.back().is_leaf){
            exit_leaf();
        }

        do_exit_region();
    }

    void enter_leaf(const char *name)
    {
        assert(!m_regions.empty());
        if(m_regions.back().is_leaf){
            exit_leaf();
        }

        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        double t=now();
        m_regions.push_back({
            -1, true, name, m_scope_name+"/"+name,
            t, usage.ru_utime.tv_sec+1e-6*usage.ru_utime.tv_usec, usage.ru_stime.tv_sec+1e-6*usage.ru_stime.tv_usec
        });
        m_scope_name=m_regions.back().full_name;
        if(m_dst){
            fprintf(m_dst, "%.6f, ENTER, %-64s\n", now(), m_scope_name.c_str());
        }
        fprintf(stderr, "%.6f, ENTER, %-64s\n", now(), m_scope_name.c_str());
    }

    void tag_leaf(const char *name)
    {
        enter_leaf(name);
    }

    void exit_leaf()
    {
        assert(!m_regions.empty());
        assert(m_regions.back().is_leaf);
        do_exit_region();
    }

    [[noreturn]] void fatal_error(const char *msg, ...)
    {
        va_list va;
        va_start(va, msg);
        log_impl("FATAL", -1, msg, va);
        va_end(va);

        on_exit();
        exit(1);
    }   

    LogContext with_region(const char *name);
};

struct LogContext
{
private:
    int m_h=0;
    Logger *m_logger=0;
public:
    LogContext()
    {}

    LogContext(const LogContext &o) = delete;

    LogContext(LogContext &&o)
        : m_h(o.m_h)
        , m_logger(o.m_logger)
    {
        o.m_h=0;
        o.m_logger=0;
    }

    LogContext &operator=(const LogContext &) = delete;

    LogContext &operator=(LogContext &&o)
    {
        assert(!m_logger);
        std::swap(m_logger, o.m_logger);
        std::swap(m_h, o.m_h);
    }

    LogContext(Logger &logger, const char *name)
    {
        enter(logger, name);
    }

    void enter(Logger &logger, const char *name)
    {
        assert(!m_logger);
        m_h=logger.enter_region(name);
        m_logger=&logger;
    }

    void exit()
    {
        assert(m_logger);
        m_logger->exit_region(m_h);
        m_logger=0;
        m_h=0;
    }

    ~LogContext()
    {
        if(m_logger){
            exit();
        }
    }
        
};

LogContext Logger::with_region(const char *name)
{
    return LogContext(*this, name);
}

#endif