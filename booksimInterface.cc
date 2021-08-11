// Hans Kasan
// CSNL - KAIST
// Subcomponent derived from BookSimInterface_Base

#include "sst_config.h"

#include "booksimInterface.h"

#include <string.h>

using namespace SST;
using namespace BookSim;

BookSimInterface::BookSimInterface(ComponentId_t id, Params& params, booksim2* parent, int num_motif_nodes) :
    BookSimInterface_Base(id),
    _parent(parent),
    _num_motif_nodes(num_motif_nodes)
{

    // Sanity check(s)
    assert(_num_motif_nodes >= 0);

    // Resize vector of links
    _booksim_link_vect.resize(_num_motif_nodes);

    // Resize vector of queue of BookSimEvent: bypassing BookSim
    //_event_vect.resize(_num_motif_nodes);

    // CAUTION: Configuration parameter is not read from emberLoadBookSim yet. Fixed the parameter passing in emberLoadBookSim for this to work

    // BookSimInterface clock supplied from SST
    bool found;
    // std::string booksiminterface_clock = params.find<string>("booksim_clock", "1GHz", found);
    // clock_handler = new Clock::Handler<BookSimInterface>(this, &BookSimInterface::handle_output);
    // registerClock("1GHz", clock_handler);

    std::string booksim_clock = params.find<string>("booksim_clock", "1GHz", found);
    //output_timing = configureSelfLink("output_timing", booksim_clock, new Event::Handler<BookSimInterface>(this, &BookSimInterface::handle_output));

    // Get link bandwidth (HANS: Double-check with LinkControl)
    UnitAlgebra link_clock = params.find<UnitAlgebra>("booksim_link_clock", booksim_clock, found); // Send 1 flit with 1GHz frequency
    TimeConverter* tc = getTimeConverter(link_clock);
    // assert(tc);
    // output_timing->setDefaultTimeBase(tc);

    for (int iter_node = 0; iter_node < _num_motif_nodes; iter_node++){
        std::stringstream motif_node_name;
        motif_node_name << "motif_node";
        motif_node_name << iter_node;        

        // HANS: For debugging purpose, delete if not needed
        // printf("Port: %d\n", iter_node);

        _booksim_link_vect[iter_node] = configureLink(motif_node_name.str(), new BookSimEvent::Handler<BookSimInterface>(this, &BookSimInterface::handle_input));
        sst_assert(_booksim_link_vect[iter_node], CALL_INFO, -1, "Error in %s: Link configuration for 'booksim_link' failed\n", getName().c_str());
        _booksim_link_vect[iter_node]->setDefaultTimeBase(tc);
    }
}

BookSimInterface::~BookSimInterface()
{
    //For debugging interface: bypassing BookSim
    // for (int iter_node = 0; iter_node < _num_motif_nodes; iter_node++){
    //     while(!_event_vect[iter_node].empty()){
    //         _event_vect[iter_node].pop();
    //     }
    // }

    _num_motif_nodes = -1;
}

void BookSimInterface::init(unsigned int phase)
{
    switch ( phase ) {
    case 0:
        for (int iter_node = 0; iter_node < _num_motif_nodes; iter_node++){
            BookSimInitEvent* init_event = new BookSimInitEvent(iter_node);
            _booksim_link_vect[iter_node]->sendInitData(init_event);
        }
        break;

    default:
        break;
    }
}

// From BookSimBridge to BookSim
void BookSimInterface::handle_input(Event* ev)
{
    // Write down what to do when there are new packets coming for the traffic manager
    // Can refer to PortControl::handle_input_n2r at portControl.cc

    // Cast to BookSimEvent
    BookSimEvent* booksim_event = static_cast<BookSimEvent*>(ev);

    // HANS: For debugging purpose, delete if not needed
    //printf("Handle input at booksimInterface with source node: %d, at time: %ld\n", booksim_event->getSrc(), getCurrentSimCycle());

    // Wake BookSim up if needed
    // Refer to PortControl::handle_input_n2r -> parent->getRequestNotifyOnEvent() for more details
    if (_parent->IsRequestAlarm()){
        _parent->WakeBookSim();
    }

    // Inject packet to BookSim
    _parent->Inject(booksim_event);

    

    // For debugging interface: bypassing BookSim
    //_event_vect[booksim_event->getDest()].push(booksim_event);

    // Send an event to wake "output_timing" up
    //int size = booksim_event->getSizeInFlits();
    //output_timing->send(nullptr);
}

/*
void BookSimInterface::handle_output(Event* ev)
{
    // Write down what to do when there are new packets coming for the traffic manager
    // Can refer to PortControl::handle_input_n2r at portControl.cc

    // For debugging interface: bypassing BookSim
    //printf("Handle output at booksimInterface\n");

    for (int iter_node = 0; iter_node < _num_motif_nodes; iter_node++){
        if(!_event_vect[iter_node].empty()){
            BookSimEvent* booksim_event = _event_vect[iter_node].front();

            //printf("BookSimInterface send to BookSimBridge for node: %d\n", iter_node);
            _booksim_link_vect[iter_node]->send(booksim_event);

            _event_vect[iter_node].pop();
        }
    }
}
*/

void BookSimInterface::send(int dest_node, BookSimEvent* event)
{
    // Sanity checks
    assert((dest_node >= 0) && (dest_node < _num_motif_nodes));

    _booksim_link_vect[dest_node]->send(event);
}