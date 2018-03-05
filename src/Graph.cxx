#include "WireCellPgraph/Graph.h"

#include <unordered_map>
#include <unordered_set>
#include <iostream> // debug

using namespace WireCell::Pgraph;

void Graph::connect(Node* tail, Node* head, size_t tpind, size_t hpind)
{
    m_edges.push_back(std::make_pair(tail,head));
    Edge edge = std::make_shared<Queue>();
    tail->port(Port::output, tpind).plug(edge);
    head->port(Port::input, hpind).plug(edge);                
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
                
    while (true) {
        bool did_something = false;
        // go through nodes starting outputs
        for (auto nit = nodes.rbegin(); nit != nodes.rend(); ++nit) {
            Node* node = *nit;
            if (!node->ready()) {
                continue; // go futher upstream
            }
                        
            bool ok = (*node)();
            if (!ok) {
                std::cerr << "PipeGraph failed\n";
                return false;
            }
            did_something = true;
            break;
        }
        if (!did_something) {
            return true;
        }
    }
    return true;    // shouldn't reach
}

