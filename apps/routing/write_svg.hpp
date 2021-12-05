#ifndef write_svg_hpp
#define write_svg_hpp

#include <vector>
#include <string>
#include <variant>
#include <optional>
#include <stack>
#include <cassert>

#include <libxml++-2.6/libxml++/document.h>

class SVGWriter
{
public:
    struct Size
    {
        double w, h;

        Size operator+(const Size &s) const
        { return {w+s.w,h+s.h}; }

        Size operator*(double s) const
        { return {w*s,h*s}; }

    };

    struct Point
    {
        double x, y;

        Point operator+(const Size &s) const
        { return {x+s.w,y+s.h}; }
    };



    struct Colour
    {
        double r, g, b;

        bool operator==(const Colour &x) const
        { return r==x.r && g==x.g && b==x.b; }

        bool operator!=(const Colour &x) const
        { return !(*this==x); }
    };
    using ColourSpec = std::variant<std::monostate,Colour,std::string>;


private:
    template<class T>
    static void merge_part(std::optional<T> &dst, const std::optional<T> &src)
    {
        if(src.has_value()){
            dst=src;
        }
    }

    template<class ...T>
    static void merge_part(std::variant<std::monostate, T...> &dst, const std::variant<std::monostate,T...> &src)
    {
        if(!std::get_if<std::monostate>(&src)){
            dst=src;
        }
    }

public:
    struct Attributes
    {
        ColourSpec stroke_colour;
        std::optional<double> width;
        ColourSpec fill_colour;
        std::optional<std::string> id;
        std::optional<std::string> _class;
        std::optional<double> scale;
        std::optional<std::string> title;

        Attributes &merge(const Attributes &src)
        {
            merge_part(stroke_colour, src.stroke_colour);
            merge_part(width, src.width);
            merge_part(fill_colour, src.fill_colour);
            merge_part(id, src.id);
            merge_part(_class, src._class);
            merge_part(scale, src.scale);
            merge_part(title, src.title);
            return *this;
        }

        Attributes operator|(const Attributes &o) const
        {
            Attributes res(*this);
            res.merge(o);
            return res;
        }
    };
private:

    std::stack<Attributes> m_attributes;
    std::stack<xmlpp::Element *> m_groups;
    std::unordered_set<std::string> m_ids;

    std::string to_string(const ColourSpec &spec) const
    {
        if(auto s=std::get_if<std::string>(&spec)){
            return *s;
        }else if(auto c=std::get_if<Colour>(&spec)){
            int r=std::min(255,std::max(0,(int)(c->r*256)));
            int g=std::min(255,std::max(0,(int)(c->g*256)));
            int b=std::min(255,std::max(0,(int)(c->b*256)));
            char buffer[32]={0}; // Big enough for any int rgb string
            snprintf(buffer, sizeof(buffer), "rgb(%u,%u,%u)", r, g, b);
            return buffer;
        }else{
            throw std::runtime_error("Colour has no string");
        }
    }

    void emit_attributes(xmlpp::Element *elt, const Attributes &ambient, const Attributes &got)
    {
        if(got.stroke_colour != ColourSpec() && ambient.stroke_colour != got.stroke_colour){
            elt->set_attribute("stroke", to_string(got.stroke_colour));
        }
        if(got.width.has_value() && ambient.width!=got.width){
            elt->set_attribute("stroke-width", std::to_string(got.width.value()));
        }
        if(got.fill_colour != ColourSpec() && ambient.fill_colour != got.fill_colour){
            elt->set_attribute("fill", to_string(got.fill_colour));
        }
        if(got.id.has_value()){
            std::string id=got.id.value();
            if(!id.empty()){
                auto it=m_ids.insert(id);
                if(!it.second){
                    throw std::runtime_error("Duplicate id");
                }
                elt->set_attribute("id", id);
            }
        }
        if(got._class.has_value()){
            if(!got._class.value().empty()){
                elt->set_attribute("class", got._class.value());
            }
        }
        if(got.scale.has_value()){
            elt->set_attribute("transform", "scale("+std::to_string(got.scale.value())+","+std::to_string(got.scale.value())+")");
        }

        if(got.title.has_value()){
            auto l=elt->add_child("title");
            l->set_child_text(got.title.value().c_str());
        }
    }

    xmlpp::Document m_doc;

