#ifndef Routing_hpp
#define Routing_hpp

#include <iostream>
#include <vector>
#include <array>
#include <cassert>
#include <sstream>
#include <memory>
#include <unordered_set>
#include <functional>
#include <algorithm>
#include <unordered_map>
#include <random>
#include <typeinfo>

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>

#include "config.h"

struct Route;
struct Link;
struct LinkIn;
struct LinkOut;
struct Node;
struct FPGANode;
struct FPGANode0p6;
struct MailboxNode;

/*
    In the tinsel network model:
    - Down : decreasing y, goes to South link
    - Up   : increasing y, goes to North link
    - Left : decreasing x, goes to East link
    - Right : increasing x, goes to West link

    So viewed as a 2D grid, the (0,0) point is in the bottom-left corner.
     
    Routing is ordered by:
    - FPGA Up/Down (North/South)
    - FPGA Left/Right (East/West) 
    - Mailbox Up/Down (North/South)
    - Mailbox Left/Right (East/West)

    In this routing model we only use the N/S/E/W names,
    both for directions and for links.
*/

enum Direction
{
    North,
    South,
    East,
    West,
    Here
};

const char * dir_to_string(Direction d)
{
    static std::array<const char *,5> dirs{"North","South","East","West","Here"};
    return dirs.at(d);
}

bool is_vertical(Direction d)
{
    return d==North || d==South;
}

bool is_horizontal(Direction d)
{
    return d==East || d==West;
}

Direction reverse(Direction d)
{
    switch(d){
    case North: return South;
    case South: return North;
    case East: return West;
    case West: return East;
    default: throw std::runtime_error("Can't reverse");
    }
}

enum LinkType
{
    UnknownLink,
    InterFPGALink,
    InterMailboxLink,
    WiringLink
};

struct thread_id_t
{
    unsigned value;
};

struct route_id_t
{
    uint64_t value;

    bool operator==(const route_id_t &o) const
    { return value==o.value; }
};

namespace std
{
    template<>
    struct hash<route_id_t>
    {
        size_t operator()(const route_id_t &x) const
        { return x.value; }
    };
};

template<class Tag>
struct Coord
{
    unsigned x;
    unsigned y;

    bool operator==(const Coord &o) const
    { return x==o.x && y==o.y; }
};


using MailboxCoord=Coord<MailboxNode>;
using FPGACoord=Coord<FPGANode0p6>;

struct MailboxOffsetInFPGATag;
using MailboxOffsetInFPGA=Coord<MailboxOffsetInFPGATag>;

struct SystemParameters
{
    std::string topology;

    unsigned FPGAMeshXLen;
    unsigned FPGAMeshYLen;
    unsigned FPGAMeshYBits;
    unsigned FPGAMeshXBits;

    unsigned MailboxMeshXLen;
    unsigned MailboxMeshYLen;
    unsigned MailboxMeshYBits;
    unsigned MailboxMeshXBits;

    unsigned LogThreadsPerMailbox;

    unsigned FPGALinkCost;
    unsigned MailboxLinkCost;

    bool operator==(const SystemParameters &o) const
    {
        return topology==o.topology && FPGAMeshXLen==o.FPGAMeshXLen && FPGAMeshYLen==o.FPGAMeshYLen &&
            FPGAMeshXBits==o.FPGAMeshXBits && FPGAMeshYBits==o.FPGAMeshYBits &&
            MailboxMeshXLen==o.MailboxMeshXLen && MailboxMeshYLen==o.MailboxMeshYLen && 
            MailboxMeshXBits==o.MailboxMeshXBits && MailboxMeshYBits==o.MailboxMeshYBits && 
            LogThreadsPerMailbox==o.LogThreadsPerMailbox &&
            FPGALinkCost==o.FPGALinkCost && MailboxLinkCost==o.MailboxLinkCost; 
    }

    template<class TAlloc>
    rapidjson::Value save(TAlloc &alloc)
    {
        rapidjson::Value res;
        res.SetObject();
        res.AddMember("type", "system_parameters", alloc);
        res.AddMember("topology", rapidjson::Value(topology, alloc), alloc);
        
        res.AddMember("FPGAMeshXLen", FPGAMeshXLen, alloc);
        res.AddMember("FPGAMeshYLen", FPGAMeshYLen, alloc);
        res.AddMember("FPGAMeshXBits", FPGAMeshXBits, alloc);
        res.AddMember("FPGAMeshYBits", FPGAMeshYBits, alloc);
        
        res.AddMember("MailboxMeshXLen", MailboxMeshXLen, alloc);
        res.AddMember("MailboxMeshYLen", MailboxMeshYLen, alloc);
        res.AddMember("MailboxMeshXBits", MailboxMeshXBits, alloc);
        res.AddMember("MailboxMeshYBits", MailboxMeshYBits, alloc);
    
        res.AddMember("LogThreadsPerMailbox", LogThreadsPerMailbox, alloc);
        
        res.AddMember("FPGALinkCost", FPGALinkCost, alloc);
        res.AddMember("MailboxLinkCost", MailboxLinkCost, alloc);

        return res;
    }

