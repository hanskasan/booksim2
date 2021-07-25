// Hans Kasan
// CSNL - KAIST
// Subcomponent derived from BookSimInterface_Base

#ifndef COMPONENTS_BOOKSIM_BOOKSIMINTERFACE_H
#define COMPONENTS_BOOKSIM_BOOKSIMINTERFACE_H

#include "booksim2.h"

#include <sst/core/event.h>
#include <sst/core/link.h>
#include <sst/core/params.h>

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
        SST::BookSim::BookSimInterface_Base)

    SST_ELI_DOCUMENT_PORTS(
        {"motif_node%(num_motif_nodes)d",  "Motif nodes which connect to Ember and Firefly.", { "booksim2.BookSimEvent" } }
    )
    

private:
    booksim2* _parent;
    int _num_motif_nodes;

    Link** _booksim_link;

public:
    BookSimInterface(ComponentId_t id, Params& params, booksim2* parent, int num_motif_nodes);
    ~BookSimInterface();

    // Event handler from the link
    void handle_input  (Event* ev);
    void handle_output (Event* ev);
};

} // namespace BookSim
} // namespace SST

#endif // COMPONENTS_BOOKSIM_BOOKSIMINTERFACE_H