    xmlpp::Element *current_group()
    {
        assert(!m_groups.empty());
        return m_groups.top();
    }
public:
    SVGWriter(double width, double height)
    {
        m_attributes.push({});
        m_attributes.top().stroke_colour=std::string("black");
        m_attributes.top().width=1.0;
        m_attributes.top().fill_colour=std::string("transparent");

        auto *r=m_doc.create_root_node("svg", "http://www.w3.org/2000/svg");
        r->set_attribute("width", std::to_string(width));
        r->set_attribute("height", std::to_string(height));
        emit_attributes(r, {}, m_attributes.top());

        auto *m=r->add_child("marker");
        m->set_attribute("id", "arrow");
        m->set_attribute("viewBox", "0 0 5 5");
        m->set_attribute("refX", "2.5");
        m->set_attribute("refY", "2.5");
        m->set_attribute("markerWidth", "3");
        m->set_attribute("markerHeight", "3");
        m->set_attribute("orient","auto-start-reverse");
        auto *p=m->add_child("path");
        p->set_attribute("d", "M 0 0 L 5 2.5 L 0 5 z");

        m_groups.push(r);
    }

    void save(const std::string &dst)
    {
        m_doc.write_to_file_formatted(dst);
    }

    
    void begin_group(const Attributes &overrides={})
    {
        auto c=current_group();
        auto child=c->add_child("g");
        emit_attributes(child, m_attributes.top(), overrides);
        m_attributes.push(m_attributes.top() | overrides);
        m_groups.push(child);
    }

    void end_group()
    {
        assert(m_groups.size()>0);
        m_groups.pop();
    }

    void text( const Point &p, const std::string &text, const Attributes &overrides={})
    {
        auto e=current_group()->add_child("text");
        e->set_attribute("x", std::to_string(p.x));
        e->set_attribute("y", std::to_string(p.y));
        e->set_attribute("text-anchor", "middle");
        e->set_attribute("vertical-align", "middle");
        emit_attributes(e, m_attributes.top(), overrides);
        e->add_child_text(text);
    }

    void line( const Point &from, const Point &to, const Attributes &overrides={})
    {
        auto e=current_group()->add_child("line");
        e->set_attribute("x1", std::to_string(from.x));
        e->set_attribute("y1", std::to_string(from.y));
        e->set_attribute("x2", std::to_string(to.x));
        e->set_attribute("y2", std::to_string(to.y));
        emit_attributes(e, m_attributes.top(), overrides);
    }

    void arrow( const Point &from, const Point &to, const Attributes &overrides={})
    {
        auto e=current_group()->add_child("line");
        e->set_attribute("x1", std::to_string(from.x));
        e->set_attribute("y1", std::to_string(from.y));
        e->set_attribute("x2", std::to_string(to.x));
        e->set_attribute("y2", std::to_string(to.y));
        e->set_attribute("marker-end","url(#arrow)");
        emit_attributes(e, m_attributes.top(), overrides);
    }

    void polyline( const std::vector<Point> &p, const Attributes &overrides={})
    {
        auto e=current_group()->add_child("polyline");
        std::stringstream acc;
        for(unsigned i=0; i<p.size(); i++){
            if(i!=0){
                acc<<",";
            }
            acc<<p[i].x<<" "<<p[i].y;
        }
        e->set_attribute("points", acc.str());
        emit_attributes(e, m_attributes.top(), overrides);
    }


    void rectangle( const Point &p1, const Point &p2, const Attributes &overrides={})
    {
        double x0=std::min(p1.x,p2.x);
        double y0=std::min(p1.y,p2.y);
        double x1=std::max(p1.x,p2.x);
        double y1=std::max(p1.y,p2.y);

        auto e=current_group()->add_child("rect");
        e->set_attribute("x", std::to_string(x0));
        e->set_attribute("y", std::to_string(y0));
        e->set_attribute("width", std::to_string(x1-x0));
        e->set_attribute("height", std::to_string(y1-y0));
        emit_attributes(e, m_attributes.top(), overrides);
    }

    void circle( const Point &p, double r, const Attributes &overrides={})
    {
        auto e=current_group()->add_child("circle");
        e->set_attribute("cx", std::to_string(p.x));
        e->set_attribute("cy", std::to_string(p.y));
        e->set_attribute("r", std::to_string(r));
        emit_attributes(e, m_attributes.top(), overrides);
    }

    static Attributes make_scale(double s)
    {
        Attributes res;
        res.scale=s;
        return res;
    }
};

#endif
