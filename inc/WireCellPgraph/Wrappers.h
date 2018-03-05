#ifndef WIRECELL_PGRAPH_WRAPPERS
#define WIRECELL_PGRAPH_WRAPPERS

#include "WireCellPgraph/Graph.h"

// fixme: this is a rather monolithic file that should be broken out
// into its own package.  It needs to depend on util and iface but NOT
// gen.
#include "WireCellIface/ISourceNode.h"
#include "WireCellIface/ISinkNode.h"
#include "WireCellIface/IFunctionNode.h"
#include "WireCellIface/IQueuedoutNode.h"
#include "WireCellIface/IJoinNode.h"

#include <map>
#include <iostream>             // debug

namespace WireCell { namespace Pgraph { 

        // Node wrappers are constructed with just an INode::pointer
        // and adapt it to Pgraph::Node.  They are constructed with a
        // type-erasing factory below.

        class PortedNode : public Pgraph::Node
        {
        public:
            PortedNode(int nin=0, int nout=0) { 
                using Pgraph::Port;
                for (int ind=0; ind<nin; ++ind) {
                    m_ports[Port::input].push_back(
                        Pgraph::Port(this, Pgraph::Port::input));
                }
                for (int ind=0; ind<nout; ++ind) {
                    m_ports[Port::output].push_back(
                        Pgraph::Port(this, Pgraph::Port::output));
                }
            }        
        };


        class Source : public PortedNode {
            ISourceNodeBase::pointer m_wcnode;
            bool m_ok;
        public:
            Source(INode::pointer wcnode) : PortedNode(0,1), m_ok(true) {
                m_wcnode = std::dynamic_pointer_cast<ISourceNodeBase>(wcnode);
            }
            virtual ~Source() {}
            virtual bool ready() { return m_ok; }
            virtual bool operator()() {
                boost::any obj;
                m_ok = (*m_wcnode)(obj);
                if (!m_ok) {
                    // fixme: need to clean up the node protocol.  When using
                    // queues, the protocol is simply: don't send anything out
                    // when you have nothing to send out.  But in the
                    // functional interface with output arguments there is not
                    // general type safe way to tell if the output argument
                    // was set to NULL.  So, we must wait to go one past EOS
                    // to see the railure returned.  What I *should* do is use
                    // excpetions to indicate failures or read-past-EOS.
                    return true;
                }
                oport().put(obj);
                return true;
            }
        };

// send objects to a terminal node.
        class Sink : public PortedNode {
            ISinkNodeBase::pointer m_wcnode;
        public:
            Sink(INode::pointer wcnode) : PortedNode(1,0) {
                m_wcnode = std::dynamic_pointer_cast<ISinkNodeBase>(wcnode);
            }
            virtual ~Sink() {}
            virtual bool operator()() {
                auto obj = iport().get();
                bool ok = (*m_wcnode)(obj);
                std::cerr << "Sink returns: " << ok << std::endl;
                return ok;
            }
        };

// One object out for every object in.
        class Function : public PortedNode {
            IFunctionNodeBase::pointer m_wcnode;
        public:
            Function(INode::pointer wcnode) : PortedNode(1,1) {
                m_wcnode = std::dynamic_pointer_cast<IFunctionNodeBase>(wcnode);
            }
            virtual ~Function() {}
            virtual bool operator()() {
                boost::any out;
                auto in = iport().get();
                bool ok = (*m_wcnode)(in, out);
                if (!ok) return false;
                oport().put(out);
                return true;
            }
        };

        class Queuedout : public PortedNode {
            IQueuedoutNodeBase::pointer m_wcnode;
        public:
            Queuedout(INode::pointer wcnode) : PortedNode(1,1) {
                m_wcnode = std::dynamic_pointer_cast<IQueuedoutNodeBase>(wcnode);
            }
            virtual ~Queuedout() {}
            virtual bool operator()() {
                IQueuedoutNodeBase::queuedany outv;
                auto in = iport().get();
                bool ok = (*m_wcnode)(in, outv);
                if (!ok) return false;
                for (auto out : outv) {
                    oport().put(out);
                }
                return true;
            }
        };

        class Join : public PortedNode {
            IJoinNodeBase::pointer m_wcnode;
        public:
            Join(INode::pointer wcnode) : PortedNode(wcnode->input_types().size(),
                                                     wcnode->output_types().size()) {
                m_wcnode = std::dynamic_pointer_cast<IJoinNodeBase>(wcnode);
            }
            virtual ~Join() {}
            virtual bool operator()() {
                auto& iports = input_ports();
                size_t nin = iports.size();
                IJoinNodeBase::any_vector inv(nin);
                for (size_t ind=0; ind<nin; ++ind) {
                    inv[ind] = iports[ind].get();
                }
                boost::any out;
                bool ok = (*m_wcnode)(inv, out);
                if (!ok) return false;
                oport().put(out);
                return true;                                      
            }
        };

        
    }}

#endif
