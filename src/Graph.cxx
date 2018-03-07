#include "WireCellPgraph/Graph.h"

#include <unordered_map>
#include <unordered_set>
#include <iostream> // debug

using namespace WireCell::Pgraph;

void Graph::add_node(Node* node)
{
    m_nodes.insert(node);
}

bool Graph::connect(Node* tail, Node* head, size_t tpind, size_t hpind)
{
    Port& tport = tail->output_ports()[tpind];
    Port& hport = head->input_ports()[hpind];
    if (tport.signature() != hport.signature()) {
        std::cerr << "Pgraph::Graph: port signature mismatch: \""
                  << tport.signature ()
                  << "\" != \""
                  << hport.signature()
                  << "\"\n";
        THROW(ValueError() << errmsg{"port signature mismatch"});

        return false;
    }

    m_edges.push_back(std::make_pair(tail,head));
    Edge edge = std::make_shared<Queue>();

    tport.plug(edge);
    hport.plug(edge);                

    add_node(tail);
    add_node(head);

    std::cerr << "Graph::connect: "
              << tport.signature () << ":" << tpind << " --> "
              << hport.signature() << ":" << hpind << "\n";

    return true;
}

std::vector<Node*> Graph::sort_kahn() {
    std::unordered_map< Node*, std::vector<Node*> > edges;
    std::unordered_map<Node*, int> nincoming;
    for (auto th : m_edges) {
        edges[th.first].push_back(th.second);
        nincoming[th.first] += 0; // make sure all nodes represented
        nincoming[th.second] += 1;
    }
                
    std::vector<Node*> ret;
    std::unordered_set<Node*> seeds;

    for (auto it : nincoming) {
        if (it.second == 0) {
            seeds.insert(it.first);
        }
    }

    while (!seeds.empty()) {
        Node* t = *seeds.begin();
        seeds.erase(t);
        ret.push_back(t);

        for (auto h : edges[t]) {
            nincoming[h] -= 1;
            if (nincoming[h] == 0) {
                seeds.insert(h);
            }
        }
    }
    return ret;
}

bool Graph::execute()
{
    auto nodes = sort_kahn();
    std::cerr << "Pgraph::Graph executing with " << nodes.size() << " nodes\n";

                
    while (true) {
        bool did_something = false;
        // go through nodes starting outputs
        int count = 0;
        for (auto nit = nodes.rbegin(); nit != nodes.rend(); ++nit, ++count) {
            Node* node = *nit;
            if (!node->ready()) {
                //std::cerr << "Pgraph::Graph node not ready: " << count << std::endl;
                continue; // go futher upstream
            }
                        
            bool ok = (*node)();
            if (!ok) {
                std::cerr << "PipeGraph node returned false\nnode:\n" << node->ident() << "\n";
                return false;
            }
            std::cerr << "Ran node " << count << ": " << node->ident() << std::endl;
            did_something = true;
            break;
        }
        if (!did_something) {
            return true;
        }
    }
    return true;    // shouldn't reach
}


bool Graph::connected()
{
    for (auto n : m_nodes) {
        if (!n->connected()) {
            return false;
        }
    }
    return true;
}

