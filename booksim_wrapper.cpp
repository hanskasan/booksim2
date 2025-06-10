// HANS KASAN
// CNSL-KAIST
// September 2022
// Based on sst-macro/sstmac/hardware/logp/logp_switch.c and sst-macro/sstmac/hardware/pisces/pisces_switch.c

#ifndef BOOKSIM_STANDALONE

#include <cmath>

#include <sstmac/hardware/booksim2/booksim_wrapper.hpp>
#include <sstmac/hardware/nic/nic.h>
#include <sstmac/hardware/network/network_message.h>

RegisterDebugSlot(booksim_wrapper);

// Global declarations from BookSim's standalone main.cpp
TrafficManager * trafficManager = NULL;

bool gPrintActivity;
int  gK; //radix
int  gN; //dimension
int  gC; //concentration
int  gNodes;
bool gTrace;
bool gIsDragonfly;
int gLanes;
ostream * gWatchOut;

int64_t GetSimTime() {
  return trafficManager->getTime();
}

using namespace std;
namespace sstmac{
namespace hw{

class BookSimEvent : public ExecutionEvent
{
  public:
    ~BookSimEvent() override {}

    void execute() override{
      _wrap->Step();
    }

    BookSimEvent(BookSimWrapper* wrap):
      _wrap(wrap)
    {}

