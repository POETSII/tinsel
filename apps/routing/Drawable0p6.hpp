#ifndef Drawable0p6_hpp
#define Drawable0p6_hpp

#include "Drawable.hpp"

#include "Routing0p6.hpp"

class FPGA0p6Drawable
    : public Drawable
{
private:
//    std::unique_ptr<Drawable> mailboxes;
    
    std::unique_ptr<Drawable> sections;
public:
    FPGA0p6Drawable(const FPGANode0p6 *_fpga)
        : fpga(_fpga)
    {
        std::vector<std::vector<std::unique_ptr<Drawable>>> elements;
        elements.resize(_fpga->meshLengthX);
        for(unsigned x=0; x<_fpga->meshLengthX; x++){
            elements[x].resize(_fpga->meshLengthY);
            for(unsigned y=0; y<_fpga->meshLengthY; y++){
                const MailboxNode *node=_fpga->get_mailbox(x,y);
                elements[x][y]=std::make_unique<MailboxDrawable>(node);
            }
        }
        auto mailboxes=std::make_unique<Grid2DDrawable>(std::move(elements));

        elements.resize(3);
        for(auto &r : elements){ r.resize(3); }
        elements[0][1]=stack_vertical(
            std::make_unique<ExpanderDrawable>(&_fpga->expanders[West]),
            std::make_unique<ReducerDrawable>(&_fpga->reducers[West])
        );
        elements[2][1]=stack_vertical(
            std::make_unique<ReducerDrawable>(&_fpga->reducers[East]),
            std::make_unique<ExpanderDrawable>(&_fpga->expanders[East])
        );
        elements[1][2]=stack_horizontal(
            std::make_unique<ExpanderDrawable>(&_fpga->expanders[North]),
            std::make_unique<ReducerDrawable>(&_fpga->reducers[North])
        );
        elements[1][0]=stack_horizontal(
            std::make_unique<ReducerDrawable>(&_fpga->reducers[South]),
            std::make_unique<ExpanderDrawable>(&_fpga->expanders[South])
        );
        elements[1][1]=std::move(mailboxes);

        sections=std::make_unique<Table2DDrawable>(std::move(elements));
    }

    const FPGANode0p6 * const fpga;

    const double pad=5;

    Size size() const override
    { return sections->size()+Size{pad*2,pad*2}; }

    void draw(SVGWriter &dst, Point topLeft, LinkMap &map) const override
    {
        dst.begin_group();
        dst.text(topLeft+Size{50,20}, fpga->name);

        dst.rectangle(topLeft, topLeft+size());
        sections->draw(dst, topLeft+Size{pad,pad}, map);
        dst.end_group();
    }
};

class System0p6Drawable
    : public Drawable
{
private:
    std::unique_ptr<Grid2DDrawable> fpgas;

    const double pad=5;

    
public:
    System0p6Drawable(const SystemNode0p6 *_system)
        : system(_system)
    {
        std::vector<std::vector<std::unique_ptr<Drawable>>> elements;
        elements.resize(_system->fpgaCountX);
        for(unsigned x=0; x<_system->fpgaCountX; x++){
            elements[x].resize(_system->fpgaCountY);
            for(unsigned y=0; y<_system->fpgaCountY; y++){
                const FPGANode0p6 *node=_system->get_fpga(x,y);
                elements[x][y]=std::make_unique<FPGA0p6Drawable>(node);
            }
        }
        fpgas=std::make_unique<Grid2DDrawable>(std::move(elements));
    }

    const SystemNode0p6 * const system;

    Size size() const override
    { return fpgas->size()+Size{pad*2,pad*2}; }

    void draw(SVGWriter &dst, Point topLeft, LinkMap &map) const override
    {
        dst.rectangle(topLeft, topLeft+size());
        fpgas->draw(dst, topLeft+Size{pad,pad}, map);
    }
};

#endif
