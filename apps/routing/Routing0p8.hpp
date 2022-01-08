#ifndef Routing0p8_hpp
#define Routing0p8_hpp


#include "Routing.hpp"

struct ProgRouter
    : Node
{
    ProgRouter() = delete;
    ProgRouter(const ProgRouter &) = delete;
    ProgRouter &operator=(const ProgRouter &) = delete;

    ProgRouter(Node *parent,const std::string name)
        : Node(parent,name)
        , position(dynamic_cast<FPGANode*>(parent)->position)
    {
        unsigned meshWidth=parent->get_parameters()->MailboxMeshXLen;

        std::vector<std::string> indices{"N","S","E","W"};
        for(unsigned i=0;i<meshWidth; i++){
            indices.push_back(std::to_string(i));
        }

        inter_in.reserve(4);
        inter_out.reserve(4);

        for(int i=0; i<4; i++){
            auto index=indices[i];
            inter_in.emplace_back(std::make_unique<LinkIn>(this, "i["+index+"]"));
            inter_out.emplace_back(std::make_unique<LinkOut>(this, "o["+index+"]"));
        }

        mbox_in.reserve(meshWidth);
        mbox_out.reserve(meshWidth);
        for(unsigned i=4; i<indices.size(); i++){
            auto index=indices[i];
            mbox_in.emplace_back(std::make_unique<LinkIn>(this, "i["+index+"]"));
            mbox_out.emplace_back(std::make_unique<LinkOut>(this, "o["+index+"]"));
        }        
    }

    const FPGACoord position;

    std::vector<std::unique_ptr<LinkIn>> inter_in;
    std::vector<std::unique_ptr<LinkOut>> inter_out;
    std::vector<std::unique_ptr<LinkIn>> mbox_in;
    std::vector<std::unique_ptr<LinkOut>> mbox_out;

    const LinkOut *get_link_to(thread_id_t dest) const override
    {
        // The programmable router is a full cross-bar, and
        // will route directly into the correct column of the mailbox mesh:
        // https://github.com/POETSII/tinsel/blob/3f0a3863b8f6a6a4052e536635c1bd164bc84938/rtl/ProgRouter.bsv#L878

        auto dest_fpga=get_fpga_coord(dest);
        if(dest_fpga==position){
            auto mbox=get_parameters()->get_mailbox_offset_in_fpga(dest);
            auto x=mbox.x;
            return mbox_out.at(x).get();
        }else{
            auto dir=direction_to(position, dest_fpga);
            return inter_out.at(dir).get();
        }
    }

    bool try_suggest_layout_position(double *x, double *y) const override
    {
        *x=position.x*1000;
        *y=position.y*1000;
        return true;
    }
};

struct MailboxNode0p8
    : MailboxNode
{
    MailboxNode0p8(Node *parent,const std::string name,  MailboxCoord _position)
        : MailboxNode(parent, name, _position)
    {
    }

    const LinkOut *get_link_to(thread_id_t dest) const override
    {
        auto fpga=get_fpga_coord(dest);
        if(fpga==this->fpga){
            return MailboxNode::get_link_to(dest);
        }else{
            return linksOut[South];
        }
    }
};

