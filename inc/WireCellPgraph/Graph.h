/** A simple DFP engine meant for single-threaded execution.

    A node is constructed with zero or more ports.  

    A port mediates between a node and an edge.

    A port is either of type input or output.

    In the context of a node, a port has a name and an index into an
    ordered list of other ports of a given type.

    An edge is a queue passing data objects in one direction from its
    tail (input) end to its head (output) end.

    Each end of an edge is exclusively "plugged" into one node through
    one port. 
    
    A valid graph consists of nodes with all ports plugged to edges.

 */

#ifndef WIRECELL_PGRAPH_GRAPH
#define WIRECELL_PGRAPH_GRAPH

#include "WireCellPgraph/Node.h"

namespace WireCell {
    namespace Pgraph {

        class Graph {
        public:
            // Connect two nodes by their given ports.
            void connect(Node* tail, Node* head,
                         size_t tpind=0, size_t hpind=0);
            
            // return a topological sort of the graph as per Kahn algorithm.
            std::vector<Node*> sort_kahn();

            // Excute the graph until nodes stop delivering
            bool execute();

        private:
            std::vector<std::pair<Node*,Node*> > m_edges;
        };
}
}
#endif
