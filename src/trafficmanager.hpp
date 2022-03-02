// $Id$

/*
 Copyright (c) 2007-2015, Trustees of The Leland Stanford Junior University
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this 
 list of conditions and the following disclaimer.
 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _TRAFFICMANAGER_HPP_
#define _TRAFFICMANAGER_HPP_

#include <list>
#include <map>
#include <set>
#include <cassert>

#include "module.hpp"
#include "config_utils.hpp"
#include "networks/network.hpp"
#include "flit.hpp"
#include "buffer_state.hpp"
#include "stats.hpp"
#include "traffic.hpp"
#include "routefunc.hpp"
#include "outputset.hpp"
#include "injection.hpp"

// HANS: ADDITIONAL FEATURES
// #define DYNAMIC_TRAFFIC

#ifndef BOOKSIM_STANDALONE
struct retired_info {
  int eject_time;
  int subnet;
  int pid;
  int sst_src;
  int src;
  int vc;

  // retired_info() :
  //   eject_time(-1),
  //   pid(-1),
  //   src(-1),
  //   vc(-1)
  //   {}
};
#endif

//register the requests to a node
class PacketReplyInfo;

class TrafficManager : public Module {

private:

  vector<vector<int> > _packet_size;
  vector<vector<int> > _packet_size_rate;
  vector<int> _packet_size_max_val;

protected:
  int _nodes;
  int _routers;
  int _vcs;

  vector<Network *> _net;
  vector<vector<Router *> > _router;

  // ============ Traffic ============ 

  int    _classes;

#ifdef DYNAMIC_TRAFFIC
  vector<double> _load[2];
#else
  vector<double> _load;
#endif

  vector<int> _use_read_write;
  vector<double> _write_fraction;

  vector<int> _read_request_size;
  vector<int> _read_reply_size;
  vector<int> _write_request_size;
  vector<int> _write_reply_size;


#ifdef DYNAMIC_TRAFFIC
  int _change_time;

  // HANS: Currently, DYNAMIC_TRAFFIC mode only supports running up to 2 traffic for a single simulation run
  vector<string> _traffic[2];
#else
  vector<string> _traffic;
#endif

  vector<int> _class_priority;

  vector<vector<int> > _last_class;

#ifdef DYNAMIC_TRAFFIC
  vector<TrafficPattern *> _traffic_pattern[2];
  vector<InjectionProcess *> _injection_process[2];
#else
  vector<TrafficPattern *> _traffic_pattern;
  vector<InjectionProcess *> _injection_process;
#endif

  // ============ Message priorities ============ 

  enum ePriority { class_based, age_based, network_age_based, local_age_based, queue_length_based, hop_count_based, sequence_based, none };

  ePriority _pri_type;

  // ============ Injection VC states  ============ 

  vector<vector<BufferState *> > _buf_states;
#ifdef TRACK_FLOWS
  vector<vector<vector<int> > > _outstanding_credits;
  vector<vector<vector<queue<int> > > > _outstanding_classes;
#endif
  vector<vector<vector<int> > > _last_vc;

  // ============ Routing ============ 

  tRoutingFunction _rf;
  bool _lookahead_routing;
  bool _noq;

  // ============ Injection queues ============ 

  vector<vector<int> > _qtime;
  vector<vector<bool> > _qdrained;
  vector<vector<list<Flit *> > > _partial_packets;

  vector<map<int, Flit *> > _total_in_flight_flits;
  vector<map<int, Flit *> > _measured_in_flight_flits;
  vector<map<int, Flit *> > _retired_packets;
  bool _empty_network;

  bool _hold_switch_for_packet;

  // ============ physical sub-networks ==========

  int _subnets;

  vector<int> _subnet;

  // ============ deadlock ==========

#ifdef BOOKSIM_STANDALONE
  int _deadlock_timer;
  int _deadlock_warn_timeout;
#else
  uint64_t _deadlock_timer;
  uint64_t _deadlock_warn_timeout;
#endif

  // ============ request & replies ==========================

  vector<int> _packet_seq_no;
  vector<list<PacketReplyInfo*> > _repliesPending;
  vector<int> _requestsOutstanding;

#ifndef BOOKSIM_STANDALONE
  // ============ Synthetic background traffic ============ 
  int _synthetic_nodes;  

#endif

  // ============ Statistics ============

  vector<Stats *> _plat_stats;     
  vector<double> _overall_min_plat;  
  vector<double> _overall_avg_plat;  
  vector<double> _overall_max_plat;  

  vector<Stats *> _nlat_stats;     
  vector<double> _overall_min_nlat;  
  vector<double> _overall_avg_nlat;  
  vector<double> _overall_max_nlat;  

  vector<Stats *> _flat_stats;     
  vector<double> _overall_min_flat;  
  vector<double> _overall_avg_flat;  
  vector<double> _overall_max_flat;  

  vector<Stats *> _frag_stats;
  vector<double> _overall_min_frag;
  vector<double> _overall_avg_frag;
  vector<double> _overall_max_frag;

  // HANS: Additional statistics
  vector<Stats *> _plat_frequent_stats;
  // vector<Stats *> _flat_frequent_stats;
  vector<Stats *> _plat_frequent_min_stats;
  vector<Stats *> _plat_frequent_non_stats;

  vector<vector<Stats *> > _pair_plat;
  vector<vector<Stats *> > _pair_nlat;
  vector<vector<Stats *> > _pair_flat;

  vector<Stats *> _hop_stats;
  vector<double> _overall_hop_stats;

  vector<vector<int> > _sent_packets;
  vector<double> _overall_min_sent_packets;
  vector<double> _overall_avg_sent_packets;
  vector<double> _overall_max_sent_packets;
  vector<vector<int> > _accepted_packets;
  vector<double> _overall_min_accepted_packets;
  vector<double> _overall_avg_accepted_packets;
  vector<double> _overall_max_accepted_packets;
  vector<vector<int> > _sent_flits;
  vector<double> _overall_min_sent;
  vector<double> _overall_avg_sent;
  vector<double> _overall_max_sent;
  vector<vector<int> > _accepted_flits;
  vector<double> _overall_min_accepted;
  vector<double> _overall_avg_accepted;
  vector<double> _overall_max_accepted;

#ifdef TRACK_STALLS
  vector<vector<int> > _buffer_busy_stalls;
  vector<vector<int> > _buffer_conflict_stalls;
  vector<vector<int> > _buffer_full_stalls;
  vector<vector<int> > _buffer_reserved_stalls;
  vector<vector<int> > _crossbar_conflict_stalls;
  vector<double> _overall_buffer_busy_stalls;
  vector<double> _overall_buffer_conflict_stalls;
  vector<double> _overall_buffer_full_stalls;
  vector<double> _overall_buffer_reserved_stalls;
  vector<double> _overall_crossbar_conflict_stalls;
#endif

  vector<int> _slowest_packet;
  vector<int> _slowest_flit;

  map<string, Stats *> _stats;

  // HANS: Additional statistics
  vector<Stats *> _plat_min_stats;
  vector<Stats *> _plat_non_stats;

  vector<double> _overall_avg_plat_min;
  vector<double> _overall_avg_plat_non;
  vector<int   > _overall_n_plat_min;
  vector<int   > _overall_n_plat_non;

  // ============ Simulation parameters ============ 

  enum eSimState { warming_up, running, draining, done };
  eSimState _sim_state;

  bool _measure_latency;

  int   _reset_time;
  int   _drain_time;

  int   _total_sims;
  int   _sample_period;
  int   _max_samples;
  int   _warmup_periods;

  int   _include_queuing;

  vector<int> _measure_stats;
  bool _pair_stats;

  vector<double> _latency_thres;

  vector<double> _stopping_threshold;
  vector<double> _acc_stopping_threshold;

  vector<double> _warmup_threshold;
  vector<double> _acc_warmup_threshold;

  int _cur_id;
  int _cur_pid;
//#ifdef BOOKSIM_STANDALONE
  int _time;
// #else
//   uint64_t _time;
// #endif

#ifndef BOOKSIM_STANDALONE
  int _jumped_time;
#endif

  set<int> _flits_to_watch;
  set<int> _packets_to_watch;

  bool _print_csv_results;

  //flits to watch
  ostream * _stats_out;

#ifdef TRACK_FLOWS
  vector<vector<int> > _injected_flits;
  vector<vector<int> > _ejected_flits;
  ostream * _injected_flits_out;
  ostream * _received_flits_out;
  ostream * _stored_flits_out;
  ostream * _sent_flits_out;
  ostream * _outstanding_credits_out;
  ostream * _ejected_flits_out;
  ostream * _active_packets_out;
#endif

#ifdef TRACK_CREDITS
  ostream * _used_credits_out;
  ostream * _free_credits_out;
  ostream * _max_credits_out;
#endif

#ifndef BOOKSIM_STANDALONE
  vector<queue< retired_info > > _retired_pid;
  vector<vector<queue<Credit* > > > _endpoint_credits;
  vector<int> _sst_credits;

  // Record latency
  // 0-5000, resolution 100
  // +1 class to count packets with latency >5000
  static const int _resolution = 100;
  static const int _num_cell = 51;

  int _plat_class[_num_cell] = {0};
  int _nlat_class[_num_cell] = {0};
  int _max_plat = 0;
#endif

  // ============ Internal methods ============ 
protected:

#ifdef BOOKSIM_STANDALONE
  virtual void _RetireFlit( Flit *f, int dest );
#else
  virtual void _RetireFlit( Flit *f, int subnet, int dest );
#endif

#ifdef BOOKSIM_STANDALONE
  void _Inject();
  void _Step( );
#endif

#ifdef BOOKSIM_STANDALONE
  bool _PacketsOutstanding( ) const;
  bool _PacketsOutstanding( int c ) const;
#endif
  
  virtual int  _IssuePacket( int source, int cl );
  void _GeneratePacket( int source, int size, int cl, int time );
  
#ifndef BOOKSIM_STANDALONE
  int _GeneratePacketfromMotif( int sst_source, int source, int dest, int size, int c );
#endif

#ifdef BOOKSIM_STANDALONE
  virtual void _ClearStats( );
#endif

  void _ComputeStats( const vector<int> & stats, int *sum, int *min = NULL, int *max = NULL, int *min_pos = NULL, int *max_pos = NULL ) const;

  virtual bool _SingleSim( );

#ifdef BOOKSIM_STANDALONE
  void _DisplayRemaining( ostream & os = cout ) const;
#endif
  
  void _LoadWatchList(const string & filename);

#ifdef BOOKSIM_STANDALONE
  virtual void _UpdateOverallStats();
#endif

  virtual string _OverallStatsCSV(int c = 0) const;

  int _GetNextPacketSize(int cl) const;
  double _GetAveragePacketSize(int cl) const;

public:

  static TrafficManager * New(Configuration const & config, 
			      vector<Network *> const & net);

  TrafficManager( const Configuration &config, const vector<Network *> & net );
  virtual ~TrafficManager( );

  bool Run( );

  virtual void WriteStats( ostream & os = cout ) const ;
  virtual void UpdateStats( ) ;
  virtual void DisplayStats( ostream & os = cout ) const ;
  virtual void DisplayOverallStats( ostream & os = cout ) const ;
  virtual void DisplayOverallStatsCSV( ostream & os = cout ) const ;

  // HANS: Additional functions
  virtual void DisplayAvgLatFrequently( ostream & os = cout, int period = -1 ) const ;

//#ifdef BOOKSIM_STANDALONE
  inline int getTime() { 
    assert(_time >= 0); // Make sure that this value is not negative due to overflow
    return _time;
  }

//#else
  //inline uint64_t getTime() { return _time;}
//#endif
  Stats * getStats(const string & name) { return _stats[name]; }

#ifndef BOOKSIM_STANDALONE
  bool _PacketsOutstanding( ) const;
  bool _PacketsOutstanding( int c ) const;
  int _CreditsOutstanding( ) {return Credit::OutStanding();}

  void _Step( );
  int _InjectMotif ( int sst_source, int source, int dest, int size );
  void _InjectBackground ( );

  bool IsRetiredPidEmpty    (int dest) const;
  bool IsAllRetiredPidEmpty () const;
  retired_info  GetRetiredPid        (int dest);
  int  GetSSTCredits  (int src);

  void InjectEndpointCredit (int node, int subnet, int vc);

  virtual void _UpdateOverallStats();

  virtual void _ClearStats( );

  void _DisplayRemaining( ostream & os = cout ) const;

  void jumpTime(int future) {
    assert(future >= 0);
    printf("Jump time from: %d to %d\n", _time, future);
    _jumped_time += future - _time;
    //_time = future;

    for (int i = 0; i < (future - _time); i++){
      _Step();
    }
  }

  int getRunTime(){
    int net_time = _time - _jumped_time;
    assert(net_time >= 0);
    return net_time;
  }

  void PrintPlatDistribution(){

    cout << "*** PACKET LATENCY DISTRIBUTION *** " << endl;
    for (int iter_cell = 0; iter_cell < _num_cell; iter_cell++){
      cout << iter_cell * _resolution << "\t" << _plat_class[iter_cell] << endl;
    }
    cout << "*** END ***" << endl;
    
    cout << "Maximum packet latency: " << _max_plat << endl;
  }

  void PrintNlatDistribution(){

    cout << "*** NETWORK LATENCY DISTRIBUTION *** " << endl;
    for (int iter_cell = 0; iter_cell < _num_cell; iter_cell++){
      cout << iter_cell * _resolution << "\t" << _nlat_class[iter_cell] << endl;
    }
    cout << "*** END ***" << endl;
    
  }

#endif

};

template<class T>
ostream & operator<<(ostream & os, const vector<T> & v) {
  for(size_t i = 0; i < v.size() - 1; ++i) {
    os << v[i] << ",";
  }
  os << v[v.size()-1];
  return os;
}

#endif
