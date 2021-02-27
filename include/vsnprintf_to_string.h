#ifndef vsnprintf_to_string_h
#define vsnprintf_to_string_h

#include <string>
#include <cstdio>

  static std::string vsprintf_to_string(const char *msg, va_list list)
  {
    std::string res;
    res.resize(256);
    auto done=vsnprintf(&res[0], res.size(), msg, list);
    if(done<0){
      throw std::runtime_error("Invalid format string.");
    }else if(done >= res.size()){
      res.resize(done+1);
      done=vsnprintf(&res[0], res.size(), msg, list);
      if(done<0 || done>=res.size()){
        throw std::runtime_error("Couldnt format string");
      }
    }
    res.resize(done);
    return res;
  }

  #endif