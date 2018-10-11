/* a quick and dirty reader for loading in EdgeList graphs from the GAP benchmark suite. */
#ifndef __EDGELIST_READER_H
#define __EDGELIST_READER_H
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <sys/time.h>

//! struct for edges
typedef struct  _edge_t {
    unsigned src;
    unsigned dst;
} edge_t;

// the graph data structure that gets populated by the edge list file
class EdgeListGraph {
    public:
        //! constructor 
	EdgeListGraph() {}
	//! destructor
	~EdgeListGraph() {}

	//! iterators
	typedef std::vector<unsigned>::iterator viterator; /**< the vertex iterator */
	typedef std::vector<edge_t>::iterator eiterator; /**< the edge iterator */
	viterator vbegin() { return _vertices.begin(); }
	eiterator ebegin() { return _edges.begin(); }
	viterator vend() { return _vertices.end(); }
	eiterator eend() { return _edges.end(); }

	//! check if vertex exists
	bool vExists(unsigned vin){
            for(viterator v = vbegin(), vf = vend(); v!=vf; ++v) {
                if(vin == *v)
		       return true;	
	    }  
	    return false;
	}

	//! add vertex
	void addV(unsigned v) {
           //if(!vExists(v))
		  _vertices.push_back(v); 
        }

	//! check edge
	bool eExists(edge_t ein){
           for(eiterator e=ebegin(), ef=eend(); e!=ef; ++e) {
	      edge_t curr = *e;
              if((ein.src == curr.src) && (ein.dst == curr.dst)) {
                return true;  
	      } 
	   }
	   return false;
	}


        //! add edge 
        void addE(edge_t e){
           //if(!eExists(e)){
              _edges.push_back(e);
	   //}
	}	

        //! returns the number of vertices
	unsigned numV() { return _vertices.size(); }
	//! returns the number of edges
	unsigned numE() { return _edges.size(); }

        //! prints the edge list
	void print(){
          for(eiterator e=ebegin(), ee=eend(); e!=ee; ++e){
              edge_t c = *e;
	      std::cout << c.src << " " << c.dst <<"\n";
	  }
	}


        //! fanOut for vertex - for a given vertex find the fanout
	uint32_t fanOut(unsigned v) {
          uint32_t cnt=0;
          for(eiterator e=ebegin(), ee=eend(); e!=ee; ++e){
              edge_t c = *e;
	      if(c.src == v)
		      cnt++;
	  }
	  return cnt;
	}

    private:
        std::vector<unsigned> _vertices; // list of the vertices in the graph
        std::vector<edge_t> _edges; // list of the edges in the graph

};

// reader used to load in the graph from a file
class EdgeListReader {

     public:
         //! the constructor
         EdgeListReader(std::string filename) {
             _filename = filename;
	     _elfile.open(_filename.c_str());
	     _elgraph = new EdgeListGraph(); 
	     parse(); // parse the file
	     //_elgraph->print();
         }

	 //! deconstructor
	 ~EdgeListReader() {
             delete _elgraph;
         }

	 //! returns the pointer to the EdgeListGraph
	 EdgeListGraph * getELGraph() { return _elgraph; }

         //! parse : parses the input edge list graph file and populates _elgraph;
	 void parse() {
              // add code here
	      std::string line;
	      unsigned last_v_id = 0xFFFFFFF;
	      while(std::getline(_elfile, line)) {
                 // we have a line, we need to split it by space
		 std::stringstream ss(line);
		  
		 // get v1
		 std::string v1;
		 std::getline(ss,v1,' ');	 

                 // get v2
		 std::string v2;
		 std::getline(ss,v2,' ');

		 // add edge and vertices
		 edge_t t;
		 //t.src = std::stoi(v1);
		 //t.dst = std::stoi(v2);
		 // we are using c++98
		 sscanf(v1.c_str(), "%d", &t.src);
		 sscanf(v2.c_str(), "%d", &t.dst);
		 if(t.src != last_v_id) {
		     last_v_id = t.src;
		     _elgraph->addV(t.src);
		 }
		 //_elgraph->addV(t.dst);
		 _elgraph->addE(t);
	      } 
         }

     private:
         std::string _filename; /**< the filename containing the edge list */
	 std::ifstream _elfile; /**< file object for the edge list file */
         EdgeListGraph *_elgraph; /**< the graph data structure */
};


#endif /* __EDGELIST_READER_H */
