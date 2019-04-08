#include "WireCellPgraph/Graph.h"
#include "WireCellUtil/Type.h"

#include <unordered_map>
#include <unordered_set>


using WireCell::demangle;
using namespace WireCell::Pgraph;

Graph::Graph()
    : l(Log::logger("pgraph"))
{
}

void Graph::add_node(Node* node)
{
    m_nodes.insert(node);
}

bool Graph::connect(Node* tail, Node* head, size_t tpind, size_t hpind)
{
    Port& tport = tail->output_ports()[tpind];
    Port& hport = head->input_ports()[hpind];
    if (tport.signature() != hport.signature()) {
        l->critical ("port signature mismatch: \"{}\" != \"{}\"",
                  tport.signature (), hport.signature());
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

    SPDLOG_LOGGER_TRACE(l, "connect {}:({}:{}) -> {}({}:{})",
             tail->ident(),
             demangle(tport.signature ()),
             tpind,
             head->ident(),
             demangle(hport.signature()),
             hpind);

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
    l->debug("executing with {} nodes", nodes.size());

    while (true) {

        int count = 0;
        bool did_something = false;            

        for (auto nit = nodes.rbegin(); nit != nodes.rend(); ++nit, ++count) {
            Node* node = *nit;

            bool ok = call_node(node);
            if (ok) {
                SPDLOG_LOGGER_TRACE(l, "ran node {}: {}", count, node->ident());
                did_something = true;
                break;          // start again from bottom of graph
            }

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
        l->error("graph call: got nullptr node");
        return false;
    }
    bool ok = (*node)();
    // this can be very noisy but useful to uncomment to understand
    // the graph execution order.
    if (ok) {
        SPDLOG_LOGGER_TRACE(l, "graph call [{}] called: {}", ok, node->ident());
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

