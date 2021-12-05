#ifndef Drawable0p8_hpp
#define Drawable0p8_hpp

#include "Drawable.hpp"

#include "Routing0p8.hpp"

class ProgRouterDrawable
    : public Drawable
{
public:
    const double pad=20;
    const double linkWidth=100;

    ProgRouterDrawable(const ProgRouter *node, double _meshWidth)
        : router(node)
        , meshWidth(_meshWidth)
        , width(_meshWidth+linkWidth+pad*2)
        , height(linkWidth+pad*2)
    {

    }

    const ProgRouter * const router;

    const double meshWidth;

    const double width, height;

    Size size() const override
    { return {width,height}; }

    void draw(SVGWriter &dst, Point topLeft, LinkMap &map) const override
    {
        Size meshBox{meshWidth,height};
        Size linkSize{linkWidth,height};
        Size routerSize{linkWidth+meshWidth,height};

        topLeft=topLeft+Size{pad,pad};

        Point meshTopLeft=topLeft+Size{linkWidth, 0};
        Point linkTopLeft=topLeft;

        dst.rectangle(topLeft, topLeft+routerSize);
        Point mid=topLeft+Size{width*0.5,height*0.5};
        dst.text(mid, router->name);

        unsigned meshLenX = router->mbox_in.size();

        for(unsigned i=0; i<meshLenX; i++){
            draw_link_in(dst, meshTopLeft, meshBox, South, meshLenX*2, 2*i, router->mbox_in[i].get(), map);
            draw_link_out(dst, meshTopLeft, meshBox, South, meshLenX*2, 2*i+1, router->mbox_out[i].get(), map);
        }
    
        draw_link_in(dst, linkTopLeft, linkSize, South, 2, 0, router->inter_in[North].get(), map);
        draw_link_out(dst, linkTopLeft, linkSize, South, 2, 1, router->inter_out[North].get(), map);

        draw_link_in(dst, linkTopLeft, linkSize, North, 2, 1, router->inter_in[South].get(), map);
        draw_link_out(dst, linkTopLeft, linkSize, North, 2, 0, router->inter_out[South].get(), map);

        draw_link_in(dst, topLeft, routerSize, West, 2, 0, router->inter_in[East].get(), map);
        draw_link_out(dst, topLeft, routerSize, West, 2, 1, router->inter_out[East].get(), map);

        draw_link_in(dst, topLeft, routerSize, East, 2, 1, router->inter_in[West].get(), map);
        draw_link_out(dst, topLeft, routerSize, East, 2, 0, router->inter_out[West].get(), map);

    }

};

class FPGA0p8Drawable
    : public Drawable
{
private:
//    std::unique_ptr<Drawable> mailboxes;
    
    std::unique_ptr<Drawable> sections;
public:
    FPGA0p8Drawable(const FPGANode0p8 *_fpga)
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
        double width=mailboxes->size().w;

        std::vector<std::vector<std::unique_ptr<Drawable>>> parts;
        parts.resize(1);
        parts[0].clear();
        parts[0].push_back( std::move(std::make_unique<ProgRouterDrawable>(&_fpga->router, width)) );
        parts[0].push_back( std::move(mailboxes) );
        sections=std::make_unique<Table2DDrawable>(
            std::move(parts)
        );
    }

    const FPGANode0p8 * const fpga;

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

class System0p8Drawable
    : public Drawable
{
private:
    std::unique_ptr<Grid2DDrawable> fpgas;

    const double pad=5;

    
public:
    System0p8Drawable(const SystemNode0p8 *_system)
        : system(_system)
    {
        std::vector<std::vector<std::unique_ptr<Drawable>>> elements;
        elements.resize(_system->fpgaCountX);
        for(unsigned x=0; x<_system->fpgaCountX; x++){
            elements[x].resize(_system->fpgaCountY);
            for(unsigned y=0; y<_system->fpgaCountY; y++){
                const FPGANode0p8 *node=_system->get_fpga(x,y);
                elements[x][y]=std::make_unique<FPGA0p8Drawable>(node);
            }
        }
        fpgas=std::make_unique<Grid2DDrawable>(std::move(elements));
    }

    const SystemNode0p8 * const system;

    Size size() const override
    { return fpgas->size()+Size{pad*2,pad*2}; }

    void draw(SVGWriter &dst, Point topLeft, LinkMap &map) const override
    {
        dst.rectangle(topLeft, topLeft+size());
        fpgas->draw(dst, topLeft+Size{pad,pad}, map);
    }
};

#endif
