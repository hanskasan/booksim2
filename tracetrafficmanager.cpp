#ifndef BOOKSIM_STANDALONE

#include <limits>
#include <sstream>
#include <fstream>

#include "packet_reply_info.hpp"
#include "tracetrafficmanager.hpp"
#include "random_utils.hpp" 

TraceTrafficManager::TraceTrafficManager( const Configuration &config, 
					  const vector<Network *> & net )
: TrafficManager(config, net), _forwarded_time(0)
{
    _sim_state = running;

    _flit_size = config.GetInt("flit_size"); // in bytes
    _max_packet_size = config.GetInt("max_packet_size"); // in flits

    _tracesPending.resize(_nodes);
    for (int s = 0; s < _nodes; s++){
        _tracesPending[s].resize(_classes);
    }
}

TraceTrafficManager::~TraceTrafficManager( )
{
    
}

void TraceTrafficManager::_GenerateTrace( int source, int dest, int payload_bytes, int cl, sstmac::Event* ev)
{
    Flit::FlitType packet_type = Flit::ANY_TYPE;
    int pid;
    int packet_destination = dest;
    bool record = true;
    int64_t time = _time;

    int num_flits;
    int num_packets;
    if (payload_bytes == 0){
        num_flits = 1;
        num_packets = 1;
    } else {
        assert(payload_bytes > 0);
        num_flits = payload_bytes / _flit_size; // _flit_size is in bytes
        if ((payload_bytes % _flit_size) > 0)
            num_flits += 1;

        num_packets = num_flits / _max_packet_size; // _max_packet_size is in bits
        if ((num_flits % _max_packet_size) > 0)
            num_packets += 1;
    }

    // HANS: Sanity check for destination
    if ((packet_destination <0) || (packet_destination >= _nodes)) {
        ostringstream err;
        err << "Incorrect packet destination " << packet_destination
            << " for stype " << packet_type;
        Error( err.str( ) );
    }

    assert(_subnets == 1); // HANS: Assume 1 subnet for now
    int subnetwork = RandomInt(_subnets - 1);

    // HANS: For debugging
    // if (source == 0)
    // cout << GetSimTime() << " - Num_packets: " << num_packets << ", Num_flits: " << num_flits << ", Payload: " << payload_bytes << ", MaxPacketSize: " << _max_packet_size << ", FlitSize: " << _flit_size << ", source: " << source << ", dest: " << dest << endl;
    
    for ( int i = 0; i < num_packets; ++i ) {

        pid = _cur_pid++;
        assert(_cur_pid);

        bool watch = gWatchOut && (_packets_to_watch.count(pid) > 0);

        if ( watch ) { 
            *gWatchOut << GetSimTime() << " | "
                       << "node" << source << " | "
                       << "Enqueuing packet " << pid
                       << " at time " << time
                       << "." << endl;
        }
  
        int size;
        if (payload_bytes == 0){
            assert(num_packets == 1);
            size = 1;
        } else {
            if ((i + 1) >= num_packets){ // At the very last payloads
                assert((i + 1) == num_packets);
                size = num_flits;
            } else {
                size = _max_packet_size; // in flits
                num_flits -= _max_packet_size;
                assert(num_flits > 0);
            }
        }

        // HANS: For debugging
        // if ((source == 0) && (dest == 1))
            // cout << GetSimTime() << " - Bytes: " << payload_bytes << ", num_packets: " << num_packets << ", source " << source << ", dest " << dest << ", size " << size << ", pid " << pid <<  endl;

        // HANS: For now.. Feel free to delete this if not needed
        // assert(size == 1);

        _injected_trace_packets += 1;
        _injected_trace_flits += size;

        for ( int j = 0; j < size; ++j ) {
            Flit * f  = Flit::New();
            f->id     = _cur_id++;
            assert(_cur_id >= 0);
            f->pid    = pid;
            assert(pid >= 0);
            f->watch  = watch | (gWatchOut && (_flits_to_watch.count(f->id) > 0));
            f->subnetwork = subnetwork;
            f->src    = source;
            f->dest   = packet_destination;
            f->ctime  = time;
            f->record = record;
            f->cl     = cl;
            f->event  = NULL;

            _total_in_flight_flits[f->cl].insert(make_pair(f->id, f));
            if(record) {
                _measured_in_flight_flits[f->cl].insert(make_pair(f->id, f));
            }

            if(gTrace){
                cout<<"New Flit "<<f->src<<endl;
            }

            if (_use_trace_read_write)
                f->type = Flit::WRITE_REQUEST;
            else
                f->type = packet_type;

            if (i == 0) { // Head packet
                f->head_packet = true;
            } else {
                f->head_packet = false;
            }
            if ( j == 0 ) { // Head flit
                f->head  = true;
                f->size = size;
                f->size_in_bits = _flit_size * 8;
                //packets are only generated to nodes smaller or equal to limit
                // f->dest  = packet_destination;
            } else {
                f->head = false;
                // f->dest = -1;
            }
            switch( _pri_type ) {
            case class_based:
                f->pri = _class_priority[cl];
                assert(f->pri >= 0);
                break;
            case age_based:
                f->pri = numeric_limits<int>::max() - time;
                assert(f->pri >= 0);
                break;
            case sequence_based:
                f->pri = numeric_limits<int>::max() - _packet_seq_no[source];
                assert(f->pri >= 0);
                break;
            default:
                f->pri = 0;
            }

            if ( i == ( num_packets - 1 ) ) { // Tail packet
                f->tail_packet = true;
                if ( j == 0 ){ // Head flit at the tail packet
                    f->event = ev;
                }
            } else {
                f->tail_packet = false;
            }
            if ( j == ( size - 1 ) ) { // Tail flit
                f->tail = true;
            } else {
                f->tail = false;
            }

            f->vc  = -1;

            if ( f->watch ) { 
                *gWatchOut << GetSimTime() << " | "
                           << "node" << source << " | "
                           << "Enqueuing flit " << f->id
                           << " (packet " << f->pid
                           << ") at time " << time
                           << " to dest " << packet_destination
                           << " with " << _tracesPending[source][cl].size()
                           << " flit ahead." << endl;
            }

            _tracesPending[source][cl].push_back(f);
        }
    }
}

