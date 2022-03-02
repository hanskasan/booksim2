// Hans Kasan
// CSNL - KAIST
// Derived from linkControl.cc

// Checklist
// 1. Make sure that there is only 1 VN since we don't want to support multiple VNs yet

//#ifdef JOHN // ignores everything

#include <sst_config.h>

#include "booksimBridge.h"

#include <sst/core/simulation.h>
#include <sst/core/timeLord.h>

namespace SST {
using namespace Interfaces;

namespace BookSim {

BookSimBridge::BookSimBridge(ComponentId_t cid, Params &params, int vns) :
    SST::Interfaces::SimpleNetwork(cid),
    // _id(cid),
    rtr_link(nullptr), output_timing(nullptr), congestion_timing(nullptr),
    req_vns(vns), used_vns(0), total_vns(0), vn_out_map(nullptr),
    vn_remap_out(nullptr), output_queues(nullptr), router_credits(nullptr),
    router_return_credits(nullptr), input_queues(nullptr),
    id(-1), logical_nid(-1), use_nid_map(false), job_id(0),
    curr_out_vn(0), waiting(true), have_packets(false), start_block(0),
    idle_start(0), is_idle(true),
    receiveFunctor(nullptr), sendFunctor(nullptr),
    network_initialized(false),
    output(Simulation::getSimulation()->getSimulationOutput()),
    sent(0)
{
    bool found = false;

    // Get the buffer sizes
    inbuf_size = params.find<UnitAlgebra>("input_buf_size", found);
    if ( !found ) {
        assert(0); // Has to be configured by the configuration file
    }
    if ( !inbuf_size.hasUnits("b") && !inbuf_size.hasUnits("B") ) {
        assert(0); // Should not go here
    }
    if ( inbuf_size.hasUnits("B") ) inbuf_size *= UnitAlgebra("8b/B");

    outbuf_size = params.find<UnitAlgebra>("output_buf_size", found);
    if ( !found ) {
        assert(0); // Has to be configured by the configuration file
    }
    if ( !outbuf_size.hasUnits("b") && !outbuf_size.hasUnits("B") ) {
        assert(0); // Should not go here
    }
    if ( outbuf_size.hasUnits("B") ) outbuf_size *= UnitAlgebra("8b/B");

    // Configure the links
    // For now give it a fake timebase.  Will give it the real timebase during init

    // Only support 1 VN for now
    assert(req_vns == 1);

    // Need to get the right port_name
    std::string port_name("rtr_port");
    if ( isAnonymous() ) {
        port_name = params.find<std::string>("port_name");
    }

    rtr_link = configureLink(port_name, std::string("1GHz"), new Event::Handler<BookSimBridge>(this,&BookSimBridge::handle_input));

    output_timing = configureSelfLink(port_name + "_output_timing", "1GHz",
            new Event::Handler<BookSimBridge>(this,&BookSimBridge::handle_output));

    // Get link bandwidth (HANS: Double-check with LinkControl)
    UnitAlgebra link_clock = params.find<UnitAlgebra>("booksim_link_clock", "1GHz", found); // Send 1 flit with 1GHz frequency
    TimeConverter* tc = getTimeConverter(link_clock);
    assert(tc);
    output_timing->setDefaultTimeBase(tc);

    // Input and output buffers.  Not all of them can be set up now.
    // Only those that are sized based on req_vns can be intialized
    // now.  Others will wait until init when we find out the rest of
    // the VN usage.
    input_queues = new network_queue_t[req_vns];

    // See if we need to set up a nid map
    job_id = params.find<int>("job_id",-1,found);
    use_nid_map = params.find<bool>("use_nid_remap",false);
    if ( found ) {
        if ( use_nid_map ) {
            std::string nid_map_name = std::string("job_") + std::to_string(job_id) + "_nid_map";

            int job_size = params.find<int>("job_size",-1);
            // if ( job_size == -1 ) {
            //     merlin_abort.fatal(CALL_INFO,1,"BookSimBridge: job_size must be set\n");
            // }
            logical_nid = params.find<nid_t>("logical_nid",-1);
            // if ( logical_nid == -1 ) {
            //     merlin_abort.fatal(CALL_INFO,1,"BookSimBridge: logical_nid must be set\n");
            // }
            // nid_map_shm = Simulation::getSharedRegionManager()->
            //     getGlobalSharedRegion(nid_map_name, job_size * sizeof(nid_t), new SharedRegionMerger());
            nid_map.initialize(nid_map_name, job_size * sizeof(nid_t));
        }
    }
    else {
        std::string nid_map_name = params.find<std::string>("nid_map_name",std::string());
        if ( !nid_map_name.empty() ) {
            int job_size = params.find<int>("job_size",-1);
            // if ( job_size == -1 ) {
            //     merlin_abort.fatal(CALL_INFO,1,"BookSimBridge: job_size must be set if nid_map_name is set\n");
            // }
            logical_nid = params.find<nid_t>("logical_nid",-1);
            // if ( logical_nid == -1 ) {
            //     merlin_abort.fatal(CALL_INFO,1,"BookSimBridge: logical_nid must be set if nid_map_name is set\n");
            // }
            nid_map.initialize(nid_map_name, job_size * sizeof(nid_t));
            use_nid_map = true;
        }
    }

    // Register statistics
    packet_latency = registerStatistic<uint64_t>("packet_latency");
    send_bit_count = registerStatistic<uint64_t>("send_bit_count");
    output_port_stalls = registerStatistic<uint64_t>("output_port_stalls");
    idle_time = registerStatistic<uint64_t>("idle_time");
    // recv_bit_count = registerStatistic<uint64_t>("recv_bit_count");
}

BookSimBridge::~BookSimBridge()
{
    delete [] vn_remap_out;
    delete [] output_queues;
    delete [] router_credits;
    delete [] router_return_credits;
    delete [] input_queues;
}

void BookSimBridge::setup()
{
    while ( init_events.size() ) {
        delete init_events.front();
        init_events.pop_front();
    }
}

void BookSimBridge::init(unsigned int phase)
{
    Event* ev;
    BookSimInitEvent* init_ev;
    BookSimCreditEvent* credit_ev;

    int credits;

    switch ( phase ) {
    case 0:
        network_initialized = true;

        // HANS: FIXME: Only support 1 VN for now
        total_vns = 1;

        // Need to start the timer for links that never send data
        idle_start = Simulation::getSimulation()->getCurrentSimCycle();
        is_idle = true;
        break;

    case 1:

        // Get the flit size from the router
        ev = rtr_link->recvUntimedData();
        init_ev = dynamic_cast<BookSimInitEvent*>(ev);
        flit_size_ua = init_ev->ua_value;
        flit_size = flit_size_ua.getRoundedValue();
        delete ev;

        // Get link ID
        ev = rtr_link->recvUntimedData();
        init_ev = static_cast<BookSimInitEvent*>(ev);
        id = init_ev->int_value;
        delete ev;

        // HANS: FIXME: This should be initialized in phase 2
        // Instance the output queues
        // /*
        vn_remap_out = new output_queue_bundle_t*[req_vns];
        output_queues = new output_queue_bundle_t[req_vns];

        output_queues[0].credits = (int)(outbuf_size / flit_size_ua).getRoundedValue();
        output_queues[0].vn = 0;

        vn_remap_out[0] = &(output_queues[0]);

        // FOR DEBUGGING
        if (id == 0)
            std::cout << "VN remap initialized to: " << vn_remap_out[0]->credits << std::endl;

        // Initialize the output tracking arrays
        router_return_credits = new int[total_vns];
        router_credits = new int[total_vns];
        for ( int i = 0; i < total_vns; ++i ) {
            router_return_credits[i] = 0;
            router_credits[i] = 0;
        }
        _max_credits = 0;

        // In the original linkControl code, the value of router_return_credits is initialized by considering the virtual network mapping
        // But since we only assume 1 VN, mapping is trivial..
        router_return_credits[0] = (int)(inbuf_size / flit_size_ua).getRoundedValue();

        // Need to send the available credits to other side of link
        credits = router_return_credits[0];
        if (credits > 0){
            rtr_link->sendUntimedData(new BookSimCreditEvent(id, 0, credits));
            router_return_credits[0] = 0;
        }

        break;

    default:
        // For all other phases, look for credit events, any other
        // events get passed up to containing component by adding them
        // to init_events queue

        while ( ( ev = rtr_link->recvUntimedData() ) != nullptr ) {

            BookSimBaseEvent* base_ev = static_cast<BookSimBaseEvent*>(ev);

            if (base_ev->getType() == BookSimBaseEvent::CREDIT) {
                credit_ev = static_cast<BookSimCreditEvent*>(base_ev);
                assert(credit_ev->vc == 0); // There is no VC differentiation at BookSim injection queue

                router_credits[credit_ev->vc] += credit_ev->credits;

                if (id == 0)
                    std::cout << "Credits initialized to: " << router_credits[0] << " | " << credit_ev->credits << std::endl;

                // Record the initial value of the credits. The 'router_credits' values should not exceed this.
                if (credit_ev->vc == 0)
                    _max_credits = router_credits[credit_ev->vc];

                // Sanity checks
                assert(credit_ev->credits > 0);
                assert(router_credits > 0);

                delete credit_ev;
                // delete ev;
            } else {
                assert(0); // Should not go here
            }
        }
        break;
    }
}

void BookSimBridge::complete(unsigned int phase)
{
  
}


void BookSimBridge::finish(void)
{
    if (is_idle) {
        idle_time->addData(Simulation::getSimulation()->getCurrentSimCycle() - idle_start);
        is_idle = false;
    }

    // Clean up all the events left in the queues.  This will help
    // track down real memory leaks as all this events won't be in the
    // way.
    for ( int i = 0; i < req_vns; i++ ) {
        while ( !input_queues[i].empty() ) {
            delete input_queues[i].front();
            input_queues[i].pop();
        }
    }
    for ( int i = 0; i < used_vns; i++ ) {
        while ( !output_queues[i].queue.empty() ) {
            delete output_queues[i].queue.front();
            output_queues[i].queue.pop();
        }
    }
}


// Returns true if there is space in the output buffer and false
// otherwise.
bool BookSimBridge::send(SimpleNetwork::Request* req, int vn) {

    // Check to see if the VN is in range
    assert (vn == 0); // Only support VN 0 for now
    if ( vn >= req_vns ) return false;
    req->vn = vn;

    // Check to see if we need to do a nid translation
    if ( use_nid_map ) req->dest = nid_map[req->dest];

    // Get the output queue information for VN 0
    output_queue_bundle_t& out_handle = *(vn_remap_out[0]);

    // Create a router event using id
    // id will always be -1 (the value is initialized by PortControl in the Merlin implementation)    
    // But the id is not needed for now, but this will be a problem when the "event->getTrustedSrc()" function is needed
    BookSimEvent* ev = new BookSimEvent(req, id);

    // Fill in the number of flits
    ev->computeSizeInFlits(flit_size);

    int flits = ev->getSizeInFlits();

    // Check to see if there are enough credits to send
    if ( out_handle.credits < flits ) return false;

    // Update the credits
    out_handle.credits -= flits;

    // // HANS: For debugging, delete if not needed
    // if (req->src == 0)
        // cout << getCurrentSimCycle() << " - New BookSimEvent, size_in_bits: " << ev->getSizeInBits() << ", size_in_flits: " << ev->getSizeInFlits() << ", from: " << req->src << ", to: " << req->dest << endl;
    //     printf("%ld: new BookSimEvent with id: %d, size: %d, flit_size: %d, from: %d to: %d, credit: %d\n", getCurrentSimCycle(), id, flits, flit_size, req->src, req->dest, out_handle.credits);

    ev->setInjectionTime(getCurrentSimTimeNano());
    out_handle.queue.push(ev);

    if ( waiting && !have_packets ) {
        //output_timing->send(1,nullptr);
        output_timing->send(nullptr);
        waiting = false;
    }

    return true;
}


// Credit is sent everytime BookSim injects a packet to the network
bool BookSimBridge::spaceToSend(int vn, int bits) {
    assert(vn == 0); // Only support 1 VN for now
    if ( vn_remap_out[vn]->credits * flit_size < bits) return false;
    return true;
}


// Returns nullptr if no event in input_buf[vn]. Otherwise, returns
// the next event.
SST::Interfaces::SimpleNetwork::Request* BookSimBridge::recv(int vn) {

    // Only support VN 0 for now
    assert(vn == 0);

    if ( input_queues[vn].size() == 0 ) return nullptr;

    BookSimEvent* event = input_queues[vn].front();
    input_queues[vn].pop();

    // Figure out how many credits to return
    int flits = event->getSizeInFlits();
    assert(flits > 0);
    router_return_credits[vn] += flits;
    rtr_link->send(1,new BookSimCreditEvent(id, 0, router_return_credits[vn]));
    router_return_credits[vn] = 0;

    SST::Interfaces::SimpleNetwork::Request* ret = event->takeRequest();
    if ( use_nid_map ) ret->dest = logical_nid;
    delete event;

    // FOR DEBUGGING
    // if (id == 0)
    //     printf("Successfully sent to NIC at: %ld from source: %d, head: %d, tail: %d, input queue size: %d\n", getCurrentSimCycle(), ret->src, ret->head, ret->tail, input_queues[vn].size());

    // rtr_link->send(1, nullptr);
    
    return ret;

}

// Handle event coming from BookSim
void BookSimBridge::handle_input(Event* ev)
{

    // HANS: For debugging purpose, delete if not needed
    // printf("Handle input at booksimBridge at time: %ld\n", getCurrentSimCycle());

    // Cast to BookSimBaseEvent
    BookSimBaseEvent* base_ev = static_cast<BookSimBaseEvent*>(ev);

    // In the original linkControl file, the vn index value is obtained from the event
    // But for now, it will be assumed that the vn index is always 0
    int vn = 0;

    if ( base_ev->getType() == BookSimBaseEvent::CREDIT ) {
        BookSimCreditEvent* credit_event = static_cast<BookSimCreditEvent*>(base_ev);
        assert(credit_event->vc == 0);
        router_credits[credit_event->vc] += credit_event->credits;
        assert(router_credits[credit_event->vc] <= _max_credits);

        // if (id == 0)
        //     std::cout << "Receive credits: " << router_credits[credit_event->vc] << " | " << credit_event->vc << endl;

        delete base_ev;

        // If we're waiting, we need to send a wakeup event to the
        // output queues
        if ( waiting ) {
            output_timing->send(1, nullptr);
            waiting = false;

            /*
            // If we were stalled waiting for credits and we had
            // packets, we need to add stall time
            if ( have_packets) {
                output_port_stalls->addData(Simulation::getSimulation()->getCurrentSimCycle() - start_block);
            }
            */
        }

    } else if ( base_ev->getType() == BookSimBaseEvent::PACKET ){
        BookSimEvent* booksim_event = static_cast<BookSimEvent*>(base_ev);

        input_queues[vn].push(booksim_event);
        if (is_idle) {
            idle_time->addData(Simulation::getSimulation()->getCurrentSimCycle() - idle_start);
            is_idle = false;
        }

        SimTime_t lat = getCurrentSimTimeNano() - booksim_event->getInjectionTime();
        // recv_bit_count->addData(event->getSizeInBits());
        packet_latency->addData(lat);
        if ( receiveFunctor != nullptr ) {
            bool keep = (*receiveFunctor)(vn);
            if ( !keep) receiveFunctor = nullptr;
        }
    } else {
        // Should not go here
        assert(0);
    }
}

// Control event to be sent to BookSim
void BookSimBridge::handle_output(Event* ev)
{
    // In the original linkControl code, round robin scheduling is done.
    // But since only 1 VN is assumed, we don't need round robin for now
    // Thus, we don't need the bool variable "found"

    int vn = 0;
    have_packets = false;

    if ( !output_queues[vn].queue.empty() ){
        // For debugging purpose, delete if not needed
        // printf("Handle output at booksimBridge, found at: %ld\n", getCurrentSimCycle());

        have_packets = true;
        
        BookSimEvent* send_event = nullptr;
        send_event = output_queues[vn].queue.front();
        int send_size = send_event->getSizeInFlits();

        if (router_credits[vn] >= send_event->getSizeInFlits()){
            // std::cout << getCurrentSimCycle() <<  " - VACANT: " << router_credits[vn] << " | " << send_event->getSizeInFlits() << endl;

            output_queues[vn].queue.pop();

            // Return credits to the output buffer
            int size = send_event->getSizeInFlits();
            // For debugging purpose, delete if not needed
            // printf("Add credit from: %d to %d\n", output_queues[vn].credits, output_queues[vn].credits + size);
            output_queues[vn].credits += size;

            // Send an event to wake up again after this packet is sent
            output_timing->send(nullptr);
            //output_timing->send(send_size, nullptr);

            // Usage of output_timing is skipped

            // Add in inject time so we can track latencies
            send_event->setInjectionTime(getCurrentSimTimeNano());

            // Subtract credits 
            router_credits[vn] -= send_size;

            // if (id == 0)
            //     std::cout << "Router credits: " << router_credits[vn] << std::endl;

            if (is_idle){
                idle_time->addData(Simulation::getSimulation()->getCurrentSimCycle() - idle_start);
                is_idle = false;
            }

            rtr_link->send(send_event);   
            last_recv_time = getCurrentSimCycle();
            sent++;

            send_bit_count->addData(send_event->getSizeInBits());
            if (sendFunctor != nullptr ) {
                bool keep = (*sendFunctor)(0); // VN is always 0. The original version is: send_event->getLogicalVN()
                if ( !keep ) sendFunctor = nullptr;
            }
        } else {
            // if (id == 100)
            //     std::cout << Simulation::getSimulation()->getCurrentSimCycle() << " - NO CREDIT: " << router_credits[vn] << " | " << send_event->getSizeInFlits() << " at ID: " << id << " MaxCredit: " << _max_credits << endl;

            waiting = true; // Because there is no enough credit to proceed
        }
    } else {
        // For debugging purpose, delete if not needed
        //printf("Handle output at booksimBridge, not found\n");

        // If there's nothing to send
        // Based on the original code, there are 2 possibilities: the output queues are empty or there's no enough room in the router buffers
        // However, since we are not dealing with credits when connecting with the traffic manager, the latter possibility can be ignored.
        start_block = Simulation::getSimulation()->getCurrentSimCycle();
        waiting = true;
        // Begin counting the amount of time this port was idle
        if (!have_packets && !is_idle) {
            idle_start = Simulation::getSimulation()->getCurrentSimCycle();
            is_idle = true;
        }
		// Should be in a stalled state rather than idle
		if (have_packets && is_idle){
            idle_time->addData(Simulation::getSimulation()->getCurrentSimCycle() - idle_start);
            is_idle = false;
        }
    }
}

// To fulfill the virtual class requirement that all functions have to be derived
void BookSimBridge::sendInitData(SST::Interfaces::SimpleNetwork::Request* req) 
{

}

SST::Interfaces::SimpleNetwork::Request* BookSimBridge::recvInitData() 
{
    return 0;
}

} // namespace BookSim
} // namespace SST

//#endif