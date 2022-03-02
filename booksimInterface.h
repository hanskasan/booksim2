// Hans Kasan
// CSNL - KAIST
// Subcomponent derived from BookSimInterface_Base

#ifndef COMPONENTS_BOOKSIM_BOOKSIMINTERFACE_H
#define COMPONENTS_BOOKSIM_BOOKSIMINTERFACE_H

#include "booksim2.h"

#include <sst/core/clock.h>
#include <sst/core/event.h>
#include <sst/core/link.h>
#include <sst/core/params.h>

#include <vector>

namespace SST {
namespace BookSim {

class BookSimInterface : public BookSimInterface_Base {

public:

    SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(
        BookSimInterface,
        "booksim2",
        "booksiminterface",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "Interface between BookSimBridge and BookSim",
        SST::BookSim::BookSimInterface_Base
    )
    
    SST_ELI_DOCUMENT_PARAMS(
        {"num_motif_nodes",     "number of motif nodes"}
    )

    SST_ELI_DOCUMENT_PORTS(
        {"motif_node%(num_motif_nodes)d",  "Motif nodes which connect to Ember and Firefly.", { "booksim2.BookSimEvent" } }
    )
    

private:
    booksim2* _parent;
    int _num_motif_nodes;

    vector<Link*> _booksim_link_vect;

    UnitAlgebra flit_size;
    UnitAlgebra input_buf_size;

    // Variables to keep track of credits.  You need to keep track of
    // the credits available for your next buffer, as well as track
    // the credits you need to return to the buffer sending data to
    // you,
    int* port_out_credits;

    //Clock::Handler<BookSimInterface>* clock_handler;

    //vector<queue<BookSimEvent*> > _event_vect;

    //Link* output_timing;

public:
    BookSimInterface(ComponentId_t id, Params& params, booksim2* parent, int num_motif_nodes);
    ~BookSimInterface();

    void init (unsigned int phase);

    // Event handler from the link
    void handle_input  (Event* ev);
    //void handle_output (Event* ev);
    //bool handle_output(Cycle_t cycle);

    // Send to BookSimBridge (BookSim to Ember)
    void send (int dest_node, BookSimBaseEvent* ev);
    int getCredit(int dest_node);
    bool spaceToSend (int dest_node, int flits);
};

} // namespace BookSim
} // namespace SST

#endif // COMPONENTS_BOOKSIM_BOOKSIMINTERFACE_H