    void load(const rapidjson::Value &val)
    {
        assert(val.IsObject());
        assert(val["type"].GetString() == std::string("system_parameters"));

        topology=val["topology"].GetString();
        
        FPGAMeshXLen=val["FPGAMeshXLen"].GetUint();
        FPGAMeshYLen=val["FPGAMeshYLen"].GetUint();
        FPGAMeshXBits=val["FPGAMeshXBits"].GetUint();
        FPGAMeshYBits=val["FPGAMeshYBits"].GetUint();

        MailboxMeshXLen=val["MailboxMeshXLen"].GetUint();
        MailboxMeshYLen=val["MailboxMeshYLen"].GetUint();
        MailboxMeshXBits=val["MailboxMeshXBits"].GetUint();
        MailboxMeshYBits=val["MailboxMeshYBits"].GetUint();

        LogThreadsPerMailbox=val["LogThreadsPerMailbox"].GetUint();
        
        FPGALinkCost=val["FPGALinkCost"].GetUint();
        MailboxLinkCost=val["MailboxLinkCost"].GetUint();
    }

    unsigned get_total_threads() const
    {
        return FPGAMeshXLen*FPGAMeshYLen*MailboxMeshXLen*MailboxMeshYLen*(1u<<LogThreadsPerMailbox);
    }

    FPGACoord get_fpga_coord(thread_id_t thread) const
    {
        auto tmp=thread.value >> (MailboxMeshXBits+MailboxMeshYBits+LogThreadsPerMailbox);
        auto x=tmp & ((1u<<FPGAMeshXBits)-1);
        auto y=(tmp>>FPGAMeshXBits) & ((1u<<FPGAMeshYBits)-1);
        assert(x<FPGAMeshXLen);
        assert(y<FPGAMeshYLen);
        // fprintf(stderr, "%0xu -> %u,%u\n", thread.value, x,y);
        return FPGACoord{x,y};
    }

    FPGACoord get_fpga_coord(MailboxCoord mbox) const
    {
        auto x = mbox.x/MailboxMeshXLen;
        auto y = mbox.y/MailboxMeshYLen;
        assert(x<FPGAMeshXLen);
        assert(y<FPGAMeshYLen);
        // fprintf(stderr, "%0xu -> %u,%u\n", thread.value, x,y);
        return FPGACoord{x,y};
    }

    // This always returns contiguous coordinates, even if the mesh is not a binary power
    MailboxCoord get_mailbox_coord(thread_id_t thread) const
    {
        auto mx = (thread.value >> LogThreadsPerMailbox) & ((1u<<MailboxMeshXBits)-1);
        auto my = (thread.value >> (MailboxMeshXBits+LogThreadsPerMailbox)) & ((1u<<MailboxMeshYBits)-1);
        auto fx = (thread.value >> (MailboxMeshYBits+MailboxMeshXBits+LogThreadsPerMailbox)) & ((1u<<FPGAMeshXBits)-1);
        auto fy = (thread.value >> (FPGAMeshXBits+MailboxMeshYBits+MailboxMeshXBits+LogThreadsPerMailbox)) & ((1u<<FPGAMeshYBits)-1);
        assert(mx<MailboxMeshXLen);
        assert(my<MailboxMeshYLen);
        assert(fx<FPGAMeshXLen);
        assert(fy<FPGAMeshYLen);
        MailboxCoord res {fx*MailboxMeshXLen+mx,fy*MailboxMeshYLen+my}; 
        //fprintf(stderr, "%0xu -> %u,%u  %u,%u,  %u,%u\n", thread.value, fx,fy, mx,my, res.x, res.y);
        return res;    
    }

    MailboxOffsetInFPGA get_mailbox_offset_in_fpga(thread_id_t thread) const
    {
        auto mx = (thread.value >> LogThreadsPerMailbox) & ((1u<<MailboxMeshXBits)-1);
        auto my = (thread.value >> (MailboxMeshXBits+LogThreadsPerMailbox)) & ((1u<<MailboxMeshYBits)-1);
        assert(mx<MailboxMeshXLen);
        assert(my<MailboxMeshYLen);
        MailboxOffsetInFPGA res {mx,my}; 
        return res;    
    }

    thread_id_t make_thread_id(MailboxCoord pos, unsigned coreOffset) const
    {
        assert(pos.x < FPGAMeshXLen*MailboxMeshXLen);
        assert(pos.y < FPGAMeshYLen*MailboxMeshYLen);
        assert(coreOffset < (1u<<LogThreadsPerMailbox));
        unsigned fx = pos.x / MailboxMeshXLen;
        unsigned mx = pos.x % MailboxMeshXLen;
        unsigned fy = pos.y / MailboxMeshYLen;
        unsigned my = pos.y % MailboxMeshYLen;
        thread_id_t res{
            coreOffset + 
            (mx<<LogThreadsPerMailbox) +
            (my<<(MailboxMeshXBits+LogThreadsPerMailbox)) +
            (fx<<(MailboxMeshYBits+MailboxMeshXBits+LogThreadsPerMailbox)) +
            (fy<<(FPGAMeshXBits+MailboxMeshYBits+MailboxMeshXBits+LogThreadsPerMailbox))
        };
        //fprintf(stderr, " %u,%u+%u -> %x\n", pos.x, pos.y, coreOffset, res.value);
        return res;
    }

