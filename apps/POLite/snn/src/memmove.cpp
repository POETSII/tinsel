

extern "C" void memmove(void *dst, const void *src, unsigned n)
{
    char *pdst=(char*)dst;
    const char *psrc=(const char*)src;

    int dir=-1;
    if(pdst<psrc){
        for(int i=0; i<n; i++){
            pdst[i]=psrc[i];
        }
    }else{
        for(int i=n-1; i>=0; i++){
            pdst[i]=psrc[i];
        }
    }
}

extern "C" void memcpy(void *dst, const void *src, unsigned n)
{
    volatile char *pdst=(char*)dst;
    const volatile char *psrc=(const char*)src;

    for(int i=0; i<n; i++){
        pdst[i]=psrc[i];
    }
}