struct FPGANode0p8
    : FPGANode
{
    FPGANode0p8() = delete;
    FPGANode0p8(const FPGANode0p8 &) = delete;
    FPGANode0p8 &operator=(const FPGANode0p8 &) = delete;

    FPGANode0p8(Node *parent, const std::string name, FPGACoord _position)
        : FPGANode(parent, name, _position)
        , meshLengthX(parent->get_parameters()->MailboxMeshXLen)
        , meshLengthY(parent->get_parameters()->MailboxMeshYLen)
        , mailboxOrigin{ _position.x*meshLengthX, _position.y*meshLengthY }
        , router{this, "router"}
    {
        for(unsigned i=0; i<meshLengthX; i++){
            null_sinks_north.push_back(std::make_unique<NullSinkNode>(this, "nsink[N]["+std::to_string(i)+"]"));
            null_sources_north.push_back(std::make_unique<NullSourceNode>(this, "nsrc[N]["+std::to_string(i)+"]"));
        }
        for(unsigned i=0; i<meshLengthY; i++){
            null_sinks_east.push_back(std::make_unique<NullSinkNode>(this, "nsink[E]["+std::to_string(i)+"]"));
            null_sources_east.push_back(std::make_unique<NullSourceNode>(this, "nsrc[E]["+std::to_string(i)+"]"));

            null_sinks_west.push_back(std::make_unique<NullSinkNode>(this, "nsink[W]["+std::to_string(i)+"]"));
            null_sources_west.push_back(std::make_unique<NullSourceNode>(this, "nsrc[W]["+std::to_string(i)+"]"));
        }

        mailboxes.resize(meshLengthX);
        for(unsigned x=0; x<meshLengthX; x++){
            mailboxes[x].resize(meshLengthY);
            for(unsigned y=0; y<meshLengthY; y++){
                std::stringstream tmp;
                tmp<<"mbox["<<x<<"]["<<y<<"]";
                auto mbox=std::make_unique<MailboxNode0p8>(this, tmp.str(), MailboxCoord{
                    mailboxOrigin.x+x, mailboxOrigin.y+y
                });
                mailboxes[x][y]=std::move(mbox);
            }
        }

        for(unsigned x=0; x<meshLengthX; x++){
            for(unsigned y=0; y<meshLengthY; y++){
                auto &mbox=*mailboxes[x][y];

                bool northEdge = y==meshLengthY-1;
                bool southEdge = y==0;
                bool eastEdge = x==meshLengthX-1;
                bool westEdge = x==0;

                mbox.outputs[North].connect( northEdge ?  &null_sinks_north[x]->in : &mailboxes[x][y+1]->inputs[South], northEdge ? WiringLink : InterMailboxLink );
                mbox.outputs[South].connect( southEdge ?  router.mbox_in[x].get() : &mailboxes[x][y-1]->inputs[North], southEdge ? WiringLink : InterMailboxLink  );
                mbox.outputs[East].connect(  eastEdge  ?  &null_sinks_east[y]->in :  &mailboxes[x+1][y]->inputs[West], eastEdge ? WiringLink : InterMailboxLink  );
                mbox.outputs[West].connect(  westEdge  ?  &null_sinks_west[y]->in :  &mailboxes[x-1][y]->inputs[East], westEdge ? WiringLink : InterMailboxLink  );

                if(northEdge){
                    null_sources_north[x]->out.connect( &mbox.inputs[North], WiringLink );
                }
                if(southEdge){
                    router.mbox_out[x]->connect( &mbox.inputs[South], InterMailboxLink);
                }
                if(eastEdge){
                    null_sources_east[y]->out.connect( &mbox.inputs[East], WiringLink );
                }
                if(westEdge){
                    null_sources_west[y]->out.connect( &mbox.inputs[West], WiringLink );
                }
            }
        }
    }

    const unsigned meshLengthX;
    const unsigned meshLengthY;
    const MailboxCoord mailboxOrigin;

    ProgRouter router;

    std::vector<std::vector<std::unique_ptr<MailboxNode>>> mailboxes;

    std::vector<std::unique_ptr<NullSinkNode>> null_sinks_east, null_sinks_west, null_sinks_north;
    std::vector<std::unique_ptr<NullSourceNode>> null_sources_east, null_sources_west, null_sources_north;

    const MailboxNode *get_mailbox(int local_x, int local_y) const
    {
        return mailboxes.at(local_x).at(local_y).get();
    }

    const Node *get_node_containing(thread_id_t dest) const override
    {
        auto pf=get_fpga_coord(dest);
        if(pf==position){
            auto pm=get_mailbox_coord(dest);
            return get_mailbox(pm.x-mailboxOrigin.x, pm.y-mailboxOrigin.y)->get_node_containing(dest);
        }else{
            return parent->get_node_containing(dest);
        }
    }
};