    void for_each_thread(std::function<void(FPGACoord,MailboxCoord,thread_id_t)> cb) const
    {
        for(unsigned fx=0; fx<FPGAMeshXLen; fx++){
            for(unsigned fy=0; fy<FPGAMeshYLen; fy++){
                FPGACoord fpga_coord{fx,fy};
                for(unsigned mx=0; mx<MailboxMeshXLen; mx++){
                    for(unsigned my=0; my<MailboxMeshYLen; my++){
                        MailboxCoord mbox_coord{fx*MailboxMeshXLen+mx,fy*MailboxMeshYLen+my};
                        auto tid=make_thread_id(mbox_coord, 0);
                        for(unsigned o=0; o<(1u<<LogThreadsPerMailbox); o++){
                            cb(fpga_coord, mbox_coord, tid);
                            tid.value++;
                        }
                    }
                }
            }
        }
    }

    thread_id_t pick_random_thread(std::mt19937 &rng) const
    {
        return make_thread_id(
            MailboxCoord{
                unsigned(rng()%(FPGAMeshXLen*MailboxMeshXLen)),
                unsigned(rng()%(FPGAMeshYLen*MailboxMeshYLen))
            },
            rng()%(1u<<TinselLogCoresPerMailbox)
        );
    }

    unsigned get_link_cost(LinkType type) const
    {
        switch(type)
        {
        case WiringLink: return 0;
        case InterFPGALink: return FPGALinkCost;
        case InterMailboxLink: return MailboxLinkCost;
        default: throw std::runtime_error("Unknown link type.");
        }
    }
};

std::unique_ptr<SystemParameters> initSystemParameters(std::string topology, unsigned FPGAMeshXLen, unsigned FPGAMeshYLen, unsigned MailboxMeshXLen, unsigned MailboxMeshYLen)
{
    std::unique_ptr<SystemParameters> p(new SystemParameters());

    p->topology=topology;

    unsigned LogCoresPerMailbox = TinselLogCoresPerMailbox;
    unsigned LogThreadsPerCore = TinselLogThreadsPerCore;
    p->LogThreadsPerMailbox = LogCoresPerMailbox + LogThreadsPerCore;

    p->FPGAMeshXBits=TinselMeshXBits;
    p->FPGAMeshYBits=TinselMeshYBits;
    p->MailboxMeshXBits=TinselMailboxMeshXBits;
    p->MailboxMeshYBits=TinselMailboxMeshYBits;

    /*
    p->FPGAMeshXBits=3;
    p->FPGAMeshYBits=3;
    p->MailboxMeshXBits=3;
    p->MailboxMeshYBits=3;
    */

    if((1u<<FPGAMeshXLen) < FPGAMeshXLen ){
        throw std::runtime_error("Not enough FPGA Mesh X bits (from hard-coded config.h).");
    }
    if((1u<<FPGAMeshYLen) < FPGAMeshYLen ){
        throw std::runtime_error("Not enough FPGA Mesh Y bits (from hard-coded config.h).");
    }
    if((1u<<MailboxMeshXLen) < MailboxMeshXLen ){
        throw std::runtime_error("Not enough mailbox Mesh X bits (from hard-coded config.h).");
    }
    if((1u<<MailboxMeshYLen) < MailboxMeshYLen ){
        throw std::runtime_error("Not enough mailbox Mesh Y bits (from hard-coded config.h).");
    }

    p->FPGAMeshXLen=FPGAMeshXLen;
    p->FPGAMeshYLen=FPGAMeshYLen;
    p->MailboxMeshXLen=MailboxMeshXLen;
    p->MailboxMeshYLen=MailboxMeshYLen;

    p->FPGALinkCost=30;  // 128 bits/cycle at 200 MHz = 25 Gb/sec
    p->MailboxLinkCost=1;  // 1GBit/sec, realistically 0.8 Gb/sec

    return p;
}


template<class Tag>
Direction direction_to(Coord<Tag> from, Coord<Tag> to)
{
    if(to.y < from.y){
        return South;
    }
    if(to.y > from.y){
        return North;
    }
    if(to.x > from.x){
        return East;
    }
    if(to.x < from.x){
        return West;
    }
    return Here;
}

int direction_dx(Direction d)
{
    switch(d){
        case West: return -1;
        case East: return +1;
        default: return 0;
    }
}

int direction_dy(Direction d)
{
    switch(d){
        case North: return -1;
        case South: return +1;
        default: return 0;
    }
}

