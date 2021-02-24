#include <cstdint>
#include <vector>
#include <random>
#include <cassert>
#include <cstdio>
#include <functional>
#include <numeric>

#include <unistd.h> 

#include "external/robin_hood.h"

template<class THash>
struct id_set
{
    id_set(THash _hash, std::vector<uint32_t> &_ids)
        : hash(_hash)
        , ids(_ids)
        , k(_ids.size())
        , max_generate(k*uint64_t(k))
    {
    }

    const THash hash;

    const std::vector<uint32_t> ids;
    const uint32_t k; //== ids.size();
    const uint64_t max_generate;
};

uint32_t
h1_splittable64(uint64_t x)
{
    x ^= x >> 30;
    x *= UINT64_C(0xbf58476d1ce4e5b9);
    x ^= x >> 27;
    x *= UINT64_C(0x94d049bb133111eb);
    x ^= x >> 31;
    return x;
}

uint32_t
h2_identity(uint64_t x)
{
    return x^(x>>32);
}

template<class THash>
struct id_set_linear_walker
{
    id_set_linear_walker(const id_set<THash> &_src, int _dim)
        : src(_src)
        , dim(_dim)
    {}

    const id_set<THash> &src;
    const int dim;
    unsigned o1=0, o2=0;
    uint64_t generated=0;

    uint32_t operator()()
    {
        if(generated==src.max_generate){
            fprintf(stderr, "Warn : exceeding period.\n");
        }

        unsigned i1=o1, i2=o2;
        if(dim){
            std::swap(i1,i2);
        }
        
        uint32_t res= src.hash(src.ids.at(i1),src.ids.at(i2));
        //fprintf(stderr, "  %llu, (%u,%u), %08x\n", generated, i1, i2, res);
        generated++;
        o1++;
        if(o1==src.k){
            o1=0;
            o2++;
            if(o2==src.k){
                o2=0;
            }
        }
       
        return res;
    }
};

template<class THash>
struct id_set_random_walker
{
    id_set_random_walker(const id_set<THash> &_src)
        : src(_src)
        , k(_src.k)
    {
        unsigned kw=1;
        while( (1<<kw) < k ){
            kw++;
        }
        kmask=(1ul<<kw)-1;
        
        seen.reserve( std::min<size_t>(1u<<30, k*size_t(k)) );
    }

    const id_set<THash> &src;
    const unsigned k;
    unsigned kmask;
    std::mt19937_64 rng;
    uint64_t generated=0;
    uint64_t collisions=0;

    struct identity
    {
        size_t operator()(uint64_t x) const
        { return x; }
    };

    robin_hood::unordered_flat_set<uint64_t,identity> seen;

    uint32_t operator()()
    {
        if(generated>src.max_generate/2){
            fprintf(stderr, "Error : generated more than half of possibilities.\n");
            exit(1);
        }
        generated++;
        unsigned o1, o2;
        uint64_t u;
        while(1){
            u=rng();
            o1=u&kmask;
            o2=(u>>32)&kmask;
            if(o1>=k || o2>=k){
                continue;
            }
            u=o1*k+o2;
            if(seen.insert(u).second){
                break;
            }
            ++collisions;
            //fprintf(stderr, "%u,%u,  %llu, %llu\n", o1, o2, collisions, generated);
        }

        return src.hash(src.ids.at(o1),src.ids.at(o2));
    }
};

template<class THash>
struct id_set_random_walker2
{
    id_set_random_walker2(const id_set<THash> &_src)
        : src(_src)
        , k(_src.k)
    {
        kw=1;
        while( (1<<kw) < k ){
            kw++;
        }
        if( (1<<kw)!=k ){
            fprintf(stderr, "Only binary k supported.");
            exit(1);
        }
        kmask=(1u<<kw)-1;
        
        bits.resize(1ull<<32);
    }

    const id_set<THash> &src;
    const unsigned k;
    unsigned kmask;
    unsigned kw;
    
    std::mt19937_64 rng;
    std::vector<bool> bits;
    uint64_t generated=0;

    uint32_t operator()()
    {
        if(generated>src.max_generate/2){
            fprintf(stderr, "Error : generated more than half of possibilities.\n");
            exit(1);
        }
        generated++;
        unsigned o1, o2;
        uint64_t u;
        while(1){
            u=rng()>>(64-2*kw);

            uint32_t h2=h2_identity(u);
            uint32_t h1=h1_splittable64(u);
            

            bool b2=bits[h2];
            bool b1=bits[h1];

            if(!(b1&&b2)){
                bits[h2]=true;
                bits[h1]=true;
                break;
            }
        }
        o1=u&kmask;
        o2=u>>kw;

        return src.hash(src.ids.at(o1),src.ids.at(o2));
    }
};

template<class THash>
struct id_set_mod_walker
{
    id_set_mod_walker(const id_set<THash> &_src)
        : src(_src)
        , k(_src.k)
        , kk(k*uint64_t(k))
    {
        step=k*7 + (k/3);
        while(std::gcd(kk,step)!=1){
            step++;
        }

    }

