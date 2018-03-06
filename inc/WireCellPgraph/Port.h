#ifndef WIRECELL_PGRAPH_PORT
#define WIRECELL_PGRAPH_PORT

#include "WireCellUtil/Exceptions.h"

#include <boost/any.hpp>

#include <string>
#include <vector>
#include <deque>
#include <memory>

namespace WireCell {
    namespace Pgraph {

        // The type of data passed in the graph.
        typedef boost::any Data;
        // A buffer of data.  It is a std::deque instead of a
        // std::queue so that it may be iterated as well as pushed
        // back into.  Like a British queue, one enters it from the
        // back and exits it from the front.
        typedef std::deque<Data> Queue;

        // Edges are just queues that can be shared.
        typedef std::shared_ptr<Queue> Edge;

        class Node;

        class Port {
        public:
            enum Type { tail=0, output=0, head=1, input=1, ntypes=2 };

            Port(Node* node, Type type, std::string signature,
                 std::string name="") :
                m_node(node), m_type(type), 
                m_name(name), m_sig(signature),
                m_edge(nullptr)
                { }
                
            bool isinput() { return m_type == Port::input; }
            bool isoutput() { return m_type == Port::output; }

            Edge edge() { return m_edge; }

            // Connect an edge, returning any previous one.
            Edge plug(Edge edge) {
                Edge ret = m_edge;
                m_edge = edge;
                return ret;
            }

            // return edge queue size or 0 if no edge has been plugged
            size_t size() {
                if (!m_edge) { return 0; }
                return m_edge->size();
            }

            // Return true if queue is empty or no edge has been plugged.
            bool empty() {
                if (!m_edge or m_edge->empty()) { return true; }
                return false;
            }

            // Get the next data.  By default this pops the data off
            // the queue.  To "peek" at the data, pas false.
            Data get(bool pop = true) {
                if (isoutput()) {
                    THROW(RuntimeError()
                          << errmsg{"can not get from output port"});
                }
                if (!m_edge) {
                    THROW(RuntimeError() << errmsg{"port has no edge"});
                }
                if (m_edge->empty()) {
                    THROW(RuntimeError() << errmsg{"edge is empty"});
                }
                Data ret = m_edge->front();
                if (pop) {
                    m_edge->pop_front();
                }
                return ret;
            }

            // Put the data onto the queue.
            void put(Data& data) {
                if (isinput()) {
                    THROW(RuntimeError() << errmsg{"can not put to input port"});
                }
                if (!m_edge) {
                    THROW(RuntimeError() << errmsg{"port has no edge"});
                }
                m_edge->push_back(data);
            }

            const std::string& name() { return m_name; }
            const std::string& signature() { return m_sig; }

        private:
            Node* m_node;       // node to which port belongs
            Type m_type;
            std::string m_name, m_sig;
            Edge m_edge;
        };

        typedef std::vector<Port> PortList;
    }
}

#endif