struct Link
{
protected:
    Link(const Node *_node, const std::string &_name)
        : node(_node)
        , name(_name)
        , _this(this)
    {
        assert(node!=nullptr);
        assert(!name.empty());
    }

    Link() = delete;
    Link(const Link &) = delete;
    Link &operator=(const Link &)=delete;
public:
    const Node *node;
    const std::string name;
    const Link *_this;
};

struct Node
{
protected:
    Node(Node *_parent, const std::string &_name)
        : parent(_parent)
        , name(_name)
        , full_name( _parent ? _parent->full_name+"/"+_name : "/"+_name )
    {
        if(_parent){
            _parent->subNodes.push_back(this);
        }
    }

public:
    Node() = delete;
    Node(const Node &) = delete;
    Node &operator=(const Node &) = delete;

    virtual ~Node()
    {}

    std::vector<const Node *> subNodes;
    std::vector<const LinkIn *> linksIn;
    std::vector<const LinkOut *> linksOut;

    const Node *parent = 0 ;
    const std::string name;
    const std::string full_name;


    virtual const SystemParameters * get_parameters() const
    { return parent->get_parameters(); }

    virtual FPGACoord get_fpga_coord(thread_id_t thread) const
    { return get_parameters()->get_fpga_coord(thread); }

    virtual MailboxCoord get_mailbox_coord(thread_id_t thread) const
    { return get_parameters()->get_mailbox_coord(thread); }

    virtual const Node *get_node_containing(thread_id_t dest) const
    {
        assert(parent);
        return parent->get_node_containing(dest);
    }

    virtual const LinkOut *get_link_to(thread_id_t /*dest*/) const
    {
        throw std::runtime_error(std::string("This node (")+full_name+") does not directly take part in routing.");
    }

    //! If the node is able, suggest where it should be positioned in a 2d graph
    virtual bool try_suggest_layout_position(double */*x*/, double */*y*/) const
    {
        return false;
    }

};

struct LinkIn
    : Link
{
    LinkIn(Node *_node, const std::string &_name)
        : Link(_node, _name)
    {
        _node->linksIn.push_back(this);
    }

    LinkOut *source = 0;
};

struct LinkOut
    : Link
{

    LinkOut(Node *_node, const std::string &_name)
        : Link(_node, _name)
        , type(UnknownLink)
    {
        _node->linksOut.push_back(this);
    }

    LinkType type;
    LinkIn *sink = 0;

    void connect(LinkIn *_partner, LinkType _type)
    {
        assert(_type!=UnknownLink);
        assert(node!=0 && _partner && _partner->node!=0);
        assert(sink==0 && _partner->source==0);
        sink=_partner;
        sink->source=this;
        type=_type;
    }
};



struct ExpanderNode
    : Node
{
    ExpanderNode(Node *_parent, const std::string name, unsigned length, Direction _input_dir)
        : Node(_parent, name)
        , input{this,"in"}
        , input_dir(_input_dir)
    {
        for(unsigned i=0; i<length; i++){
            outputs.emplace_back(std::make_unique<LinkOut>(this, "out["+std::to_string(i)+"]"));
        }
    }

    LinkIn input;
    std::vector<std::unique_ptr<LinkOut>> outputs;
    const Direction input_dir;

public:

    const LinkOut *get_link_to(thread_id_t /*dest*/) const override
    {
        return outputs[0].get(); // Tinsel always connects to first channel, and rest to null
    }
};

struct ReducerNode
    : Node
{
    ReducerNode() = delete;
    ReducerNode(const ReducerNode &) = delete;
    ReducerNode &operator=(const ReducerNode &) = delete;

    ReducerNode(Node *parent, const std::string name, unsigned length, Direction _output_dir)
        : Node(parent, name)
        , output{this, "out"}
        , output_dir(_output_dir)
    {
        for(unsigned i=0; i<length; i++){
            inputs.emplace_back(std::make_unique<LinkIn>(this, "in["+std::to_string(i)+"]"));
        }
    }

    std::vector<std::unique_ptr<LinkIn>> inputs;
    LinkOut output;
    Direction output_dir;

    const LinkOut *get_link_to(thread_id_t /*dest*/) const override
    {
        return &output;
    }
};

struct NullSourceNode
    : Node
{
    NullSourceNode(Node *parent, const std::string name)
        : Node(parent, name)
        , out{this,"out"}
    {}

    LinkOut out; 
};

struct NullSinkNode
    : Node
{
    NullSinkNode(Node *parent, const std::string name)
        : Node(parent, name)
        , in{this,"in"}
    {}

    LinkIn in; 
};

struct ClusterNode
    : Node
{
    ClusterNode(Node *parent)
        : Node(parent, "cluster")
        , in(this, "in")
        , out(this, "out")
    {}

    LinkIn in;
    LinkOut out;

    virtual const LinkOut *get_link_to(thread_id_t dest) const
    {
        if(get_node_containing(dest)==this){
            return nullptr;
        }else{
            return &out;
        }
    }
};

