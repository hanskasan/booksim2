// Hans Kasan
// CSNL-KAIST

#ifndef _BOOKSIM_H
#define _BOOKSIM_H

#include <sst/core/clock.h>
#include <sst/core/component.h>
#include <sst/core/subcomponent.h>
#include <sst/core/event.h>
#include <sst/core/link.h>
#include <sst/core/params.h>
#include <sst/core/interfaces/simpleNetwork.h>
    
#include "src/trafficmanager.hpp"
#include "src/booksim_config.hpp"

namespace SST {
namespace BookSim {

class BookSimInterface_Base;
class BookSimInterface;
class BookSimEvent;

class booksim2 : public SST::Component
{
public:

    // REGISTER THIS COMPONENT INTO THE ELEMENT LIBRARY
    SST_ELI_REGISTER_COMPONENT(
        booksim2,                       // Component class
        "booksim2",                     // Component library (for Python/library lookup)
        "booksim2",                     // Component name (for Python/library lookup)
        SST_ELI_ELEMENT_VERSION(1,0,0), // Version of the component (not related to SST version)
        "The best cycle-accurate network simulator",   // Description
        COMPONENT_CATEGORY_NETWORK      // Category
    )

    // Document the parameters that this component accepts
    // { "parameter_name", "description", "default value or NULL if required" }
    SST_ELI_DOCUMENT_PARAMS(
        {"booksim_clock",       "BookSim clock frequency", "1GHz"},
        {"topology",            "Network topology", "none"},
        {"num_motif_nodes",     "Number of motif nodes", "0"},
        {"routing_function",    "Routing function algorithm", "none"},
        {"packet_size",         "Packet size in flits", "1"},
        {"num_vcs",             "Number of virtual channels", "1"}
    )

    // Document the ports that this component has
    // Connect the motif endpoint nodes to Ember and Firefly
    // {"Port name", "Description", { "list of event types that the port can handle"} }
    SST_ELI_DOCUMENT_PORTS(
        //{"motif_node%(num_motif_nodes)d",  "Motif nodes which connect to Ember and Firefly.", { "booksim2.BookSimEvent" } }
    )
    
    // Optional since there is nothing to document - see statistics example for more info
    SST_ELI_DOCUMENT_STATISTICS( )

    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS( 
        {"booksim_interface", "Interfaces between BookSimBridge and BookSim's traffic manager", "SST::BookSim::BookSimInterface"}
    )

    // CLASS MEMBERS
    // Constructor
    booksim2(SST::ComponentId_t id, SST::Params& params);

    // Destructor
    ~booksim2();

    // Run a single simulation step
    bool BabyStep(Cycle_t cycle);

private:
    BookSimConfig       config;
    vector<Network*>    _net;
    int                 _subnets;
    int                 _num_motif_nodes;

    Clock::Handler<booksim2>* clock_handler;

    BookSimInterface_Base* _interface;

    //void handle_new_packets(Event* ev);

    // Links
    //SST::Link* nic2booksim_link;

};

// Base class for subcomponent BookSimInterface
class BookSimInterface_Base : public SubComponent {

public:

    // parameter: node ID
    SST_ELI_REGISTER_SUBCOMPONENT_API(SST::BookSim::BookSimInterface_Base, booksim2*, int)

    BookSimInterface_Base(SST::ComponentId_t id) : SubComponent(id) {}
    virtual ~BookSimInterface_Base() {}

};

// Events to be recognized by BookSim and its interfaces (BookSimBridge and BookSimInterface)
class BookSimEvent : public Event {

public:

    // Constructor
    BookSimEvent(SST::Interfaces::SimpleNetwork::Request* req, SST::Interfaces::SimpleNetwork::nid_t trusted_src)
    {
        request = req;
    }

    BookSimEvent() {} // For serialization only

    // Deconstuctor
    ~BookSimEvent()
    {
        if (request) delete request;
    }

    inline void setInjectionTime(SimTime_t time) {injectionTime = time;}
    inline SimTime_t getInjectionTime(void) const { return injectionTime; }

    inline void computeSizeInFlits(int flit_size ) {size_in_flits = (request->size_in_bits + flit_size - 1) / flit_size; }
    inline int getSizeInFlits() { return size_in_flits; }
    inline int getSizeInBits() { return request->size_in_bits; }

    SST::Interfaces::SimpleNetwork::Request* takeRequest() {
        auto ret = request;
        request = nullptr;
        return ret;
    }

    void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        Event::serialize_order(ser);
        ser & request;
        ser & size_in_flits;
        ser & injectionTime;
    }

protected:
   

private:
    SST::Interfaces::SimpleNetwork::Request* request;
    SimTime_t injectionTime;
    int size_in_flits;

    ImplementSerializable(BookSimEvent);

};

} // namespace BookSim
} // namespace SST

#endif