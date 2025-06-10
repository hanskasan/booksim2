// HANS KASAN
// CNSL-KAIST
// September 2022
// Based on sst-macro/sstmac/hardware/logp/logp_switch.h

#ifndef BOOKSIM_STANDALONE

#ifndef SSTMAC_HARDWARE_BOOKSIM_WRAPPER_H_INCLUDED
#define SSTMAC_HARDWARE_BOOKSIM_WRAPPER_H_INCLUDED

// HANS: For debugging only
// #define BYPASS_BOOKSIM

#include <sstmac/common/event_handler.h>
#include <sstmac/common/event_manager.h>
#include <sstmac/common/event_scheduler.h>
#include <sstmac/common/sst_event.h>
#include <sstmac/common/timestamp.h>
#include <sstmac/hardware/common/connection.h>
#include <sstmac/hardware/pisces/pisces.h>
#include <sprockit/sim_parameters.h>

#include <sstmac/hardware/booksim2/booksim_config.hpp>
#include <sstmac/hardware/booksim2/globals.hpp>
#include <sstmac/hardware/booksim2/trafficmanager.hpp>

DeclareDebugSlot(booksim_wrapper)

namespace sstmac {
namespace hw {

// class BookSimCreditCounter : public ConnectableSubcomponent
class BookSimCreditCounter
{
  public:
    BookSimCreditCounter():
    _credit(0)
    // BookSimCreditCounter(uint32_t cid, std::string& name, SST::Component* parent):
    // ConnectableSubcomponent(cid, name, parent), _credit(0)
    {}

    ~BookSimCreditCounter() {}

    std::string toString(){
      return "BookSim credit counter";
    }

    void InitializeCredit(int initial_credit){
      _credit = initial_credit;
      _max_credit = initial_credit;
    }

    void handleCredit(Event* ev){
      PiscesCredit* credit = static_cast<PiscesCredit*>(ev);
      _credit += credit->numCredits();

      assert(_credit <= _max_credit);
    }

    void useCredit(int bytes){
      _credit -= bytes;

      assert(_credit >= 0);
    }

    int GetCredit(){ return _credit; }

  protected:
    int _credit;
    int _max_credit;
};

class BookSimWrapper : public ConnectableComponent
{
 public:
  SST_ELI_REGISTER_COMPONENT(
    BookSim,
    "macro",
    "booksim_wrapper",
    SST_ELI_ELEMENT_VERSION(1,0,0),
    "Wrapper for the best network simulator ever",
    COMPONENT_CATEGORY_NETWORK)

  SST_ELI_DOCUMENT_PORTS(SSTMAC_VALID_PORTS)

 public:
  BookSimWrapper(uint32_t cid, SST::Params& params, EventManager* mgr);

  ~BookSimWrapper();

  std::string toString() const override {
    return "BookSim wrapper";
  }

  void connectInput(int src_outport, int, EventLink::ptr&& credit_link) override {
    _credit_links_to_nodes[src_outport] = std::move(credit_link);
  }

  void connectOutput(int src_outport, int  /*dst_inport*/, EventLink::ptr&& payload_link) override {
    _links_to_nodes[src_outport] = std::move(payload_link);
  }

  LinkHandler* payloadHandler(int /*port*/) override {
    return newLinkHandler(this, &BookSimWrapper::InjectTrace);
  }

  LinkHandler* creditHandler(int port) override {
    auto* counter = _credit_counters[port];
    return newLinkHandler(counter, &BookSimCreditCounter::handleCredit);
  }

  void InjectTrace  (Event *ev);
  void EjectTrace   ();

  void ReturnCredit ();

  int GetNumNodes(){
    assert(gNodes > 0);
    return gNodes;
  }

  void Step();

  void DisplayOverallStats();

 private:
  vector<EventLink::ptr>         _links_to_nodes;
  vector<EventLink::ptr>         _credit_links_to_nodes;
  vector<BookSimCreditCounter*>  _credit_counters; // For Pisces

  BookSimConfig       _config;
  vector<Network*>    _net;

  EventManager* _mgr;

  // const Timestamp* _now_ptr;
  Timestamp _period;

  int _stats_print_period;

  int _flit_size;

  unsigned int _num_injected_events;
  unsigned int _num_ejected_events;

  bool _is_pisces;
  bool _is_booksim_active;

#ifdef BYPASS_BOOKSIM
  vector<queue<Event*> > _event_vect;
#endif

  // double GetSSTTime() const {
  //   return _now_ptr->nsec();
  //   // return _now_ptr->nsecRounded();
  // }

  bool ShouldBookSimRun();
  void ScheduleNextStep();
};

}
}

#endif
#endif