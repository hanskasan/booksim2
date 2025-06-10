// HANS KASAN
// CNSL-KAIST
// August 2022
// Based on sst-macro/sstmac/hardware/interconnect/interconnect.h

#ifndef BOOKSIM_STANDALONE

#ifndef SSTMAC_HARDWARE_BOOKSIM_INTERCONNECT_H_INCLUDED
#define SSTMAC_HARDWARE_BOOKSIM_INTERCONNECT_H_INCLUDED

#include <sstmac/hardware/booksim2/booksim_wrapper.hpp>
#include <sstmac/common/event_scheduler.h>
#include <sstmac/common/timestamp.h>
#include <sstmac/hardware/interconnect/interconnect.h>
#include <sstmac/hardware/topology/topology_fwd.h>

#include <sstmac/backends/common/parallel_runtime_fwd.h>
#include <sstmac/backends/common/sim_partition_fwd.h>

#include <sprockit/debug.h>
#include <sprockit/factory.h>

DeclareDebugSlot(booksim_interconnect)

namespace sstmac {
namespace hw {

class BookSimInterconnect : public Interconnect
{
 public:
   static BookSimInterconnect* staticBookSimInterconnect(SST::Params& params, EventManager* mgr);

   static BookSimInterconnect* staticBookSimInterconnect();

   static void clearStaticBookSimInterconnect(){
       if (_static_booksiminterconnect) delete _static_booksiminterconnect;
       _static_booksiminterconnect = nullptr;
   }

   BookSimInterconnect(SST::Params& params, EventManager* mgr, Partition* part, ParallelRuntime* rt);
   ~BookSimInterconnect();

   virtual void DisplayOverallStats() { _booksim_wrapper->DisplayOverallStats(); }


 private:
   static BookSimInterconnect* _static_booksiminterconnect;
   BookSimWrapper* _booksim_wrapper;

   uint64_t connectBookSimLogP(uint64_t idOffset, EventManager* mgr, SST::Params& /*node_params*/, SST::Params& nic_params);
   uint64_t connectBookSimPisces(uint64_t idOffset, EventManager* mgr, SST::Params& /*node_params*/, SST::Params& nic_params);

   const Timestamp* _now_ptr;

   bool _is_pisces;

 protected:
   virtual void configureInterconnectLookahead(SST::Params& params);

};

}
} // end of namespace sstmac

#endif
#endif