#include "WireCellPgraph/Factory.h"
#include "WireCellPgraph/Wrappers.h"

using namespace WireCell::Pgraph;

Factory::Factory() {
    // categories defined in INode.h.
    // if it's not here, you ain't usin' it.
    bind_maker<Source>(INode::sourceNode);
    bind_maker<Sink>(INode::sinkNode);
    bind_maker<Function>(INode::functionNode);
    bind_maker<Queuedout>(INode::queuedoutNode);
    bind_maker<Join>(INode::joinNode);
    bind_maker<Split>(INode::splitNode);
    bind_maker<Hydra>(INode::hydraNode);
    // ...
}

Node* Factory::operator()(WireCell::INode::pointer wcnode) {
    if (!wcnode) {
        std::cerr << "Pgraph::Factory given nullptr wcnode\n";
        THROW(ValueError() << errmsg{"nullptr wcnode"});
    }

    auto nit = m_nodes.find(wcnode);
    if (nit != m_nodes.end()) {
        return nit->second;
    }
    auto mit = m_factory.find(wcnode->category());
    if (mit == m_factory.end()) {
        std::cerr << "Pgraph::Factory failed to find maker for category: "
                  << wcnode->category() << "\n";
        THROW(ValueError() << errmsg{"failed to find maker"});
    }
    auto maker = mit->second;
    
    Node* node = (*maker)(wcnode);
    m_nodes[wcnode] = node;
    return node;
}