    const id_set<THash> &src;
    const unsigned k;
    const uint64_t kk;
    uint64_t generated=0;
    uint64_t step;
    uint64_t pos=0;

    robin_hood::unordered_flat_set<uint64_t> seen;

    uint32_t operator()()
    {
        if(generated>src.max_generate/2){
            fprintf(stderr, "Error : generated more than half of possibilities.\n");
            exit(1);
        }
        generated++;
        unsigned o1, o2;
        pos=(pos+step);
        if(pos>=kk){
            pos-=kk;
        }
        o1=pos/k;
        o2=pos%k;

        //assert(seen.insert(pos).second);

        return src.hash(src.ids.at(o1),src.ids.at(o2));
    }
};


inline uint32_t hash32_to_uniform_bits(uint32_t x)
{
    // Each xorshift is 2 instr, while each multiply is 3 instr (need 2 to construct 32-bit immediate)
    // So about 15 instructions if inlined
    x ^= x >> 17;
    x *= 0xed5ad4bbu;
    x ^= x >> 11;
    x *= 0xac4c1b51u;
    x ^= x >> 15;
    x *= 0x31848babu;
    x ^= x >> 14;
    return x;
}


uint32_t pairwise_hash_asymmetric_ubits(uint32_t a, uint32_t b)
{
    // For less than 2^22 keys this fairly works well, and passes practrand for
    // up to 1B samples generated (linear1, linear2, mod)

    const uint32_t M1= 0xed5ad4bbu;

    uint32_t x=(a+M1) ^ b;
    for(int i=0; i<3; i++){
        x ^= x >> 16;
        x *= M1;
    }
    return x;
}


template<class THash>
auto make_id_set(THash hash, unsigned k, unsigned tag_bits,std::mt19937_64 &rng)
{
    unsigned id_width=1;
    while((1<<id_width) < k){
        id_width++;
    }
    unsigned tag_width=std::min<unsigned>(tag_bits,32-id_width);
    unsigned tag_mask=(1u<<tag_width)-1;

    fprintf(stderr, "id_w=%u, tag_w=%u\n", id_width, tag_width);

    std::vector<uint32_t> ids;
    for(unsigned i=0; i<k; i++){
        uint32_t id;
        id=(i<<tag_width) | (rng()&tag_mask);
        ids.push_back(id);
    }

    return id_set<THash>(hash, ids);
}

template<class THash>
auto make_linear_walker(const id_set<THash> &h, int dir)
{
    return id_set_linear_walker<THash>(h,dir);
}

template<class THash>
auto make_random_walker(const id_set<THash> &h)
{
    return id_set_random_walker2<THash>(h);
}

template<class THash>
auto make_mod_walker(const id_set<THash> &h)
{
    return id_set_mod_walker<THash>(h);
}

int main(int argc, char *argv[])
{
    unsigned log2k=16;
    std::string method="linear1";
    unsigned log2tag=32-log2k;

    if(argc>1){
        log2k=atoi(argv[1]);
    }
    if(argc>2){
        log2tag=atoi(argv[2]);
    }
    if(argc>3){
        method=argv[3];
    }


    std::mt19937_64 rng;
    auto s=make_id_set(pairwise_hash_asymmetric_ubits, 1<<log2k, log2tag, rng);

    bool text_out=false;

    std::function<uint32_t()> src;
    if(method=="linear1"){
        auto w=make_linear_walker(s,0);
        src = [w]() mutable -> uint32_t { return w(); };
    }else if(method=="linear2"){
        auto w=make_linear_walker(s,1);
        src = [w]() mutable -> uint32_t { return w(); };
    }else if(method=="random"){
        auto w=make_random_walker(s);
        src = [w]() mutable -> uint32_t { return w(); };
    }else if(method=="mod"){
        auto w=make_mod_walker(s);
        src = [w]() mutable -> uint32_t { return w(); };
    }else{
        fprintf(stderr, "Unknown method %s\n", method.c_str());
    }

    if(text_out){
        fprintf(stderr, "k=%u, max=%llu\n", s.k, s.max_generate);
        for(unsigned i=0; i<s.max_generate; i++){
            fprintf(stdout, "%.8f\n",ldexp(src(),-32));
        }
    }else{
        if(isatty(fileno(stdout))){
            fprintf(stderr, "Not printing binary to a tty\n");
            exit(1);
        }

        const unsigned BUFF_SIZE=1024;
        uint32_t buffer[BUFF_SIZE];
        for(unsigned i=0; i<s.max_generate; i+=BUFF_SIZE){
            unsigned todo=std::min<size_t>(BUFF_SIZE, s.max_generate-i );
            for(unsigned j=0; j<todo; j++){
                buffer[j]=src();
            }
            fwrite(buffer, 4, todo, stdout);
        }
    }
}