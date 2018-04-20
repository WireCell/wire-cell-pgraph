#include "WireCellPgraph/Graph.h"
#include "WireCellUtil/Type.h"

#include <unordered_map>
#include <unordered_set>
#include <iostream> // debug

using WireCell::demangle;
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

    m_edges_forward[tail].push_back(head);
    m_edges_backward[head].push_back(tail);


    std::cerr << "Graph::connect:\n"
              << tail->ident() << "\n"
              << "\t(" << demangle(tport.signature ()) << ":" << tpind << ")\n"
              << "\t--> \n"
              << head->ident() << "\n"
              << "\t(" << demangle(hport.signature()) << ":" << hpind << ")\n";

    return true;
}

std::vector<Node*> Graph::sort_kahn() {

    std::unordered_map<Node*, int> nincoming;
    for (auto th : m_edges) {

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

        for (auto h : m_edges_forward[t]) {
            nincoming[h] -= 1;
            if (nincoming[h] == 0) {
                seeds.insert(h);
            }
        }
    }
    return ret;
}

int Graph::execute_upstream(Node* node)
{
    int count = 0;
    for (auto parent : m_edges_backward[node]) {
        bool ok = call_node(parent);
        if (ok) {
            ++count;
            continue;
        }
        count += execute_upstream(parent);
    }
    bool ok = call_node(node);
    if (ok) { ++count; }
    return count;
}

// this bool indicates exception, and is probably ignored
bool Graph::execute()
{
    auto nodes = sort_kahn();
    std::cerr << "Pgraph::Graph executing with " << nodes.size() << " nodes\n";

                
    while (true) {

        int count = 0;
        bool did_something = false;            

        for (auto nit = nodes.rbegin(); nit != nodes.rend(); ++nit, ++count) {
            Node* node = *nit;

            bool ok = call_node(node);
            if (ok) {
                //std::cerr << "Ran node " << count << ": " << node->ident() << std::endl;
                did_something = true;
                break;          // start again from bottom of graph
            }

            // int nupstream = execute_upstream(node);
            // if (nupstream > 0) {
            //     did_something = true;
            //     break;          // start again from bottom of graph
            // }

            // otherwise try upstream following topological sort
        }

        if (!did_something) {
            return true;        // it's okay to do nothing.
        }
    }
    return true;    // shouldn't reach
}

bool Graph::call_node(Node* node)
{
    if (!node) {
        std::cerr << "Graph: call: got nullptr node\n";
        return false;
    }
    bool ok = (*node)();
    // this can be very noisy but useful to uncomment to understand
    // the graph execution order.
    if (ok) {
        std::cerr << "Graph call ["<<ok<<"] called: " << node->ident() << "\n";
    }
    return ok;
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