struct SystemNode0p8
    : SystemNode
{
    SystemNode0p8(const SystemParameters *_parameters)
        : SystemNode("sys")
        , parameters(_parameters)
        , fpgaCountX(_parameters->FPGAMeshXLen)
        , fpgaCountY(_parameters->FPGAMeshYLen)
        , mailboxMeshXLen(_parameters->MailboxMeshXLen)
        , mailboxMeshYLen(_parameters->MailboxMeshYLen)
    {
        fpgas.resize(fpgaCountX);
        for(unsigned x=0; x<fpgaCountX; x++){
            fpgas[x].resize(fpgaCountY);
            for(unsigned y=0; y<fpgaCountY; y++){
                std::stringstream tmp;
                tmp<<"fpga["<<x<<"]["<<y<<"]";
                fpgas.at(x).at(y)=std::make_unique<FPGANode0p8>(this, tmp.str(), FPGACoord{x,y});
            }
        }

        for(unsigned i=0; i<fpgaCountX; i++){
            null_sinks[North].push_back(std::make_unique<NullSinkNode>(this, "null_sink[North]["+std::to_string(i)+"]"));
            null_sinks[South].push_back(std::make_unique<NullSinkNode>(this, "null_sink[South]["+std::to_string(i)+"]"));
            null_sources[North].push_back(std::make_unique<NullSourceNode>(this, "null_source[North]["+std::to_string(i)+"]"));
            null_sources[South].push_back(std::make_unique<NullSourceNode>(this, "null_source[South]["+std::to_string(i)+"]"));
        }

        for(unsigned i=0; i<fpgaCountY; i++){
            null_sinks[East].push_back(std::make_unique<NullSinkNode>(this, "null_sink[East]["+std::to_string(i)+"]"));
            null_sinks[West].push_back(std::make_unique<NullSinkNode>(this, "null_sink[West]["+std::to_string(i)+"]"));
            null_sources[East].push_back(std::make_unique<NullSourceNode>(this, "null_source[East]["+std::to_string(i)+"]"));
            null_sources[West].push_back(std::make_unique<NullSourceNode>(this, "null_source[West]["+std::to_string(i)+"]"));
        }

        for(unsigned x=0; x<fpgaCountX; x++){
            for(unsigned y=0; y<fpgaCountY; y++){
                bool northEdge = y==fpgaCountY-1;
                bool southEdge = y==0;
                bool eastEdge = x==fpgaCountX-1;
                bool westEdge = x==0;
                
                auto &fpga=*fpgas[x][y];

                fpga.router.inter_out[North]->connect( northEdge ? &null_sinks[North][x]->in : fpgas[x][y+1]->router.inter_in[South].get(), northEdge ? WiringLink : InterFPGALink );
                fpga.router.inter_out[South]->connect( southEdge ? &null_sinks[South][x]->in : fpgas[x][y-1]->router.inter_in[North].get(), southEdge ? WiringLink : InterFPGALink  );
                fpga.router.inter_out[East]->connect( eastEdge ? &null_sinks[East][y]->in : fpgas[x+1][y]->router.inter_in[West].get(), eastEdge ? WiringLink : InterFPGALink  );
                fpga.router.inter_out[West]->connect( westEdge ? &null_sinks[West][y]->in : fpgas[x-1][y]->router.inter_in[East].get(), westEdge ? WiringLink : InterFPGALink  );
            
                if(northEdge){ null_sources[North][x]->out.connect( fpga.router.inter_in[North].get(), WiringLink); }
                if(southEdge){ null_sources[South][x]->out.connect( fpga.router.inter_in[South].get(), WiringLink); }
                if(eastEdge){  null_sources[East][y]->out.connect( fpga.router.inter_in[East].get(), WiringLink); }
                if(westEdge){  null_sources[West][y]->out.connect( fpga.router.inter_in[West].get(), WiringLink); }
            }
        }
    }

    const SystemParameters * const parameters;

    const unsigned fpgaCountX, fpgaCountY;
    const unsigned mailboxMeshXLen, mailboxMeshYLen;

    std::vector<std::vector<std::unique_ptr<FPGANode0p8>>> fpgas;
    std::array<std::vector<std::unique_ptr<NullSinkNode>>,4> null_sinks;
    std::array<std::vector<std::unique_ptr<NullSourceNode>>,4> null_sources;

    const SystemParameters * get_parameters() const override
    {
        return parameters;    
    }

    const FPGANode0p8 *get_fpga(int local_x, int local_y) const
    {
        return fpgas.at(local_x).at(local_y).get();
    }

    const Node *get_node_containing(thread_id_t dest) const override
    {
        auto pf=get_fpga_coord(dest);
        return get_fpga(pf.x, pf.y)->get_node_containing(dest);
    }
};



#endif
