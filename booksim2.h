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

#include "src/globals.hpp"

#include <map>

namespace SST {
namespace BookSim {

class BookSimInterface_Base;
class BookSimInterface;
class BookSimBaseEvent;
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
        {"booksim_interface", "Interfaces between BookSimBridge and BookSim's traffic manager", "SST::BookSim::BookSimInterface_Base"}
    )

    // CLASS MEMBERS
    // Constructor
    booksim2(SST::ComponentId_t id, SST::Params& params);

    // Destructor
    ~booksim2();

    // Inject motif to BookSim
    void Inject(BookSimEvent* event);

    // To notify BookSim that there is an incoming event, thus activate the clock if it was paused
    // Refer to hr_router::notifyEvent() for more details
    bool IsRequestAlarm();
    void WakeBookSim();

    void init(unsigned int phase);

    // Run a single BookSim step
    bool BabyStep(Cycle_t cycle);

    // Print nodes last retirement time
    void PrintInjectionTime();
    void PrintEjectionTime();
    void PrintRetireTime();

private:

    struct booksim_event_bundle {
        BookSimEvent* event;
        bool ejected;
        int subnet;
        int vc;

        booksim_event_bundle() :
            event(nullptr),
            ejected(false),
            subnet(-1),
            vc(-1)
            {}
    };

    BookSimConfig       config;
    vector<Network*>    _net;
    int                 _num_motif_nodes;
    int                 _mapping_id;
    int                 _force_flitsize;
    bool                _is_request_alarm;

    TimeConverter* _booksim_tc;
    Clock::Handler<booksim2>* _clock_handler;

    BookSimInterface_Base* _interface;

    // vector<map<int, booksim_event_bundle> > _injected_events;
    vector<vector<deque<pair<int, booksim_event_bundle> > > > _injected_events;

    //void handle_new_packets(Event* ev);

    // Links
    //SST::Link* nic2booksim_link;

    // Record last retirement
    int _last_fg_ejection_time;
    int _last_bg_ejection_time;
    
    vector<int> _last_injection_time_vect; // Injected from BookSim, not yet ordered
    vector<int> _last_ejection_time_vect; // Ejected from BookSim, not yet ordered
    vector<int> _last_retire_time;

    // Record the 'true' packet size in flits
    // This is important since BookSim can force the packet size to a certain value, not following the packet size calculated by SST
    // vector<queue<int> > _used_credits_vect;
};

// Base class for subcomponent BookSimInterface
class BookSimInterface_Base : public SubComponent {

public:

    // parameter: node ID
    SST_ELI_REGISTER_SUBCOMPONENT_API(SST::BookSim::BookSimInterface_Base, booksim2*, int)

    BookSimInterface_Base(SST::ComponentId_t id) : SubComponent(id) {}
    virtual ~BookSimInterface_Base() {}

    virtual void send(int dest_node, BookSimBaseEvent* event) {}
    virtual int  getCredit(int dest_node) {return 0;}
    virtual bool spaceToSend(int dest_node, int flits) {return true;}
};

// Events to be recognized by BookSim and its interfaces (BookSimBridge and BookSimInterface)

class BookSimBaseEvent : public Event {

public:
    enum BookSimEventType {CREDIT, PACKET, INITIALIZATION};

    inline BookSimEventType getType() const { return type; }

    void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        Event::serialize_order(ser);
        ser & type;
    }

protected:
    BookSimBaseEvent(BookSimEventType type):
        Event(),
        type(type)
    {}

    BookSimBaseEvent()  {} // For serialization only

private:
    BookSimEventType type;

    ImplementSerializable(BookSimBaseEvent);
      
};
class BookSimEvent : public BookSimBaseEvent {

public:

    // Constructor
    BookSimEvent(SST::Interfaces::SimpleNetwork::Request* req, SST::Interfaces::SimpleNetwork::nid_t trusted_src) :
    BookSimBaseEvent(BookSimBaseEvent::PACKET),
    request(req)
    {}

    BookSimEvent() {} // For serialization only

    // Deconstuctor
    ~BookSimEvent()
    {
        if (request) delete request;
    }

    inline void setInjectionTime(SimTime_t time) {injectionTime = time;}
    inline SimTime_t getInjectionTime(void) const { return injectionTime; }

    inline void computeSizeInFlits(int flit_size ) {size_in_flits = (request->size_in_bits + flit_size - 1) / flit_size; }
    inline int getSrc() { return request->src; }
    inline int getDest() { return request->dest; }
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

class BookSimInitEvent : public BookSimBaseEvent {

public:

    // Variables
    int int_value;
    UnitAlgebra ua_value;

    // Constructor
    BookSimInitEvent() :
    BookSimBaseEvent(BookSimBaseEvent::INITIALIZATION)
    {}

    //BookSimInitEvent() {} // For serialization only

    // Deconstuctor
    ~BookSimInitEvent()
    {

    }

    // int getIntVal() {return int_val;}

    void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        Event::serialize_order(ser);
        ser & int_value;
        ser & ua_value;
    }

protected:
   

private:
    ImplementSerializable(BookSimInitEvent);

};

class BookSimCreditEvent : public BookSimBaseEvent {

public:

    int nid; // Credit's destination node
    int vc;
    int credits;

    // Constructor
    BookSimCreditEvent(int nid, int vc, int credits) :
    BookSimBaseEvent(BookSimBaseEvent::CREDIT),
    nid(nid),
    vc(vc),
    credits(credits)
    {}

    BookSimCreditEvent() {} // For serialization only

    // Deconstuctor
    ~BookSimCreditEvent()
    {

    }

    void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        Event::serialize_order(ser);
        ser & nid;
        ser & vc;
        ser & credits;
    }

protected:
   

private:
    ImplementSerializable(BookSimCreditEvent);

};

} // namespace BookSim
} // namespace SST

#endif