#ifndef _RNG_H_
#define _RNG_H_

inline uint32_t urng(uint32_t &state) {
  state = state*1664525+1013904223;
  return state;
}

// World's crappiest gaussian (courtesy of dt10!)
inline float grng(uint32_t &state) {
  uint32_t u=urng(state);
  int32_t acc=0;
  for(unsigned i=0;i<8;i++){
    acc += u&0xf;
    u=u>>4;
  }
  // a four-bit uniform has mean 7.5 and variance ((15-0+1)^2-1)/12 = 85/4
  // sum of four uniforms has mean 8*7.5=60 and variance of 8*85/4=170
  const float scale=0.07669649888473704; // == 1/sqrt(170)
  return (acc-60.0f) * scale;
}

#endif
