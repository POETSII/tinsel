#ifndef Drawable_hpp
#define Drawable_hpp

#include "Routing0p6.hpp"

#include "rapidjson/filereadstream.h"

#include "write_svg.hpp"

using Size = SVGWriter::Size;
using Point = SVGWriter::Point;

struct LinkMap
{
    std::unordered_map<const LinkOut*,Point> linksOut;
    std::unordered_map<const LinkIn*,Point> linksIn;
};

class Drawable
{
public:
    virtual ~Drawable()
    {}

    virtual Size size() const =0;
    virtual void draw(
        SVGWriter &dst, Point topLeft, LinkMap &map
    ) const =0;
};

void draw_centered(SVGWriter &dst, Point p, Size box, const Drawable &d, LinkMap &map)
{
    Size s=d.size();
    assert(s.w <= box.w && s.h <= box.h);
    d.draw( dst, { p.x + (box.w-s.w)/2 , p.y + (box.h-s.h)/2 }, map );
}

Point calc_link_pos(std::pair<Point,Point> line, unsigned pinCount, unsigned pinOff )
{
    assert(pinOff < pinCount);
    double p=(pinOff+1)/(double)(pinCount+1);
    return {
        line.first.x + (line.second.x-line.first.x)*p,
        line.first.y + (line.second.y-line.first.y)*p,
    };
}

std::pair<Point,Point> find_side(Point p, Size box, Direction side)
{
    switch(side){
    case North:    return std::make_pair( p, p+Size{box.w,0.0} );
    case South:    return std::make_pair( p+Size{0.0,box.h}, p+Size{box.w,box.h} );
    case East:     return std::make_pair( p, p+Size{0.0,box.h} );
    case West:     return std::make_pair( p+Size{box.w,0.0}, p+Size{box.w,box.h} );
    default: throw std::runtime_error("Invalid size");
    }
}

void draw_link_in(SVGWriter &dst, Point p, Size box, Direction side, unsigned pinCount, unsigned pinOff, const LinkIn *link, LinkMap &map )
{
    auto line=find_side(p, box, side);
    auto pos=calc_link_pos(line, pinCount, pinOff);
    dst.circle(pos, 2);
    map.linksIn[link]=pos;
}

void draw_link_out(SVGWriter &dst, Point p, Size box, Direction side, unsigned pinCount, unsigned pinOff, const LinkOut *link, LinkMap &map )
{
    auto line=find_side(p, box, side);
    auto pos=calc_link_pos(line, pinCount, pinOff);
    dst.circle(pos, 2);
    map.linksOut[link]=pos;
}

class Grid2DDrawable
    : public Drawable
{
private:
    Size elt_size;
public:    
    Grid2DDrawable(std::vector<std::vector<std::unique_ptr<Drawable>>> &&_elements)
        : elements(std::move(_elements))
        , ncols(elements.size())
        , nrows(elements.at(0).size())
    {
        assert(ncols>0 && nrows>0);

        Size m{0.0,0.0};
        for(unsigned x=0; x<ncols; x++){
            for(unsigned y=0; y<nrows; y++){
                if(elements[x][y]){
                    Size es=elements[x][y]->size();
                    m.w=std::max(m.w, es.w);
                    m.h=std::max(m.h, es.h);
                }
            }
            assert(nrows==elements[x].size());
        }

        elt_size=m;
    }

    const std::vector<std::vector<std::unique_ptr<Drawable>>> elements;
    const unsigned ncols;
    const unsigned nrows;

    const double pad = 50;

    Size size() const override
    {
        return {
            ncols*elt_size.w + (ncols+1)*pad,
            nrows*elt_size.h + (nrows+1)*pad,
        };
    }

    void draw(SVGWriter &dst, Point topLeft, LinkMap &map) const override
    {
        for(unsigned x=0; x<ncols; x++){
            for(unsigned y=0; y<nrows; y++){
                Point p={
                    topLeft.x+pad+(x*(elt_size.w+pad)),
                    topLeft.y+pad+y*(elt_size.h+pad),
                };
                if(elements[x][y]){
                    elements[x][y]->draw(dst,p, map);
                }
            }
        }
    }
};

std::unique_ptr<Drawable> stack_vertical(std::unique_ptr<Drawable> &&a, std::unique_ptr<Drawable> &&b)
{
    std::vector<std::vector<std::unique_ptr<Drawable>>> elements;
    elements.resize(1);
    elements[0].push_back(std::move(a));
    elements[0].push_back(std::move(b));
    return std::make_unique<Grid2DDrawable>(std::move(elements));
}

std::unique_ptr<Drawable> stack_horizontal(std::unique_ptr<Drawable> &&a, std::unique_ptr<Drawable> &&b)
{
    std::vector<std::vector<std::unique_ptr<Drawable>>> elements;
    elements.resize(2);
    elements[0].push_back(std::move(a));
    elements[1].push_back(std::move(b));
    return std::make_unique<Grid2DDrawable>(std::move(elements));
}