struct MailboxNode
    : Node
{
    MailboxNode() = delete;
    MailboxNode(const MailboxNode &) = delete;
    MailboxNode &operator=(const MailboxNode &) = delete;

    MailboxNode(Node *parent,const std::string name,  MailboxCoord _position)
        : Node(parent,name)
        , position(_position)
        , fpga(parent->get_parameters()->get_fpga_coord(_position))
        , inputs{LinkIn{this,"in[N]"},LinkIn{this,"in[S]"},LinkIn{this,"in[E]"},LinkIn{this,"in[W]"},LinkIn{this,"in[C]"}}
        , outputs{LinkOut{this,"out[N]"},LinkOut{this,"out[S]"},LinkOut{this,"out[E]"},LinkOut{this,"out[W]"},LinkOut{this,"out[C]"}}
        , cluster{std::make_unique<ClusterNode>(this)}
    {
        outputs[Here].connect(&cluster->in, InterMailboxLink);
        cluster->out.connect(&inputs[Here], InterMailboxLink);
    }

    const MailboxCoord position;
    const FPGACoord fpga;
    std::array<LinkIn,5> inputs;
    std::array<LinkOut,5> outputs;

    std::unique_ptr<ClusterNode> cluster;

    const Node *get_node_containing(thread_id_t dest) const override
    {
        auto p=get_mailbox_coord(dest);
        if(p==position){
            return cluster.get();
        }else{
            return parent->get_node_containing(dest);
        }
    }

    const LinkOut *get_link_to(thread_id_t dest) const override
    {
        auto dir=direction_to(position, get_mailbox_coord(dest));
        //fprintf(stderr, "  Mailbox::get_link_to(%u). (%u,%u) -> (%u,%u). Dir=%s\n", dest.value, position.x, position.y, get_mailbox_coord(dest).x, get_mailbox_coord(dest).y, dir_to_string(dir));
        return &outputs.at(dir);
    }

    bool try_suggest_layout_position(double *x, double *y) const override
    {
        *x=position.x*1000;
        *y=position.y*1000;
        return true;
    }
};

struct FPGANode
    : Node
{
    FPGANode() = delete;
    FPGANode(const FPGANode &) = delete;
    FPGANode &operator=(const FPGANode &) = delete;

protected:
    FPGANode(Node *parent, const std::string name, FPGACoord _position)
        : Node(parent, name)
        , position(_position)
    {}
public:
    const FPGACoord position;
};

struct SystemNode
    : Node
{
    SystemNode() = delete;
    SystemNode(const SystemNode &) = delete;
    SystemNode &operator=(const SystemNode &) = delete;

protected:
    SystemNode(const std::string name)
        : Node(nullptr, name)
    {}
public:
};

void validate_node_heirarchy(const Node *root)
{
    auto require=[](const Node *node, bool cond, const std::string &msg)
    {
        if(!cond){
            throw std::runtime_error("Invalid node heirarchy on node " + node->full_name + " : "+msg);
        }
    };

    auto require_link_in=[](const Node *node, const LinkIn *link, bool cond, const std::string &msg)
    {
        if(!cond){
            throw std::runtime_error("Invalid node heirarchy on node " + node->full_name + ", LinkIn "+link->name+ " : "+msg);
        }
    };

    // First build the set of reachable nodes based on subNode relationshop

    std::unordered_set<const Node *> nodes;

    std::function<void(const Node *)> collect_nodes_in_heirarchy = [&](const Node *node)
    {
        if(nodes.find(node)==nodes.end()){    
            nodes.insert(node);
            for(auto n : node->subNodes){
                require(n, n->parent==node, "Invalid parent/child relationship");
                collect_nodes_in_heirarchy(n);
            }
        }
    };
    collect_nodes_in_heirarchy(root);

    // Then walk the links and check that all links are connected, and all nodes are in reachable nodes
    for(auto n : nodes){
        for(auto in : n->linksIn){
            require(n, in->node==n, "LinkIn does not point to parent.");
            require(n, static_cast<const Link*>(in)==in->_this, "LinkIn has moved in memory.");
            require(n, !in->name.empty(), "LinkIn does not have a name");
            require_link_in(n, in, in->source!=nullptr, "LinkIn is not connected.");
            require_link_in(n, in, in->source->sink==in, "LinkIn source is not reflected.");
            require_link_in(n, in, nodes.find(in->source->node)!=nodes.end(), "LinkIn points at a non-reachable node.");
            const auto &linksOut=in->source->node->linksOut;
            require_link_in(n, in, std::find(linksOut.begin(),linksOut.end(),in->source)!=linksOut.end(),
                "LinkIn points to LinkOut which is not known by partner node ");
        }

        for(auto out : n->linksOut){
            require(n, out->node==n, "LinkOut does not point to parent.");
            require(n, out->sink!=nullptr, "LinkOut is not connected.");
            require(n, out->sink->source==out, "LinkOut sink is not reflected.");
            require(n, nodes.find(out->sink->node)!=nodes.end(), "LinkIn points at a non-reachable node.");
            require(n, out->type!=UnknownLink, "LinkOut does not have type.");
            const auto &linksIn=out->sink->node->linksIn;
            require(n, std::find(linksIn.begin(),linksIn.end(),out->sink)!=linksIn.end(),
                "LinkOut points to LinkIn which is not known by partner node ");
        }
    }

    std::mt19937 rng;

    unsigned max_hops_x = root->get_parameters()->FPGAMeshXLen*root->get_parameters()->MailboxMeshXLen;
    unsigned max_hops_y = root->get_parameters()->FPGAMeshYLen*root->get_parameters()->MailboxMeshYLen;
    unsigned max_hops=2*(max_hops_x+max_hops_y) + 8 + 8; // Big over-estimate
    root->get_parameters()->for_each_thread([&](FPGACoord fpga, MailboxCoord mbox, thread_id_t thread){
        auto *n = root->get_node_containing(thread);
        require(n, n->get_mailbox_coord(thread)==mbox, "Thread locator returns wrong mailbox");
        require(n, n->get_fpga_coord(thread)==fpga, "Thread locator returns wrong FPGA.");

        for(unsigned i=0; i<10; i++){
            auto t=root->get_parameters()->pick_random_thread(rng);
            auto *nt = root->get_node_containing(t);
            auto dst_fpga=root->get_fpga_coord(t);

            //fprintf(stderr, "%u -> %u, FPGA (%u,%u) -> FPGA (%u,%u)\n", thread.value, t.value, fpga.x, fpga.y, dst_fpga.x, dst_fpga.y);


            unsigned hops=0;
            auto *path=n;
            while(path!=nt){
                auto link=path->get_link_to(t);
                assert(link);
                //fprintf(stderr, "%u -> %u : %s:%s -> %s:%s\n", thread.value, t.value, path->full_name.c_str(), link->name.c_str(), link->sink->node->full_name.c_str(), link->sink->name.c_str() );
                path=link->sink->node;
                

                ++hops;
                if(hops > max_hops){
                    throw std::runtime_error("ROute could not be found.");
                }
            }
        }
    });

}



