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

#include "WireCellUtil/Type.h"

#include <map>
#include <iostream>             // debug
#include <sstream>

namespace WireCell { namespace Pgraph { 

        // Node wrappers are constructed with just an INode::pointer
        // and adapt it to Pgraph::Node.  They operate at the
        // boost::any level and the I*BaseNode INode level.  They are
        // not meant to be constructed directly but through the
        // type-erasing Pgraph::Factory.  They intercept data looking
        // for INode::DFPMeta objects to control flow.

        // Base class taking care of constructing ports and providing
        // ident().
        class PortedNode : public Pgraph::Node
        {
        public:
            PortedNode(INode::pointer wcnode) : m_wcnode(wcnode) {
                
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

            virtual std::string ident() {
                std::stringstream ss;
                ss << "<Node "
                   << " type:" << WireCell::type(*(m_wcnode.get()))
                   << " cat:" << m_wcnode->category()
                   << " sig:" << demangle(m_wcnode->signature());
                ss << " inputs:[";
                for (auto t : m_wcnode->input_types()) {
                    ss << " " << demangle(t);
                }
                ss << " ]";
                ss << " outputs:[";
                for (auto t : m_wcnode->output_types()) {
                    ss << " " << demangle(t);
                }
                ss << " ]";
                return ss.str();
            }

        private:
            INode::pointer m_wcnode;
        };


        class Source : public PortedNode {
            ISourceNodeBase::pointer m_wcnode;
            bool m_ok;
        public:
            Source(INode::pointer wcnode) : PortedNode(wcnode), m_ok(true) {
                m_wcnode = std::dynamic_pointer_cast<ISourceNodeBase>(wcnode);
            }
            virtual ~Source() {}

            virtual bool operator()() {
                boost::any obj;
                m_ok = (*m_wcnode)(obj);
                if (!m_ok) {
                    return false;
                }
                oport().put(obj);
                return true;
            }
        };

        class Sink : public PortedNode {
            ISinkNodeBase::pointer m_wcnode;
        public:
            Sink(INode::pointer wcnode) : PortedNode(wcnode) {
                m_wcnode = std::dynamic_pointer_cast<ISinkNodeBase>(wcnode);
            }
            virtual ~Sink() {}
            virtual bool operator()() {
                Port& ip = iport();
                if (ip.empty()) { return false; }
                auto obj = ip.get();
                bool ok = (*m_wcnode)(obj);
                //std::cerr << "Sink returns: " << ok << std::endl;
                return ok;
            }
        };

        class Function : public PortedNode {
            IFunctionNodeBase::pointer m_wcnode;
        public:
            Function(INode::pointer wcnode) : PortedNode(wcnode) {
                m_wcnode = std::dynamic_pointer_cast<IFunctionNodeBase>(wcnode);
            }
            virtual ~Function() {}
            virtual bool operator()() {
                Port& ip = iport();
                if (ip.empty()) { return false; }
                boost::any out;
                auto in = ip.get();
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
                Port& ip = iport();
                if (ip.empty()) { return false; }
                IQueuedoutNodeBase::queuedany outv;
                auto in = ip.get();
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
                for (size_t ind=0; ind<nin; ++ind) {
                    if (iports[ind].empty()) { return false;
                    }                        
                }
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

        // N-to-1 of the same type with synchronization on input.
        // class Fanin : public PortedNode {
        //     IFaninNodeBase::pointer m_wcnode;
        //     std::vector<bool> m_eos;
        // public:
        //     Fanin(INode::pointer wcnode) : PortedNode(wcnode) {
        //         m_wcnode = std::dynamic_pointer_cast<IFaninNodeBase>(wcnode);
        //     }
        //     virtual ~Fanin() {}

        // };

        class Hydra : public PortedNode {
            IHydraNodeBase::pointer m_wcnode;
        public:
            Hydra(INode::pointer wcnode) : PortedNode(wcnode) {
                m_wcnode = std::dynamic_pointer_cast<IHydraNodeBase>(wcnode);
            }
            virtual ~Hydra() { }
            
            virtual bool operator()() {
                auto& iports = input_ports();
                size_t nin = iports.size();

                // 0) Hydra needs all input ports full to be ready.
                // For EOS, the concrete INode better retain the
                // terminating nullptr in its input stream.
                for (size_t ind=0; ind < nin; ++ind) {
                    if (iports[ind].empty()) {
                        return false;
                    }
                }

                // 1) fill input any queue vector
                IHydraNodeBase::any_queue_vector inqv(nin);
                for (size_t ind=0; ind < nin; ++ind) {
                    Edge edge = iports[ind].edge();
                    if (!edge) {
                        std::cerr << "Hydra: got broken edge\n";
                        continue;
                    }
                    if (edge->empty()) {
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
                    Edge edge = oports[ind].edge();
                    edge->insert(edge->end(), outqv[ind].begin(), outqv[ind].end());
                }

                return true;
            }
        };


    }}

#endif
