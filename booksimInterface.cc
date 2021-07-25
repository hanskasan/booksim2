// Hans Kasan
// CSNL - KAIST
// Subcomponent derived from BookSimInterface_Base

#include <sst_config.h>

#include "booksimInterface.h"

using namespace SST;
using namespace BookSim;

BookSimInterface::BookSimInterface(ComponentId_t id, Params& params, booksim2* parent, int num_motif_nodes) :
    BookSimInterface_Base(id),
    _parent(parent),
    _num_motif_nodes(num_motif_nodes)
{

    // Sanity check(s)
    assert(_num_motif_nodes >= 0);

    for (int iter_node = 0; iter_node < _num_motif_nodes; iter_node++){
        std::stringstream motif_node_name;
        motif_node_name << "motif_node";
        motif_node_name << iter_node;        

        _booksim_link[iter_node] = configureLink(motif_node_name.str(), new Event::Handler<BookSimInterface>(this, &BookSimInterface::handle_input));
    }

}

BookSimInterface::~BookSimInterface()
{
    _num_motif_nodes = -1;
}

void BookSimInterface::handle_input(Event* ev)
{
    // Write down what to do when there are new packets coming for the traffic manager
    // Can refer to PortControl::handle_input_n2r at portControl.cc
}

void BookSimInterface::handle_output(Event* ev)
{
    // Write down what to do when there are new packets coming for the traffic manager
    // Can refer to PortControl::handle_input_n2r at portControl.cc
}