void print_node_heirarchy(std::ostream &dst, std::string prefix, const Node *node)
{
    dst<<prefix<<node->name<<"\n";
    for(auto n : node->subNodes){
        print_node_heirarchy(dst, prefix+"  ", n);
    }
}



struct AppGraph
{
    struct Vertex
    {
        unsigned index;  // must match the index in the vertices array
        std::string id;
        std::unordered_map<unsigned,unsigned> out;
    };

    std::string name;
    std::string generator;
    std::vector<Vertex> vertices;

    template<class TAlloc>
    rapidjson::Value save(TAlloc &alloc)
    {
        rapidjson::Value res;
        res.SetObject();
        res.AddMember("name", name, alloc);
        res.AddMember("type","app_graph",alloc);
        res.AddMember("generator", generator, alloc);

        rapidjson::Value vs;
        vs.SetArray();
        vs.Reserve(vertices.size(), alloc);
        unsigned count=0;
        for(const auto &v : vertices){
            if(v.index != vs.Size()){
                fprintf(stderr, "Index=%u, id=%s, got index=%u\n", vs.Size(), v.id.c_str(), v.index);
                throw std::runtime_error("Indices are not contiguous starting at zero.");
            }
            vs.PushBack( rapidjson::Value(v.id.c_str(),alloc), alloc);
        }
        res.AddMember("vertices", vs, alloc);

        rapidjson::Value es;
        es.SetArray();
        es.Reserve(vertices.size(), alloc);

        for(unsigned src=0; src<vertices.size(); src++){
            const auto &v=vertices[src];
            rapidjson::Value fanout;
            fanout.SetArray();
            fanout.Reserve(v.out.size(), alloc);
            for(const auto &kv : v.out){
                if(kv.second==0){
                    continue;
                }
                if(kv.second==1){
                    fanout.PushBack(kv.first, alloc);
                }else{
                    rapidjson::Value pair;
                    pair.SetArray();
                    pair.PushBack(kv.first, alloc);
                    pair.PushBack(kv.second, alloc);
                    fanout.PushBack(pair, alloc);
                }
            }
            es.PushBack(fanout, alloc);
        }

        res.AddMember("edges", es, alloc);
        return res;
    }

