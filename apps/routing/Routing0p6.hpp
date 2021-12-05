#ifndef Routing0p6_hpp
#define Routing0p6_hpp


#include "Routing.hpp"


struct FPGANode0p6
    : FPGANode
{
    FPGANode0p6() = delete;
    FPGANode0p6(const FPGANode0p6 &) = delete;
    FPGANode0p6 &operator=(const FPGANode0p6 &) = delete;

    FPGANode0p6(Node *parent, const std::string name, FPGACoord _position)
        : FPGANode(parent, name, _position)
        , meshLengthX(parent->get_parameters()->MailboxMeshXLen)
        , meshLengthY(parent->get_parameters()->MailboxMeshYLen)
        , mailboxOrigin{ _position.x*meshLengthX, _position.y*meshLengthY }
        , expanders{{ // Expanders in North, South, East, West order
          { this, "Expander[North]", meshLengthX , North }, { this, "Expander[South]", meshLengthX, South },
          { this, "Expander[East]", meshLengthY , East },  { this, "Expander[West]", meshLengthY, West }   
        }}
        , reducers{{ // Reducers in North, South, East, West order
          { this, "Reducer[North]", meshLengthX, North }, { this, "Reducer[South]", meshLengthX, South },
          { this, "Reducer[East]", meshLengthY, East },  { this, "Reducer[West]", meshLengthY, West }   
        }}
    {
        mailboxes.resize(meshLengthX);
        for(unsigned x=0; x<meshLengthX; x++){
            mailboxes[x].resize(meshLengthY);
            for(unsigned y=0; y<meshLengthY; y++){
                std::stringstream tmp;
                tmp<<"mbox["<<x<<"]["<<y<<"]";
                auto mbox=std::make_unique<MailboxNode>(this, tmp.str(), MailboxCoord{
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

                mbox.outputs[North].connect( northEdge ?  reducers[North].inputs[x].get() : &mailboxes[x][y+1]->inputs[South] );
                mbox.outputs[South].connect( southEdge ?  reducers[South].inputs[x].get() : &mailboxes[x][y-1]->inputs[North] );
                mbox.outputs[East].connect(  eastEdge  ?  reducers[East].inputs[y].get() :  &mailboxes[x+1][y]->inputs[West] );
                mbox.outputs[West].connect(  westEdge  ?  reducers[West].inputs[y].get() :  &mailboxes[x-1][y]->inputs[East] );
                
                if(northEdge){ expanders[North].outputs[x]->connect( &mbox.inputs[North] ); }
                if(southEdge){ expanders[South].outputs[x]->connect( &mbox.inputs[South] ); }
                if(eastEdge){  expanders[East].outputs[y]->connect( &mbox.inputs[East] ); }
                if(westEdge){  expanders[West].outputs[y]->connect( &mbox.inputs[West] ); }
            }
        }
    }

    const unsigned meshLengthX;
    const unsigned meshLengthY;
    const MailboxCoord mailboxOrigin;

    std::array<ExpanderNode,4> expanders;
    std::array<ReducerNode,4> reducers;

    std::vector<std::vector<std::unique_ptr<MailboxNode>>> mailboxes;

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

struct SystemNode0p6
    : SystemNode
{
    SystemNode0p6(const SystemParameters *_parameters)
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
                fpgas.at(x).at(y)=std::make_unique<FPGANode0p6>(this, tmp.str(), FPGACoord{x,y});
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

                fpga.reducers[North].output.connect( northEdge ? &null_sinks[North][x]->in  : &fpgas[x][y+1]->expanders[South].input );
                fpga.reducers[South].output.connect( southEdge ? &null_sinks[South][x]->in  : &fpgas[x][y-1]->expanders[North].input );
                fpga.reducers[East].output.connect( eastEdge   ? &null_sinks[East][y]->in   : &fpgas[x+1][y]->expanders[West].input );
                fpga.reducers[West].output.connect( westEdge   ? &null_sinks[West][y]->in   : &fpgas[x-1][y]->expanders[East].input );
            
                if(northEdge){ null_sources[North][x]->out.connect( &fpga.expanders[North].input); }
                if(southEdge){ null_sources[South][x]->out.connect( &fpga.expanders[South].input); }
                if(eastEdge){  null_sources[East][y]->out.connect( &fpga.expanders[East].input); }
                if(westEdge){  null_sources[West][y]->out.connect( &fpga.expanders[West].input); }
            }
        }
    }

    const SystemParameters * const parameters;

    const unsigned fpgaCountX, fpgaCountY;
    const unsigned mailboxMeshXLen, mailboxMeshYLen;

    std::vector<std::vector<std::unique_ptr<FPGANode0p6>>> fpgas;
    std::array<std::vector<std::unique_ptr<NullSinkNode>>,4> null_sinks;
    std::array<std::vector<std::unique_ptr<NullSourceNode>>,4> null_sources;

    const SystemParameters * get_parameters() const override
    {
        return parameters;    
    }

    const FPGANode0p6 *get_fpga(int local_x, int local_y) const
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
