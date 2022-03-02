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
    _num_motif_nodes(num_motif_nodes),
    port_out_credits(NULL)
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

    // Flit size
    flit_size = params.find<UnitAlgebra>("flitSize", found); 
    if ( !found ) {
        assert(0); // Has to be configured by the configuration file
    }
    if ( !flit_size.hasUnits("b") && !flit_size.hasUnits("B") ) {
        assert(0); // Unit has to be specified
    }
    if ( flit_size.hasUnits("B") ) {
        flit_size *= UnitAlgebra("8b");
    }

    // Buffer size
     input_buf_size = params.find<UnitAlgebra>("input_buf_size",found);
    if ( !found ) {
        assert(0); // Has to be configured by the configuration file
    }
    if ( !input_buf_size.hasUnits("b") && !input_buf_size.hasUnits("B") ) {
        assert(0); // Unit has to be specified
    }
    if ( input_buf_size.hasUnits("B") ) {
        input_buf_size *= UnitAlgebra("8b/B");
    }

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

    // // HANS: Double check
    std::cout << "Flit size (in bits): " << flit_size.getRoundedValue() << std::endl;
    std::cout << "Injection buffer size (in bits): " << input_buf_size.getRoundedValue() << std::endl;

    // Initialize credit arays
    // HANS: port_out_credits is an array with the size of "num_motif_nodes" instead of "num_vcs"
    port_out_credits = new int[num_motif_nodes];


    for (int iter_node = 0; iter_node < _num_motif_nodes; iter_node++){
        port_out_credits[iter_node] = 0;
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

    if ( port_out_credits != NULL ) delete [] port_out_credits;
}

void BookSimInterface::init(unsigned int phase)
{
    Event* ev;
    BookSimInitEvent* init_ev;
    BookSimCreditEvent* credit_ev;

    switch ( phase ) {
    case 0:
        for (int iter_node = 0; iter_node < _num_motif_nodes; iter_node++){
            // Send flit size
            BookSimInitEvent* flit_init_event = new BookSimInitEvent();
            flit_init_event->ua_value = flit_size;

            // Send link ID
            BookSimInitEvent* link_init_event = new BookSimInitEvent();
            link_init_event->int_value = iter_node;

            // Send init events
            _booksim_link_vect[iter_node]->sendInitData(flit_init_event);
            _booksim_link_vect[iter_node]->sendInitData(link_init_event);
        }
        break;

    case 1:
        // Initialize credit at BookSimBridge
        for (int iter_node = 0; iter_node < _num_motif_nodes; iter_node++){
            // HANS: FIXME: Should be configurable from the configuration file
            int input_buf_size_int = input_buf_size.getRoundedValue();
            int flit_size_int = flit_size.getRoundedValue();
            int available_credit = input_buf_size_int / flit_size_int;

            // HANS: Sanity check, input_buf_size_int > flit_size_int
            assert(available_credit > 0);

            BookSimCreditEvent* credit_event = new BookSimCreditEvent(iter_node, 0, available_credit);
            _booksim_link_vect[iter_node]->sendInitData(credit_event);
        }
        break;

    default:

        // Need to recv the credits sent from the other side
        for (int iter_node = 0; iter_node < _num_motif_nodes; iter_node++){
            while ( ( ev = _booksim_link_vect[iter_node]->recvInitData() ) != NULL ) {

                BookSimBaseEvent* base_ev = static_cast<BookSimBaseEvent*>(ev);

                if (base_ev->getType() == BookSimBaseEvent::CREDIT) {

                    credit_ev = static_cast<BookSimCreditEvent*>(base_ev);
                    assert(credit_ev->vc == 0); // There is no VC differentiation at BookSim injection queue

                    port_out_credits[iter_node] += credit_ev->credits;

                    // Sanity checks
                    assert(credit_ev->credits > 0);
                    assert(port_out_credits[iter_node] > 0);

                    delete credit_ev;

                } else {
                    assert(0); // Should not go here
                }
            }
        }

        break;
    }
}

// From BookSimBridge to BookSim
void BookSimInterface::handle_input(Event* ev)
{
    // Write down what to do when there are new packets coming for the traffic manager
    // Can refer to PortControl::handle_input_n2r at portControl.cc

    // Cast to BookSimEvent
    BookSimBaseEvent* base_ev = static_cast<BookSimBaseEvent*>(ev);

    if (base_ev->getType() == BookSimEvent::CREDIT){
        BookSimCreditEvent* credit_event = static_cast<BookSimCreditEvent*>(base_ev);
        port_out_credits[credit_event->nid] += credit_event->credits;

        // FOR DEBUGGING
        // if (credit_event->nid == 0)
        //     std::cout << "Port out get credit, to: " << port_out_credits[0] << endl;

        delete credit_event;

        
    } else if (base_ev->getType() == BookSimEvent::PACKET) {

        BookSimEvent* booksim_event = static_cast<BookSimEvent*>(base_ev);

        // HANS: For debugging purpose, delete if not needed
        // printf("Handle input at booksimInterface with source node: %d, at time: %ld\n", booksim_event->getSrc(), getCurrentSimCycle());

        // Wake BookSim up if needed
        // Refer to PortControl::handle_input_n2r -> parent->getRequestNotifyOnEvent() for more details
        if (_parent->IsRequestAlarm()){
            _parent->WakeBookSim();
        }

        // Inject packet to BookSim
        _parent->Inject(booksim_event);

        // For debugging interface: bypassing BookSim
        // this->send(booksim_event->getDest(), booksim_event);

        // Send an event to wake "output_timing" up
        //int size = booksim_event->getSizeInFlits();
        //output_timing->send(nullptr);
    } else {
        assert(0); // Should not go here
    }
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

void BookSimInterface::send(int dest_node, BookSimBaseEvent* event)
{
    // Sanity checks
    assert((dest_node >= 0) && (dest_node < _num_motif_nodes));

    if (event->getType() == BookSimEvent::PACKET){
        // Subtract credits
        BookSimEvent* booksim_ev = static_cast<BookSimEvent*>(event);
        int size = booksim_ev->getSizeInFlits();
        assert(size > 0);
        port_out_credits[dest_node] -= size;

        // FOR DEBUGGING
        // if (dest_node == 0)
        //     std::cout << getCurrentSimCycle() << " - send to BookSimBridge with " << port_out_credits[dest_node] << " credits left." << endl;
    }

    _booksim_link_vect[dest_node]->send(event);
}

int BookSimInterface::getCredit(int dest_node)
{
    return port_out_credits[dest_node];
}

bool BookSimInterface::spaceToSend(int dest_node, int flits)
{
    if (port_out_credits[dest_node] < flits) return false;
    return true;
}