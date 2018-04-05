#include "WireCellPgraph/Pgrapher.h"
#include "WireCellPgraph/Factory.h"
#include "WireCellIface/INode.h"
#include "WireCellUtil/NamedFactory.h"

#include <iostream>

WIRECELL_FACTORY(Pgrapher, WireCell::Pgraph::Pgrapher,
                 WireCell::IApplication, WireCell::IConfigurable);



using WireCell::get;
using namespace WireCell::Pgraph;

WireCell::Configuration Pgrapher::default_configuration() const
{
    Configuration cfg;

    cfg["edges"] = Json::arrayValue;
    return cfg;
}

static
std::pair<WireCell::INode::pointer, int> get_node(WireCell::Configuration jone)
{
    using namespace WireCell;
    std::string node = jone["node"].asString();

    // false as we should be the ones creating
    auto nptr = Factory::lookup_tn<INode>(node, false);
    if (!nptr) {
        std::cerr << "Pgrapher: failed to get node " << node << "\n";
        THROW(ValueError() << errmsg{"failed to get node"});
    }

    int port = get(jone,"port",0);
    return std::make_pair(nptr, port);
}

void Pgrapher::configure(const WireCell::Configuration& cfg)

{
    Pgraph::Factory fac;
    std::cerr << "Pgrapher: connecting: " << cfg["edges"].size() << " edges\n";
    for (auto jedge : cfg["edges"]) {
        auto tail = get_node(jedge["tail"]);
        auto head = get_node(jedge["head"]);

        //std::cerr << "Pgrapher: connecting:\n" << jedge << "\n";

        bool ok = m_graph.connect(fac(tail.first),  fac(head.first),
                                  tail.second, head.second);
        if (!ok) {
            std::cerr << "Pgrapher: failed to connect edge:\n" << jedge << std::endl;
            THROW(ValueError() << errmsg{"failed to connect edge"});
        }
    }
    if (!m_graph.connected()) {
        std::cerr << "Pgrapher: graph not fully connected\n";
        THROW(ValueError() << errmsg{"graph not fully connected"});
    }
}



void Pgrapher::execute()
{
    m_graph.execute();
}



Pgrapher::Pgrapher()
{
}
Pgrapher::~Pgrapher()
{
}