    void load(const rapidjson::Value &v)
    {
        assert(v["type"].GetString()==std::string("app_graph"));

        name=v["name"].GetString();
        generator=v["generator"].GetString();

        const auto &vs=v["vertices"];
        assert(vs.IsArray());
        vertices.clear();
        vertices.resize(vs.Size());

        std::unordered_set<std::string> ids;

        for(unsigned i=0; i<vs.Size(); i++){
            vertices[i].index=i;
            std::string id=vs[i].GetString();
            if(!ids.insert(id).second){
                throw std::runtime_error("Duplicate id in graph");
            }
            vertices[i].id=id;
        }
        
        const auto &es=v["edges"];
        assert(es.IsArray());
        assert(es.Size()==vs.Size());
        for(unsigned src=0; src<vertices.size(); src++){
            const auto &fanout=es[src]; 
            assert(fanout.IsArray());
            for(unsigned j=0; j<fanout.Size(); j++){
                const auto &e=fanout[j];
                uint32_t dest=-1;
                unsigned weight=1;
                if(e.IsNumber()){
                    dest=e.GetUint();
                }else{
                    assert(e.IsArray() && e.Size()==2);
                    dest=e[0].GetUint();
                    weight=e[1].GetUint();
                }
                if(dest>=vertices.size()){
                    throw std::runtime_error("Dest vertex nmber out of range");
                }
                vertices[src].out[dest]+=weight;
            }
        }
    }
};

struct AppPlacement
{
    std::string graph_id;
    SystemParameters system;
    std::vector<thread_id_t> placement; // Mapping of vertex to thread

    template<class TAlloc>
    rapidjson::Value save(TAlloc &alloc)
    {
        rapidjson::Value res;
        res.SetObject();
        res.AddMember("type","placement",alloc);
        res.AddMember("graph", graph_id, alloc);

        rapidjson::Value vs;
        vs.SetArray();
        vs.Reserve(placement.size(), alloc);
        unsigned count=0;
        for(unsigned i=0; i<placement.size(); i++){
            vs.PushBack( rapidjson::Value((uint64_t)placement[i].value), alloc);
        }
        res.AddMember("device_to_thread", vs, alloc);

        rapidjson::Value pp;
        pp=system.save(alloc);
        res.AddMember("system", pp, alloc);

        return res;
    }

    void load(const rapidjson::Value &v)
    {
        assert(v["type"].GetString()==std::string("placement"));

        graph_id=v["graph"].GetString();

        const auto &vs=v["device_to_thread"];
        assert(vs.IsArray());
        placement.clear();
        placement.resize(vs.Size());

        for(unsigned i=0; i<vs.Size(); i++){
            placement[i].value=vs[i].GetUint64();
        }

        system.load(v["system"]);
    }
};

struct Route
{
    route_id_t id;
    std::string foreign_id;
    thread_id_t source_thread;
    thread_id_t dest_thread;
    const Node *source_node;
    const Node *sink_node;
    std::vector<const LinkOut*> path;
    unsigned weight; // Parameter representing intensity of route, e.g. number of repeated edges or more frequent communication
    uint64_t cost;   // Cost describing the route in terms of hops, independent of the weight
    unsigned hops; // may be less than path.size()
    unsigned fpga_hops;
    unsigned mailbox_hops;
};

Route create_route(const Node *system, route_id_t id, thread_id_t source, thread_id_t dest, unsigned weight=1, const std::string &foreign_id="")
{
    Route res{
        id,
        foreign_id,
        source, dest,
        system->get_node_containing(source), system->get_node_containing(dest),
        {},
        weight,
        0,
        0,
        0,
        0
    };

    //std::cerr<<"Routing from "<<res.source_node->full_name<<" to "<<res.sink_node->full_name<<"\n";
    const Node *curr=res.source_node;
    while(curr != res.sink_node){
        auto link=curr->get_link_to(dest);
        //std::cerr<<"  "<<curr->full_name<<":"<<link->name<<" -> "<<link->sink->node->full_name<<":"<<link->sink->name<<"\n";

        res.cost += system->get_parameters()->get_link_cost(link->type);
        res.path.push_back(link);
        curr=link->sink->node;
        if(link->type!=WiringLink){
            res.hops++;
        }
        if(link->type==InterFPGALink){
            res.fpga_hops++;
        }
        if(link->type==InterMailboxLink){
            res.mailbox_hops++;
        }
    }

    return res;
}


struct Routing
{
    const Node *system;
    std::unordered_map<route_id_t,Route> routes;
    std::unordered_map<std::string,route_id_t> routes_by_foreign_id;
    std::unordered_map<const LinkOut*,std::unordered_set<route_id_t>> links;
    uint64_t cost = 0;

    uint64_t next_route_id =0 ;

    Routing(const Node *_system)
        : system(_system)
    {}

    void add_graph(const AppPlacement &p, const AppGraph &g)
    {
        assert(p.system==*system->get_parameters());
        assert(p.graph_id==g.name);

        for(unsigned src=0; src<g.vertices.size(); src++){
            for(const auto &dw : g.vertices[src].out){
                assert(dw.first < g.vertices.size());
                add_route( p.placement[src], p.placement[dw.first], dw.second );
            }
        }
    }