class Table2DDrawable
    : public Drawable
{

    std::vector<double> m_col_widths;
    std::vector<double> m_row_heights;

    Size m_size{0.0,0.0};

public:    
    Table2DDrawable(std::vector<std::vector<std::unique_ptr<Drawable>>> &&_elements)
        : elements(std::move(_elements))
        , ncols(elements.size())
        , nrows(elements.at(0).size())
    {
        assert(ncols>0 && nrows>0);

        m_col_widths.resize(ncols, 0.0);
        m_row_heights.resize(nrows, 0.0);

        for(unsigned x=0; x<ncols; x++){
            for(unsigned y=0; y<nrows; y++){
                if(elements[x][y]){
                    Size es=elements[x][y]->size();
                    m_col_widths[x]=std::max(m_col_widths[x], es.w);
                    m_row_heights[y]=std::max(m_row_heights[y], es.h);
                }
            }
            assert(nrows==elements[x].size());
        }


        m_size.w += (ncols+1)*pad;
        for(unsigned x=0; x<ncols; x++){
            m_size.w += m_col_widths[x];
        }
        m_size.h += (nrows+1)*pad;
        for(unsigned y=0; y<nrows; y++){
            m_size.h += m_row_heights[y];
        }
    }

    const std::vector<std::vector<std::unique_ptr<Drawable>>> elements;
    const unsigned ncols;
    const unsigned nrows;

    const double pad = 2;

    Size size() const override
    {
        return m_size;
    }

    void draw(SVGWriter &dst, Point topLeft, LinkMap &map) const override
    {
        double xOff=topLeft.x+pad;
        for(unsigned x=0; x<ncols; x++){
            double yOff=topLeft.y;
            for(unsigned y=0; y<nrows; y++){
                if(elements[x][y]){
                    draw_centered( dst, {xOff,yOff}, { m_col_widths[x],m_row_heights[y] } , *elements[x][y], map );
                }
                yOff+=m_row_heights[y]+pad; 
            }
            xOff+=m_col_widths[x]+pad;
        }
    }
};

class MailboxDrawable
    : public Drawable
{
public:
    MailboxDrawable(const MailboxNode *node)
        : mailbox(node)
    {}

    const MailboxNode * const mailbox;

    const double width = 100, height = 100;

    Size size() const override
    { return {width*1.7,height*1.7}; }

    void draw(SVGWriter &dst, Point topLeft, LinkMap &map) const override
    {
        Size routerSize{width,height};
        Size clusterSize{width/2,height/2};

        dst.rectangle(topLeft, topLeft+routerSize);
        Point mid=topLeft+Size{width*0.5,height*0.5};
        dst.text(mid, mailbox->name);
    
        draw_link_in(dst, topLeft, routerSize, North, 3, 1, &mailbox->inputs[South], map);
        draw_link_out(dst, topLeft, routerSize, South, 3, 1, &mailbox->outputs[North], map);

        draw_link_in(dst, topLeft, routerSize, South, 3, 0, &mailbox->inputs[North], map);
        draw_link_out(dst, topLeft, routerSize, North, 3, 0, &mailbox->outputs[South], map);

        draw_link_in(dst, topLeft, routerSize, West, 3, 1, &mailbox->inputs[East], map);
        draw_link_out(dst, topLeft, routerSize, East, 3, 1, &mailbox->outputs[West], map);

        draw_link_in(dst, topLeft, routerSize, East, 3, 0, &mailbox->inputs[West], map);
        draw_link_out(dst, topLeft, routerSize, West, 3, 0, &mailbox->outputs[East], map);

        auto clusterTopLeft=topLeft+routerSize+Size{0.2*width,0.2*height};

        dst.rectangle(clusterTopLeft, clusterTopLeft+clusterSize);
        draw_link_in(dst, clusterTopLeft, clusterSize, North, 1, 0, &mailbox->cluster->in, map);
        draw_link_out(dst, topLeft, routerSize, West, 3, 2, &mailbox->outputs[Here], map);

        draw_link_out(dst, clusterTopLeft, clusterSize, East, 1, 0, &mailbox->cluster->out, map);
        draw_link_in(dst, topLeft, routerSize, South, 3, 2, &mailbox->inputs[Here], map);
        
    }
};

class ExpanderDrawable
    : public Drawable
{
public:
    ExpanderDrawable(const ExpanderNode *node)
        : expander(node)
        , width( is_vertical(node->input_dir) ? 100 : 20 )
        , height( is_vertical(node->input_dir) ? 20 : 100 )
    {}

    const ExpanderNode * const expander;

    const double width;
    const double height;

    Size size() const override
    { return {width,height}; }

    void draw(SVGWriter &dst, Point topLeft, LinkMap &map) const override
    {
        dst.rectangle(topLeft, topLeft+size());
        draw_link_in(dst, topLeft, size(), reverse(expander->input_dir), 1, 0, &expander->input, map);
        for(unsigned i=0; i<expander->outputs.size(); i++){
            draw_link_out(dst, topLeft, size(), expander->input_dir, expander->outputs.size(), i, expander->outputs[i].get(), map);
        }
    }
};

class ReducerDrawable
    : public Drawable
{
public:
    ReducerDrawable(const ReducerNode *node)
        : reducer(node)
        , width( is_vertical(node->output_dir) ? 100 : 20 )
        , height( is_vertical(node->output_dir) ? 20 : 100 )
    {}

    const ReducerNode * const reducer;

    const double width;
    const double height;

    Size size() const override
    { return {width,height}; }

    void draw(SVGWriter &dst, Point topLeft, LinkMap &map) const override
    {
        dst.rectangle(topLeft, topLeft+size());

        draw_link_out(dst, topLeft, size(), reverse(reducer->output_dir), 1, 0, &reducer->output, map);
        for(unsigned i=0; i<reducer->inputs.size(); i++){
            draw_link_in(dst, topLeft, size(), reducer->output_dir, reducer->inputs.size(), i, reducer->inputs[i].get(), map);
        }

    }
};


#endif