void TraceTrafficManager::_InjectTraceReply(){
    assert(_use_trace_read_write);

    for ( int input = 0; input < _nodes; ++input ){
        for ( int c = 0; c < _classes; ++c ) {
            if ((!_repliesPending[input].empty()) && (_partial_packets[input][c].empty())){ // Possibly issue reply
            // if ((!_repliesPending[input].empty()) && (_partial_packets_count[input][c][0] == 0)){ // Possibly issue reply
              GenerateTraceReply(input, c);
            } else {
              break;
            }
        }
    }
}

void TraceTrafficManager::_IssueTrace(){
    for ( int input = 0; input < _nodes; ++input ){
        for ( int c = 0; c < _classes; ++c ){
            if ((_repliesPending[input].empty()) && (_partial_packets[input][c].empty())){ // Prioritize replies
                while (!_tracesPending[input][c].empty()) {
                    Flit * const f = _tracesPending[input][c].front();
                    _tracesPending[input][c].pop_front();

                    _partial_packets[input][c].push_back(f);

                    if ((f->tail_packet) && (f->tail)) // Issue one message at a time
                        break;
                }
            }
        }
    }
}

void TraceTrafficManager::GenerateTraceReply(int source, int cl){
    Flit::FlitType packet_type = Flit::WRITE_REPLY;
    int size;
    int pid = _cur_pid++;
    assert(_cur_pid);
    int packet_destination;
    bool record = true;
    bool watch = gWatchOut && (_packets_to_watch.count(pid) > 0);
    int64_t time;

    assert(_use_trace_read_write);

    // Generate reply
    PacketReplyInfo* rinfo = _repliesPending[source].front();
    if (rinfo->type == Flit::READ_REQUEST) {//read reply
        assert(0); // HANS: Should not go here for now
        size = _read_reply_size[cl];
        packet_type = Flit::READ_REPLY;
    } else if(rinfo->type == Flit::WRITE_REQUEST) {  //write reply
        size = _write_reply_size[cl];
        packet_type = Flit::WRITE_REPLY;
    } else {
        ostringstream err;
        err << "Invalid packet type: " << rinfo->type;
        Error( err.str( ) );
    }
    packet_destination = rinfo->source;
    time = rinfo->time;
    _repliesPending[source].pop_front();
    rinfo->Free();

    // To keep BookSim running when there are replies waiting to be sent
    _count_pendingreplies -= 1;
    assert(_count_pendingreplies >= 0);

    // Issue credit for the ejection ports
    // HANS: Additional info
    // int vc = rinfo->vc;
    // int subnetwork = rinfo->subnetwork;
    int subnetwork = _subnet[packet_type];

    if ((packet_destination <0) || (packet_destination >= _nodes)) {
        ostringstream err;
        err << "Incorrect packet destination " << packet_destination
            << " for stype " << packet_type;
        Error( err.str( ) );
    }
  
    if ( watch ) { 
        *gWatchOut << GetSimTime() << " | "
                   << "node" << source << " | "
                   << "Enqueuing packet " << pid
                   << " at time " << time
                   << "." << endl;
    }
  
    for ( int i = 0; i < size; ++i ) {
        Flit * f  = Flit::New();
        f->id     = _cur_id++;
        assert(_cur_id >= 0);
        f->pid    = pid;
        assert(pid >= 0);
        f->watch  = watch | (gWatchOut && (_flits_to_watch.count(f->id) > 0));
        f->subnetwork = subnetwork;
        f->src    = source;
        f->dest   = packet_destination;
        f->ctime  = time;
        f->record = record;
        f->cl     = cl;

        // if (f->id == 4998341)
            // f->watch = true;

        _total_in_flight_flits[f->cl].insert(make_pair(f->id, f));
        if(record) {
            _measured_in_flight_flits[f->cl].insert(make_pair(f->id, f));
        }
    
        if(gTrace){
            cout<<"New Flit "<<f->src<<endl;
        }
        
        f->type = packet_type;

        if ( i == 0 ) { // Head flit
            f->head  = true;
            //packets are only generated to nodes smaller or equal to limit
            // f->dest  = packet_destination;
            f->event = NULL;
            f->size_in_bits = _flit_size * 8;
        } else {
            f->head = false;
            // f->dest = -1;
        }
        switch( _pri_type ) {
        case class_based:
            f->pri = _class_priority[cl];
            assert(f->pri >= 0);
            break;
        case age_based:
            f->pri = numeric_limits<int>::max() - time;
            assert(f->pri >= 0);
            break;
        case sequence_based:
            f->pri = numeric_limits<int>::max() - _packet_seq_no[source];
            assert(f->pri >= 0);
            break;
        default:
            f->pri = 0;
        }
        if ( i == ( size - 1 ) ) { // Tail flit
            f->tail = true;
        } else {
            f->tail = false;
        }
    
        f->vc  = -1;

        if ( f->watch ) { 
            *gWatchOut << GetSimTime() << " | "
                       << "node" << source << " | "
                       << "Enqueuing flit " << f->id
                       << " (packet " << f->pid
                       << ") at time " << time
                       << "." << endl;
        }

        _partial_packets[source][cl].push_back( f );
    }
// #endif
}

#endif