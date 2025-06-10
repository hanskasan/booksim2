#ifndef BOOKSIM_STANDALONE

#ifndef _TRACETRAFFICMANAGER_HPP_
#define _TRACETRAFFICMANAGER_HPP_

#include <iostream>

#include <sstmac/hardware/nic/nic.h>
#include <sstmac/common/sst_event_fwd.h>

#include "config_utils.hpp"
#include "stats.hpp"
#include "trafficmanager.hpp"

class TraceTrafficManager : public TrafficManager {

protected:

public:

  TraceTrafficManager( const Configuration &config, const vector<Network *> & net );
  virtual ~TraceTrafficManager( );

  virtual void _GenerateTrace(int source, int dest, int payload_bytes, int cl, sstmac::Event* ev);
  virtual void _InjectTrace(int source, int dest, int payload_bytes, sstmac::Event* ev) {
    _GenerateTrace(source, dest, payload_bytes, 0, ev); // All packets from trace are assigned to class 0
  }

  virtual void _InjectTraceReply();

  virtual void _IssueTrace();

  virtual void _ForwardTime(int64_t new_time){
    if (new_time < _time)
      cout << "Error - cannot jump from " << _time << " to " << new_time << endl;

    assert(new_time >= _time);
    _forwarded_time += (new_time - _time);

    _time = new_time;
  }

  virtual sstmac::Event* _GetArrivedEvent(int dest){
    assert((dest >= 0) && (dest < gNodes));
    if (_arrived_event[dest].empty()){
        return NULL;
    } else {
        sstmac::Event* temp = _arrived_event[dest].front();
        _arrived_event[dest].pop();
        _count_waiting_events -= 1;
        assert(_count_waiting_events >= 0);
        return temp;
    }
  }

  // virtual void _PopArrivedEvent(int dest){
  //   _arrived_event[dest].pop();
  //   _count_waiting_events -= 1;
  //   assert(_count_waiting_events >= 0);
  // }

  virtual void _UpdateOverallStats(){
    _drain_time = _time;
    TrafficManager::_UpdateOverallStats();
  }

  virtual int64_t _GetForwardedTime(){
    return _forwarded_time;
  }

private:
    int64_t _forwarded_time;
    
    int _flit_size;
    int _max_packet_size;

    void GenerateTraceReply(int source, int cl);

    // HANS: Waiting room for the traces
    vector<vector<list<Flit *> > > _tracesPending;

};

#endif
#endif