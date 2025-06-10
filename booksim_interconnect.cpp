// HANS KASAN
// CNSL-KAIST
// August 2022
// Based on sst-macro/sstmac/hardware/interconnect/interconnect.cc

#ifndef BOOKSIM_STANDALONE

#include <sstmac/hardware/booksim2/booksim_interconnect.hpp>
#include <sstmac/hardware/node/node.h>
#include <sstmac/hardware/nic/nic.h>
#include <sstmac/hardware/topology/topology.h>
#include <sstmac/common/runtime.h>
#include <sstmac/common/event_manager.h>
#include <sstmac/backends/common/parallel_runtime.h>
#include <sstmac/backends/common/sim_partition.h>

#include <cassert>

RegisterDebugSlot(booksim_interconnect);


#define interconn_debug(str, ...) \
  debug_printf(sprockit::dbg::interconnect, "Rank %d: " str, EventManager::global->me(), __VA_ARGS__)

namespace sstmac {
namespace hw {

BookSimInterconnect* BookSimInterconnect::_static_booksiminterconnect = nullptr;



BookSimInterconnect*
BookSimInterconnect::staticBookSimInterconnect(SST::Params& params, EventManager* mgr)
{
  if (!_static_booksiminterconnect){
    ParallelRuntime* rt = ParallelRuntime::staticRuntime(params);
    Partition* part = rt ? rt->topologyPartition() : nullptr;
    _static_booksiminterconnect = new BookSimInterconnect(params, mgr, part, rt);
  }
  return _static_booksiminterconnect;
}

BookSimInterconnect*
BookSimInterconnect::staticBookSimInterconnect()
{
  if (!_static_booksiminterconnect){
    spkt_abort_printf("interconnect not initialized");
  }
  return _static_booksiminterconnect;
}

#if !SSTMAC_INTEGRATED_SST_CORE
BookSimInterconnect::~BookSimInterconnect()
{
}
#endif

BookSimInterconnect::BookSimInterconnect(SST::Params& params, SSTMAC_MAYBE_UNUSED EventManager * mgr, SSTMAC_MAYBE_UNUSED Partition * part, SSTMAC_MAYBE_UNUSED ParallelRuntime * rt)
: Interconnect(params, mgr, part, rt)
{
  if (!_static_booksiminterconnect) _static_booksiminterconnect = this;
  topology_ = Topology::staticTopology(params);
  Runtime::setTopology(topology_);

  num_nodes_ = topology_->numNodes();
  // num_nodes_ = _booksim_wrapper->GetNumNodes();
  // num_nodes_ = app_params.find<int>("size");

  components_.resize(num_nodes_ + 1); // +1 for BookSimWrapper

  partition_ = part;
  rt_ = rt;
  int nproc = rt_->nproc();

  SST::Params node_params = params.get_namespace("node");
  SST::Params nic_params = node_params.get_namespace("nic");
  SST::Params switch_params = params.get_namespace("switch");
  SST::Params app_params = node_params.get_namespace("app1");

  // Topology* top = topology_;

  // Are we using logp or pisces?
  std::string switch_model = switch_params->getLowercaseParam("name");
  _is_pisces = (switch_model == "pisces");

  
  // Build BookSim Wrapper
  // uint32_t my_offset = rt_->me() * rt_->nthread() + top->numNodes() + top->numSwitches();
  uint32_t my_offset = rt_->me() * rt_->nthread() + 2 * num_nodes_;
  uint32_t id = my_offset + 1; // +1 offset

  assert(rt_->nthread() == 1); // HANS: Only support single thread for now
  mgr->setComponentManager(id, 0);
  _now_ptr = mgr->nowPtr();
  _booksim_wrapper = new BookSimWrapper(id, params, mgr);

  std::cout << "NumNodes: " << num_nodes_ << std::endl;
  nodes_.resize(num_nodes_);

  // buildEndpoints(node_params, nic_params, mgr);

  if (_is_pisces)
    uint64_t linkId = connectBookSimPisces(0/*number from zero*/, mgr, node_params, nic_params);
  else
    uint64_t linkId = connectBookSimLogP(0/*number from zero*/, mgr, node_params, nic_params);
  // configureInterconnectLookahead(params);
}

void BookSimInterconnect::configureInterconnectLookahead(SST::Params& params)
{
  // HANS: Assuming that injection_latency is always smaller than the hop latency
  SST::Params inj_params = params.get_namespace("node").get_scoped_params("nic").get_scoped_params("injection");
  TimeDelta injection_latency = TimeDelta(inj_params.find<SST::UnitAlgebra>("latency").getValue().toDouble());

  lookahead_ = injection_latency;
}

uint64_t BookSimInterconnect::connectBookSimLogP(uint64_t linkIdOffset, EventManager* mgr, SST::Params& /*node_params*/, SST::Params& nic_params)
{
  SST::Params inj_params = nic_params.get_namespace("injection");
  SST::Params empty{};

  int my_rank = rt_->me();

  uint64_t linkId = linkIdOffset;

  // Connect endpoints to BookSim
  for (int n = 0; n < num_nodes_; n++){
    int target_thread = partition_->threadForSwitch(n);
    int target_rank = partition_->lpidForSwitch(n);

    if (target_rank == my_rank){
      Node* nd = nodes_[n];

      auto* nodetobooksim_link = new LocalLink(linkId++, TimeDelta(0), mgr->threadManager(target_thread), _booksim_wrapper->payloadHandler(n));
      nd->nic()->connectOutput(NIC::LogP, n, EventLink::ptr(nodetobooksim_link));

      auto* booksimtonode_link = new LocalLink(linkId++, TimeDelta(0), mgr->threadManager(target_thread), nd->payloadHandler(NIC::LogP));
      _booksim_wrapper->connectOutput(n, 0, EventLink::ptr(booksimtonode_link));
    } else {
      linkId += 2;
    }
  }

  return linkId;
}

uint64_t BookSimInterconnect::connectBookSimPisces(uint64_t linkIdOffset, EventManager* mgr, SST::Params& /*node_params*/, SST::Params& nic_params)
{
  SST::Params inj_params = nic_params.get_namespace("injection");
  SST::Params empty{};

  int my_rank = rt_->me();

  uint64_t linkId = linkIdOffset;

  // Connect endpoints to BookSim
  for (int n = 0; n < num_nodes_; n++){
    int target_thread = partition_->threadForSwitch(n);
    int target_rank = partition_->lpidForSwitch(n);

    if (target_rank == my_rank){
      Node* nd = nodes_[n];

      // INJECTION
      auto* booksimtonode_credit_link = new LocalLink(linkId++, TimeDelta(0), mgr->threadManager(target_thread), nd->creditHandler(n));
      _booksim_wrapper->connectInput(n, 0, EventLink::ptr(booksimtonode_credit_link));

      auto* nodetobooksim_payload_link = new LocalLink(linkId++, TimeDelta(0), mgr->threadManager(target_thread), _booksim_wrapper->payloadHandler(n));
      nd->connectOutput(NIC::Injection, 0, EventLink::ptr(nodetobooksim_payload_link));

      // EJECTION
      auto* booksimtonode_payload_link = new LocalLink(linkId++, TimeDelta(0), mgr->threadManager(target_thread), nd->payloadHandler(NIC::Injection));
      _booksim_wrapper->connectOutput(n, 0, EventLink::ptr(booksimtonode_payload_link));

      auto* nodetobooksim_credit_link = new LocalLink(linkId++, TimeDelta(0), mgr->threadManager(target_thread), _booksim_wrapper->creditHandler(n));
      nd->connectInput(0, NIC::Injection, EventLink::ptr(nodetobooksim_credit_link));
    } else {
      linkId += 4;
    }
  }

  return linkId;
}

}
}

#endif