  protected:
    BookSimWrapper* _wrap;
};



BookSimWrapper::BookSimWrapper(uint32_t cid, SST::Params& params, EventManager* mgr) :
  ConnectableComponent(cid, params)
{
  SST::Params switch_params  = params.get_namespace("switch");
  SST::Params node_params    = params.get_namespace("node");
  SST::Params nic_params     = node_params.get_namespace("nic");
  SST::Params inj_params     = nic_params.get_namespace("injection");
  SST::Params booksim_params = params.get_namespace("booksim");


  // Are we using logp or pisces?
  std::string switch_model = switch_params->getLowercaseParam("name");
  _is_pisces = (switch_model == "pisces");

  // Read BookSim parameters
  _config.Assign("sim_type", "trace");

  int use_trace_rw = booksim_params.find<int>("use_trace_read_write");
  _config.Assign("use_trace_read_write", use_trace_rw);

  double freq = booksim_params.find<SST::UnitAlgebra>("frequency").getValue().toDouble();
  _period = Timestamp(1.0 / freq);

  _stats_print_period = booksim_params.find<int>("stats_print_period");

  _flit_size = booksim_params.find<int>("flit_size"); // in bytes
  cout << "Flit size: " << _flit_size << " bytes." << endl;
  _config.Assign("flit_size", _flit_size);

  int max_ps = booksim_params.find<int>("max_packet_size"); // in flits
  _config.Assign("max_packet_size", max_ps);

  std::string tp = booksim_params.find<string>("topology");
  _config.Assign("topology", tp);

  int k = booksim_params.find<int>("k");
  assert(k > 0);
  _config.Assign("k", k);

  int n = booksim_params.find<int>("n");
  assert(n > 0);
  _config.Assign("n", n);

  int c = booksim_params.find<int>("c");
  assert(c > 0);
  _config.Assign("c", c);

  int x = booksim_params.find<int>("x");
  assert(x > 0);
  _config.Assign("x", x);

  int y = booksim_params.find<int>("y");
  assert(y > 0);
  _config.Assign("y", y);

  int xr = booksim_params.find<int>("xr");
  assert(xr > 0);
  _config.Assign("xr", xr);

  int yr = booksim_params.find<int>("yr");
  assert(yr > 0);
  _config.Assign("yr", yr);

  int cl = booksim_params.find<int>("chan_latency");
  assert(cl > 0);
  cout << "Chan latency: " << cl << endl;
  _config.Assign("chan_latency", cl);

  int gcl = booksim_params.find<int>("global_chan_latency");
  _config.Assign("global_chan_latency", gcl);

  int noc_lat = booksim_params.find<int>("use_noc_latency");
  _config.Assign("use_noc_latency", noc_lat);

  std::string rf = booksim_params.find<string>("routing_function");
  _config.Assign("routing_function", rf);

  int ai = booksim_params.find<int>("alloc_iters");
  _config.Assign("alloc_iters", ai);

  int num_vcs = booksim_params.find<int>("num_vcs");
  assert(num_vcs > 0);
  _config.Assign("num_vcs", num_vcs);

  int vc_size = booksim_params.find<int>("vc_buf_size");
  assert(vc_size > 0);
  cout << "VC Buf Size: " << vc_size << endl;
  _config.Assign("vc_buf_size", vc_size);

  int gvc_size = booksim_params.find<int>("global_vc_buf_size");
  _config.Assign("global_vc_buf_size", gvc_size);

  int read_req_vc_begin = booksim_params.find<int>("read_request_begin_vc");
  _config.Assign("read_request_begin_vc", read_req_vc_begin);

  int read_req_vc_end = booksim_params.find<int>("read_request_end_vc");
  _config.Assign("read_request_end_vc", read_req_vc_end);

  int read_rep_vc_begin = booksim_params.find<int>("read_reply_begin_vc");
  _config.Assign("read_reply_begin_vc", read_rep_vc_begin);

  int read_rep_vc_end = booksim_params.find<int>("read_reply_end_vc");
  _config.Assign("read_reply_end_vc", read_rep_vc_end);

  int write_req_vc_begin = booksim_params.find<int>("write_request_begin_vc");
  _config.Assign("write_request_begin_vc", write_req_vc_begin);

  int write_req_vc_end = booksim_params.find<int>("write_request_end_vc");
  _config.Assign("write_request_end_vc", write_req_vc_end);

  int write_rep_vc_begin = booksim_params.find<int>("write_reply_begin_vc");
  _config.Assign("write_reply_begin_vc", write_rep_vc_begin);

  int write_rep_vc_end = booksim_params.find<int>("write_reply_end_vc");
  _config.Assign("write_reply_end_vc", write_rep_vc_end);

  double in_speedup = booksim_params.find<double>("internal_speedup");
  assert(in_speedup >= 1.0);
  _config.Assign("internal_speedup", in_speedup);

  int read_rep_size = booksim_params.find<int>("read_reply_size");
  _config.Assign("read_reply_size", read_rep_size);

  int write_rep_size = booksim_params.find<int>("write_reply_size");
  _config.Assign("write_reply_size", write_rep_size);

  int seed = booksim_params.find<int>("seed");
  _config.Assign("seed", seed);
  cout << "seed: " << seed << endl;

  // Initialize based on our configuration
  InitializeRoutingMap( _config );

  gPrintActivity = (_config.GetInt("print_activity") > 0);
  gTrace = (_config.GetInt("viewer_trace") > 0);
    
  // string watch_out_file = _config.GetStr( "watch_out" );
  // if(watch_out_file == "") {
  //   gWatchOut = NULL;
  // } else if(watch_out_file == "-") {
  //   gWatchOut = &cout;
  // } else {
  //   gWatchOut = new ofstream(watch_out_file.c_str());
  // }
  gWatchOut = &cout;
    
  // Build network
  int subnets = _config.GetInt("subnets");
  /*To include a new network, must register the network here
  *add an else if statement with the name of the network
  */
  _net.resize(subnets);
  for (int i = 0; i < subnets; ++i) {
    ostringstream name;
    name << "network_" << i;
    _net[i] = Network::New( _config, name.str() );
  }

  cout << "Subnet: " << subnets << endl;

  // Print configuration
  // cout << "Local and global VC buffer size is " << vc_size << " and " << gvc_size << ", respectively." << endl;

  // Build traffic manager
  assert(trafficManager == NULL);
  trafficManager = TrafficManager::New( _config, _net );

  _links_to_nodes.resize(gNodes);
  _credit_links_to_nodes.resize(gNodes);

  // Initialize credits for Pisces
  if (_is_pisces){
    _credit_counters.resize(gNodes);

    int initial_credit = inj_params.find<SST::UnitAlgebra>("credits").getRoundedValue();
    cout << "Initial credit: " << initial_credit << endl;
    for (int i = 0; i < gNodes; i++){
      // int my_id = cid + i + 1;
      // std::string name;
      // name << "credit_counter_" << my_id;
      _credit_counters[i] = new BookSimCreditCounter();
      _credit_counters[i]->InitializeCredit(initial_credit);
    }
  }

  // Initializations
  _mgr = mgr;
  // _now_ptr = now_ptr;

  _num_injected_events = 0;
  _num_ejected_events  = 0;

  _is_booksim_active = false;

#ifdef BYPASS_BOOKSIM
  _event_vect.resize(gNodes);
#endif
}

BookSimWrapper::~BookSimWrapper()
{
}

void BookSimWrapper::InjectTrace(Event* ev)
{
  Timestamp now = _mgr->now();

  int64_t booksim_time_sst = now.nsec() / _period.nsec();

  // Forward if needed
  if (!_is_booksim_active){
    _is_booksim_active = true;

    trafficManager->_ForwardTime(booksim_time_sst);

    ScheduleNextStep();
  }

// Increment counter
  _num_injected_events += 1;

  int src;
  int dest;
  int length;

  if (_is_pisces){
    PiscesPacket* payload = static_cast<PiscesPacket*>(ev);

    src  = payload->fromaddr();
    dest = payload->toaddr();
    length = payload->byteLength();
  } else {
    NicEvent* nev = static_cast<NicEvent*>(ev);
    NetworkMessage* msg = nev->msg();

    src  = msg->fromaddr();
    dest = msg->toaddr();
    length = msg->byteLength();
  }

  // HANS: For debugging
  // std::cout << now.usec() << " | " << booksim_time_sst << " | " << GetSimTime() << " - From: " << src << " To: " << dest << " Size: " << length << std::endl;

#ifndef BYPASS_BOOKSIM
  trafficManager->_InjectTrace(src, dest, length, ev);
#else
  _event_vect[dest].push(ev);
#endif



}

void BookSimWrapper::EjectTrace()
{
  for (int n = 0; n < gNodes; n++){

#ifndef BYPASS_BOOKSIM
    Event* ev = trafficManager->_GetArrivedEvent(n);
#else
    Event* ev;

    if (_event_vect[n].empty()){
      ev = NULL;
    } else {
      ev = _event_vect[n].front();
      _event_vect[n].pop();
    }
#endif
    if (ev == NULL){
      continue;
    }
    
    bool can_eject = true;

    if (_is_pisces){
      PiscesPacket* payload = static_cast<PiscesPacket*>(ev);

      if (payload->byteLength() > _credit_counters[n]->GetCredit()){
        can_eject = false;
        assert(0); // HANS: For debugging
      } else {
        _credit_counters[n]->useCredit(payload->byteLength());
      }
    }

    if (can_eject){

      // trafficManager->_PopArrivedEvent(n);

      // Increment counter
      _num_ejected_events += 1;

      // Print stats frequently
      if ((_num_ejected_events % _stats_print_period) == 0){
        cout << "*** PRINTING STATS WITH " << _num_ejected_events << " EVENTS AT " << _mgr->now().nsec() << endl;
        cout << "Current BookSim time is " << GetSimTime() << " cycle. ";
        cout << "BookSim is forwarded " << trafficManager->_GetForwardedTime() << " cycles." << endl;
        cout << "BookSim has run for " << GetSimTime() - trafficManager->_GetForwardedTime() << " cycles." << endl;
        trafficManager->DisplayStats();
      }

      int dest;

      if (_is_pisces){
        PiscesPacket* payload = static_cast<PiscesPacket*>(ev);
        dest = payload->toaddr();
        // cout << "Eject at " << dest << " | arrival: " << payload->arrival().sec() << endl;
        _links_to_nodes[dest]->send(TimeDelta(0), payload);
      } else {
        NicEvent* nev = static_cast<NicEvent*>(ev);
        NetworkMessage* msg = nev->msg();
        dest = msg->toaddr();
        _links_to_nodes[dest]->send(TimeDelta(0), nev);
      }
    } else {
      // HANS: This should not occur. If it occurs, we really need to fix it.
      assert(0);
    }
  }
}

void BookSimWrapper::Step()
{
  // if (!_is_booksim_active){
  //   _is_booksim_active = true;

  //   int booksim_time_sst = _mgr->now().nsec() / _period.nsec();
  //   trafficManager->_ForwardTime(booksim_time_sst);
  // }

  trafficManager->_Step();

  EjectTrace();

  ReturnCredit();

  if (ShouldBookSimRun()){
    ScheduleNextStep();
  } else {
    _is_booksim_active = false;
    DisplayOverallStats();
  }

}

void BookSimWrapper::ReturnCredit()
{
  if (_is_pisces){
    for (int n = 0; n < gNodes; n++){
      int num_bytes = trafficManager->GetSSTCredits(n);

      if (num_bytes > 0){
        // cout << "Send credit, NumBytes " << num_bytes << " at node " << n << endl;
        PiscesCredit* credit = new PiscesCredit(0, 0, num_bytes);
        _credit_links_to_nodes[n]->send(TimeDelta(0), credit);
      }
    }
  } else {
    // Do nothing
  }
}
  
bool BookSimWrapper::ShouldBookSimRun()
{
  // assert(_num_injected_events >= _num_ejected_events);
  // return (_num_injected_events > _num_ejected_events);

  bool is_should = trafficManager->_ShouldBookSimRun();

  if (!is_should){
    cout << "BookSim not running with " << _num_injected_events << " injected and " << _num_ejected_events << " ejected events." << endl;
    assert(_num_injected_events == _num_ejected_events);
  }

  return is_should;
}

void BookSimWrapper::ScheduleNextStep()
{
  BookSimEvent* new_event = new BookSimEvent(this);
  new_event->setTime(Timestamp(_mgr->now() + _period));
  _mgr->schedule(new_event);
}

void BookSimWrapper::DisplayOverallStats()
{
  trafficManager->UpdateStats();
  trafficManager->DisplayStats();

  // cout << "COMPARE INJECT-EJECT: " << _num_injected_events << " | " << _num_ejected_events << endl;

  cout << "*** OVERALL RUNTIME ***" << endl;
  cout << "Current BookSim time is " << GetSimTime() << " cycle. BookSim is forwarded " << trafficManager->_GetForwardedTime() << " cycles." << endl;
  cout << "Overall, BookSim has run for " << GetSimTime() - trafficManager->_GetForwardedTime() << " cycles." << endl;
  
  // trafficManager->PrintNlatDistribution();
  // trafficManager->PrintRetireTime();
}

}
}

#endif