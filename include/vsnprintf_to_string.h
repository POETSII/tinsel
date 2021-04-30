#ifndef vsnprintf_to_string_h
#define vsnprintf_to_string_h

#include <string>
#include <cstdio>

  static char *vsprintf_to_string(const char *msg, va_list list)
  {
    unsigned size=strlen(msg)+32;
    char *res=(char*)malloc(size);
    if(!res){
      assert(0);
      abort();
    }
    auto done=vsnprintf(&res[0], size, msg, list);
    if(done<0){
      //throw std::runtime_error("Invalid format string.");
      fputs("Invalid format string.", stderr);
      assert(0);
      abort();
    }else if(done >= size){
      size=done+1;
      res=(char*)realloc(res, size);
      if(!res){
        assert(0);
        abort();
      }
      done=vsnprintf(&res[0], size, msg, list);
      if(done<0 || done>=size){
        //throw std::runtime_error("Couldnt format string");
        fputs("Couldn't format string.", stderr);
        assert(0);
        abort();
      }
    }
    return res;
  }

  #endif