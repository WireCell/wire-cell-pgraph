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
#include "WireCellIface/IHydraNode.h"

#include <map>
#include <iostream>             // debug

namespace WireCell { namespace Pgraph { 

        // Node wrappers are constructed with just an INode::pointer
        // and adapt it to Pgraph::Node.  They are constructed with a
        // type-erasing factory below.

        class PortedNode : public Pgraph::Node
        {
        public:
            PortedNode(INode::pointer wcnode) {
                
                using Pgraph::Port;
                for (auto sig : wcnode->input_types()) {
                    m_ports[Port::input].push_back(
                        Pgraph::Port(this, Pgraph::Port::input, sig));
                }
                for (auto sig : wcnode->output_types()) {
                    m_ports[Port::output].push_back(
                        Pgraph::Port(this, Pgraph::Port::output, sig));
                }
            }        
        };


        class Source : public PortedNode {
            ISourceNodeBase::pointer m_wcnode;
            bool m_ok;
        public:
            Source(INode::pointer wcnode) : PortedNode(wcnode), m_ok(true) {
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
            Sink(INode::pointer wcnode) : PortedNode(wcnode) {
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
            Function(INode::pointer wcnode) : PortedNode(wcnode) {
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
            Queuedout(INode::pointer wcnode) : PortedNode(wcnode) {
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
            Join(INode::pointer wcnode) : PortedNode(wcnode) {
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

        class Hydra : public PortedNode {
            IHydraNodeBase::pointer m_wcnode;
        public:
            Hydra(INode::pointer wcnode) : PortedNode(wcnode) {
                m_wcnode = std::dynamic_pointer_cast<IHydraNodeBase>(wcnode);
            }
            virtual ~Hydra() { }
            virtual bool ready() {
                // require at least one input and that some new input
                // was consumed or output was produced since last call.

                for (auto& p : m_ports[Port::input]) {
                    if (!p.empty()) return true;
                }
                return true;
            }
            
            virtual bool operator()() {
                auto& iports = input_ports();
                size_t nin = iports.size();

                // 1) fill input any queue vector
                IHydraNodeBase::any_queue_vector inqv(nin);
                for (size_t ind=0; ind < nin; ++ind) {
                    Edge edge = iports[ind].edge();
                    if (!edge) {
                        std::cerr << "Hydra: got broken edge\n";
                        continue;
                    }
                    inqv[ind].insert(inqv[ind].begin(), edge->begin(), edge->end());
                }

                auto& oports = output_ports();
                size_t nout = oports.size();

                // 2) create output any queue vector
                IHydraNodeBase::any_queue_vector outqv(nout);

                // 3) call
                bool ok = (*m_wcnode)(inqv, outqv);
                if (!ok) { return false; } // fixme: this probably
                                           // needs to reflect into
                                           // ready().

                // 4) pop dfp input queues to match.  BIG FAT
                // WARNING: this trimming assumes calller only
                // pop_front's.  Really should hunt for which ones
                // have been removed.
                for (size_t ind=0; ind < nin; ++ind) {
                    size_t want = inqv[ind].size();
                    while (iports[ind].size() > want) {
                        iports[ind].get(); 
                    }
                }
                
                // 5) send out output any queue vectors
                for (size_t ind=0; ind < nout; ++ind) {
                    Edge edge = oports[nout].edge();
                    edge->insert(edge->end(), outqv.begin(), outqv.end());
                }
                // 6) record input queue levels

                return true;
            }
        };
    }}

#endif