    route_id_t add_route(thread_id_t source, thread_id_t dest, int weight, const std::string foreign_id="")
    {
        route_id_t id{next_route_id++};
        
        auto it=routes.insert(std::make_pair(id, create_route(system, id, source, dest, weight)));
        const Route &r=it.first->second;
        for(auto l : r.path){
            links[l].insert(id);
        }
        if(!foreign_id.empty()){
            auto fit=routes_by_foreign_id.insert(std::make_pair(foreign_id,id));
            if(!fit.second){
                routes.erase(it.first);
                throw std::runtime_error("Duplicate foreign ids for route.");
            }
        }

        cost += r.cost;

        return id;
    }

    std::unordered_map<const LinkOut*,unsigned> calculate_link_load() const
    {
        std::unordered_map<const LinkOut*,unsigned> res;
        res.reserve(links.size());
        for(const auto &kv : links){
            unsigned sumWeight=0;
            for(const auto &rid : kv.second){
                sumWeight += routes.at(rid).weight;
            }
            res.insert(std::make_pair(kv.first, sumWeight));
        }
        return res; 
    }
};


void print_node_heirarchy_as_dot(std::ostream &dst, const Node *root,
    const std::unordered_map<const LinkOut*,std::vector<std::string>> &linkEdgeProperties = {}
)
{
    auto q=[](const std::string &n)
    { return "\""+n+"\""; };

    auto qn=[](const Node *n)
    { return "\""+n->full_name+"\""; };

    auto ql=[](const Link *l)
    { return "\""+l->node->full_name+":"+l->name+"\""; };

    std::function<void(const Node *)> visit_create = [&](const Node *node)
    {
        dst<<"  subgraph \"cluster_"<<node->full_name<<"\"{\n";
        for(auto n : node->subNodes){
            visit_create(n);
        }

        dst<<"  "<<qn(node)<<" [ shape = rectangle, label="<<q(node->name);
        //double x, y;
        /*if(node->try_suggest_layout_position(&x, &y)){
            dst<<", pos=\""<<x<<","<<y<<"!\"";
        }*/
        dst<<" ]; \n";

        for(auto li : node->linksIn){
            dst<<"  "<<ql(li)<<" [ shape = doublecircle , label="<<q(li->name)<<" ];\n";
            dst<<"  "<<ql(li)<<" -> "<<qn(node)<<";\n";
        }

        for(auto lo : node->linksOut){
            dst<<"  "<<ql(lo)<<" [ shape = circle , label="<<q(lo->name)<<" ];\n";
            dst<<"  "<<qn(node)<<" -> "<<ql(lo)<<";\n";
        }

        dst<<"  };\n";
    };

    std::function<void(const Node *)> visit_link = [&](const Node *node)
    {
        for(auto n : node->subNodes){
            visit_link(n);
        }
        
        for(auto lo : node->linksOut){
            dst<<"  "<<ql(lo)<<" -> "<<ql(lo->sink);
            auto it=linkEdgeProperties.find(lo);
            if(it!=linkEdgeProperties.end()){
                dst<<"[";
                auto &v=it->second;
                for(unsigned i=0; i<v.size(); i++){
                    if(i!=0){
                        dst<<",";
                    }
                    dst<<v[i];
                }
                dst<<"]";
            }
            dst<<";\n";
        }
    };

    dst<<"digraph G{\n";
    dst<<"  overlap = false;\n";
    visit_create(root);
    visit_link(root);
    dst<<"}";
}

void print_node_heirarchy_as_dot_no_links(std::ostream &dst, const Node *root,
    const std::unordered_map<const LinkOut*,std::vector<std::string>> &linkEdgeProperties = {}
)
{
    auto q=[](const std::string &n)
    { return "\""+n+"\""; };

    auto qn=[](const Node *n)
    { return "\""+n->full_name+"\""; };

    auto ql=[](const Link *l)
    { return "\""+l->node->full_name+":"+l->name+"\""; };

    std::function<void(const Node *)> visit_create = [&](const Node *node)
    {
        bool doSubgraph=dynamic_cast<const SystemNode*>(node)==0;
        if(doSubgraph){
            dst<<"  subgraph \"cluster_"<<node->full_name<<"\"{\n";
        }
        for(auto n : node->subNodes){
            visit_create(n);
        }

        dst<<"  "<<qn(node)<<" [ shape = rectangle, label="<<q(node->name)<<"]";

        if(doSubgraph){
            dst<<"  };\n";
        }
    };

    std::function<void(const Node *)> visit_link = [&](const Node *node)
    {
        for(auto n : node->subNodes){
            visit_link(n);
        }
        
        for(auto lo : node->linksOut){
            dst<<"  "<<qn(lo->node)<<" -> "<<qn(lo->sink->node);
            auto it=linkEdgeProperties.find(lo);
            if(it!=linkEdgeProperties.end()){
                dst<<"[";
                auto &v=it->second;
                for(unsigned i=0; i<v.size(); i++){
                    if(i!=0){
                        dst<<",";
                    }
                    dst<<v[i];
                }
                dst<<"]";
            }
            dst<<";\n";
        }
    };

    dst<<"digraph G{\n";
    dst<<"  overlap = false;\n";
    visit_create(root);
    visit_link(root);
    dst<<"}";
}


#endif
