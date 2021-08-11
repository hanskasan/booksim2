/*
  Copyright (c) 2007-2015, Trustees of The Leland Stanford Junior University
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification,
  are permitted provided that the following conditions are met:

  Redistributions of source code must retain the above copyright notice, this list
  of conditions and the following disclaimer.
  Redistributions in binary form must reproduce the above copyright notice, this 
  list of conditions and the following disclaimer in the documentation and/or 
  other materials provided with the distribution.
  Neither the name of the Stanford University nor the names of its contributors 
  may be used to endorse or promote products derived from this software without 
  specific prior written permission.

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

#include "../booksim.hpp"
#include <vector>
#include <sstream>

#include "dragonfly.hpp"
#include "../random_utils.hpp"
#include "../misc_utils.hpp"
#include "../globals.hpp"

#define DRAGON_LATENCY

int gP, gA, gG;
int gGK, gPX, gGX, gN1K, gN1X;

//calculate the hop count between src and estination
int dragonflynew_hopcnt(int src, int dest) 
{
  int hopcnt;
  int dest_grp_ID, src_grp_ID; 
  int src_hopcnt, dest_hopcnt;
  int src_intm, dest_intm;
  int grp_output, dest_grp_output;
  int grp_output_RID;

  int _grp_num_routers= gA;
  int _grp_num_nodes =_grp_num_routers*gP;
  
  dest_grp_ID = int(dest/_grp_num_nodes);
  src_grp_ID = int(src / _grp_num_nodes);
  
  //source and dest are in the same group, either 0-1 hop
  if (dest_grp_ID == src_grp_ID) {
    if ((int)(dest / gP) == (int)(src /gP))
      hopcnt = 0;
    else
      hopcnt = 1;
    
  } else {
    //source and dest are in the same group
    //find the number of hops in the source group
    //find the number of hops in the dest group
    if (src_grp_ID > dest_grp_ID)  {
      grp_output = dest_grp_ID;
      dest_grp_output = src_grp_ID - 1;
    }
    else {
      grp_output = dest_grp_ID - 1;
      dest_grp_output = src_grp_ID;
    }
    grp_output_RID = ((int) (grp_output / (gP))) + src_grp_ID * _grp_num_routers;
    src_intm = grp_output_RID * gP;

    grp_output_RID = ((int) (dest_grp_output / (gP))) + dest_grp_ID * _grp_num_routers;
    dest_intm = grp_output_RID * gP;

    //hop count in source group
    if ((int)( src_intm / gP) == (int)( src / gP ) )
      src_hopcnt = 0;
    else
      src_hopcnt = 1; 

    //hop count in destination group
    if ((int)( dest_intm / gP) == (int)( dest / gP ) ){
      dest_hopcnt = 0;
    }else{
      dest_hopcnt = 1;
    }

    //tally
    hopcnt = src_hopcnt + 1 + dest_hopcnt;
  }

  return hopcnt;  
}


//packet output port based on the source, destination and current location
int dragonfly_port(int rID, int source, int dest){
  int _grp_num_routers= gA;
  int _grp_num_nodes =_grp_num_routers*gP;

  int out_port = -1;
  int grp_ID = int(rID / _grp_num_routers); 
  int dest_grp_ID = int(dest/_grp_num_nodes);
  int grp_output=-1;
  int grp_RID=-1;
  
  //which router within this group the packet needs to go to
  if (dest_grp_ID == grp_ID) {
    grp_RID = int(dest / gP);
  } else {
    if (grp_ID > dest_grp_ID) {
      grp_output = dest_grp_ID;
    } else {
      grp_output = dest_grp_ID - 1;
    }
    grp_RID = int(grp_output /gP) + grp_ID * _grp_num_routers;
  }

  //At the last hop
  if (dest >= rID*gP && dest < (rID+1)*gP) {    
    out_port = dest%gP;
  } else if (grp_RID == rID) {
    //At the optical link
    out_port = gP + (gA-1) + grp_output %(gP);
  } else {
    //need to route within a group
    assert(grp_RID!=-1);

    if (rID < grp_RID){
      out_port = (grp_RID % _grp_num_routers) - 1 + gP;
    }else{
      out_port = (grp_RID % _grp_num_routers) + gP;
    }
  }  
 
  assert(out_port!=-1);
  return out_port;
}


DragonFlyNew::DragonFlyNew( const Configuration &config, const string & name ) :
  Network( config, name )
{

  _ComputeSize( config );
  _Alloc( );
  _BuildNet( config );
}

void DragonFlyNew::_ComputeSize( const Configuration &config )
{

  // LIMITATION
  //  -- only one dimension between the group
  // _n == # of dimensions within a group
  // _p == # of processors within a router
  // inter-group ports : _p
  // terminal ports : _p
  // intra-group ports : 2*_p - 1
  _p = config.GetInt( "k" );	// # of ports in each switch
  _n = config.GetInt( "n" );


  assert(_n==1);
  // dimension

  if (_n == 1)
    _k = _p + _p + 2*_p  - 1;
  else
    _k = _p + _p + 2*_p;

  
  // FIX...
  gK = _p; gN = _n;

  // with 1 dimension, total of 2p routers per group
  // N = 2p * p * (2p^2 + 1)
  // a = # of routers per group
  //   = 2p (if n = 1)
  //   = p^(n) (if n > 2)
  //  g = # of groups
  //    = a * p + 1
  // N = a * p * g;
  
  if (_n == 1)
    _a = 2 * _p;
  else
    _a = powi(_p, _n);

  _g = _a * _p + 1;
  _nodes   = _a * _p * _g;

  _num_of_switch = _nodes / _p;
  _channels = _num_of_switch * (_k - _p); 
  _size = _num_of_switch;


  
  gG = _g;
  gP = _p;
  gA = _a;
  _grp_num_routers = gA;
  _grp_num_nodes =_grp_num_routers*gP;

  // HANS: Additional variables
  gC = gP;
  gGK = _p;	// Global channel radix
  gPX = 1;	// Set to 1 processor multiple
  gGX = 1;	// Set to 1 global channel multiple
  gN1K = 2 * _p;// Dimension 1 radix
  gN1X = 1;	// Set to 1 global channel multiple
  // gBias = 0;	// Bias for PAR

  gIsDragonfly = true; // Indicate that we use dragonfly
}

void DragonFlyNew::_BuildNet( const Configuration &config )
{

  int _output=-1;
  int _input=-1;
  int _dim_ID=-1;
  int _num_ports_per_switch=-1;
  int c;

  ostringstream router_name;



  cout << " Dragonfly " << endl;
  cout << " p = " << _p << " n = " << _n << endl;
  cout << " each switch - total radix =  "<< _k << endl;
  cout << " # of switches = "<<  _num_of_switch << endl;
  cout << " # of channels = "<<  _channels << endl;
  cout << " # of nodes ( size of network ) = " << _nodes << endl;
  cout << " # of groups (_g) = " << _g << endl;
  cout << " # of routers per group (_a) = " << _a << endl;

  for ( int node = 0; node < _num_of_switch; ++node ) {
    // ID of the group
    int grp_ID;
    grp_ID = (int) (node/_a);
    router_name << "router";
    
    router_name << "_" <<  node ;

    _routers[node] = Router::NewRouter( config, this, router_name.str( ), 
					node, _k, _k );
    _timed_modules.push_back(_routers[node]);

    router_name.str("");

    for ( int cnt = 0; cnt < _p; ++cnt ) {
      c = _p * node +  cnt;
      _routers[node]->AddInputChannel( _inject[c], _inject_cred[c] );

    }

    for ( int cnt = 0; cnt < _p; ++cnt ) {
      c = _p * node +  cnt;
      _routers[node]->AddOutputChannel( _eject[c], _eject_cred[c] );

    }

    // add OUPUT channels
    // _k == # of processor per router
    //  need 2*_k routers  --thus, 
    //  2_k-1 outputs channels within group
    //  _k-1 outputs for intra-group

    //

    if (_n > 1 )  { cout << " ERROR: n>1 dimension NOT supported yet... " << endl; exit(-1); }

    //-------------------------------------ADDITIONAL--------------------------------
    // CAUTION: Only for 1D-Dragonfly
    // Allocate memory to keep the data for all the routers
    _routers[node]->_next_routers = new Router* [_num_of_switch];
    //----------------------------------END OF ADDITIONAL----------------------------

    //********************************************
    //   connect OUTPUT channels
    //********************************************
    // add intra-group output channel
    for ( int dim = 0; dim < _n; ++dim ) {
      for ( int cnt = 0; cnt < (2*_p -1); ++cnt ) {
	_output = (2*_p-1 + _p) * _n  * node + (2*_p-1) * dim  + cnt;

	_routers[node]->AddOutputChannel( _chan[_output], _chan_cred[_output] );

#ifdef DRAGON_LATENCY
	_chan[_output]->SetLatency(10);
	_chan_cred[_output]->SetLatency(10);
#endif
      }
    }

    // add inter-group output channel

    for ( int cnt = 0; cnt < _p; ++cnt ) {
      _output = (2*_p-1 + _p) * node + (2*_p - 1) + cnt;

      //      _chan[_output].global = true;
      _routers[node]->AddOutputChannel( _chan[_output], _chan_cred[_output] );
#ifdef DRAGON_LATENCY
      _chan[_output]->SetLatency(100);
      _chan_cred[_output]->SetLatency(100);
#endif
    }


    //********************************************
    //   connect INPUT channels
    //********************************************
    // # of non-local nodes 
    _num_ports_per_switch = (_k - _p);


    // intra-group GROUP channels
    for ( int dim = 0; dim < _n; ++dim ) {

      _dim_ID = ((int) (node / ( powi(_p, dim))));



      // NODE ID withing group
      _dim_ID = node % _a;




      for ( int cnt = 0; cnt < (2*_p-1); ++cnt ) {

	if ( cnt < _dim_ID)  {

	  _input = 	grp_ID  * _num_ports_per_switch * _a - 
	    (_dim_ID - cnt) *  _num_ports_per_switch + 
	    _dim_ID * _num_ports_per_switch + 
	    (_dim_ID - 1);
	}
	else {

	  _input =  grp_ID * _num_ports_per_switch * _a + 
	    _dim_ID * _num_ports_per_switch + 
	    (cnt - _dim_ID + 1) * _num_ports_per_switch + 
	    _dim_ID;
			
	}

	if (_input < 0) {
	  cout << " ERROR: _input less than zero " << endl;
	  exit(-1);
	}


	_routers[node]->AddInputChannel( _chan[_input], _chan_cred[_input] );
      }
    }


    // add INPUT channels -- "optical" channels connecting the groups
    int grp_output;

    for ( int cnt = 0; cnt < _p; ++cnt ) {
      //	   _dim_ID
      grp_output = _dim_ID* _p + cnt;

      if ( grp_ID > grp_output)   {

	_input = (grp_output) * _num_ports_per_switch * _a    +   		// starting point of group
	  (_num_ports_per_switch - _p) * (int) ((grp_ID - 1) / _p) +      // find the correct router within grp
	  (_num_ports_per_switch - _p) + 					// add offset within router
	  grp_ID - 1;	
      } else {

	_input = (grp_output + 1) * _num_ports_per_switch * _a    + 
	  (_num_ports_per_switch - _p) * (int) ((grp_ID) / _p) +      // find the correct router within grp
	  (_num_ports_per_switch - _p) +
	  grp_ID;	
      }

      _routers[node]->AddInputChannel( _chan[_input], _chan_cred[_input] );
    }

  }

  //-------------------------------------ADDITIONAL--------------------------------
  // CAUTION: For 1D-Dragonfly only
  // Provide information so that each router has instant access to all the routers in the network.
  for ( int node = 0; node < _num_of_switch; ++node ){
    for ( int cnt = 0; cnt < _num_of_switch; ++cnt ){
      _routers[node]->_next_routers[cnt] = _routers[cnt];
    }
  }
  //----------------------------------END OF ADDITIONAL----------------------------

  cout<<"Done links"<<endl;
}


int DragonFlyNew::GetN( ) const
{
  return _n;
}

int DragonFlyNew::GetK( ) const
{
  return _k;
}

void DragonFlyNew::InsertRandomFaults( const Configuration &config )
{
 
}

double DragonFlyNew::Capacity( ) const
{
  return (double)_k / 8.0;
}

void DragonFlyNew::RegisterRoutingFunctions(){

  gRoutingFunctionMap["min_dragonflynew"] = &min_dragonflynew;
  gRoutingFunctionMap["ugal_dragonflynew"] = &ugal_dragonflynew;
  gRoutingFunctionMap["valn_dragonflynew"] = &valn_dragonflynew;
  gRoutingFunctionMap["ugal_inflight_avg_dragonflynew"] = &ugal_inflight_avg_dragonflynew;
  gRoutingFunctionMap["par_inflight_avg_dragonflynew"] = &par_inflight_avg_dragonflynew;
#ifdef DGB_ON
  gRoutingFunctionMap["dgb_dragonflynew"] = &dgb_dragonflynew;
#endif
}


void min_dragonflynew( const Router *r, const Flit *f, int in_channel, 
		       OutputSet *outputs, bool inject )
{
  outputs->Clear( );

  if(inject) {
    int inject_vc= RandomInt(gNumVCs-1);
    outputs->AddRange(-1, inject_vc, inject_vc);
    return;
  }

  int _grp_num_routers= gA;

  int dest  = f->dest;
  int rID =  r->GetID(); 

  int grp_ID = int(rID / _grp_num_routers); 
  int debug = f->watch;
  int out_port = -1;
  int out_vc = 0;
  int dest_grp_ID=-1;

  //All minimal routing
  f->min = 1;

  if ( in_channel < gP ) {
    out_vc = 0;
    f->ph = 0;
    if (dest_grp_ID == grp_ID) {
      f->ph = 1;
    }
  } 


  out_port = dragonfly_port(rID, f->src, dest);

  //optical dateline
  if (out_port >=gP + (gA-1)) {
    f->ph = 1;
  }  
  
  out_vc = f->ph;
  if (debug)
    *gWatchOut << GetSimTime() << " | " << r->FullName() << " | "
	       << "	through output port : " << out_port 
	       << " out vc: " << out_vc << endl;
  outputs->AddRange( out_port, out_vc, out_vc );
}


//Basic adaptive routign algorithm for the dragonfly
void ugal_dragonflynew( const Router *r, const Flit *f, int in_channel,
			OutputSet *outputs, bool inject )
{
  outputs->Clear( );
  if(inject) {
    int inject_vc= RandomInt(gNumVCs-1);
    outputs->AddRange(-1, inject_vc, inject_vc);
    return;
  }

  //this constant biases the adaptive decision toward minimum routing
  //negative value would biases it towards nonminimum routing
  //int adaptive_threshold = 0;
  // /int bias = gBias;
  int bias = 0;

  int _router_num_nodes = gP;
  int _grp_num_routers= gA;
  int _grp_num_nodes =_grp_num_routers*gP;
  int _network_size =  gA * gP * gG;

  int src = f->src;
  int dest  = f->dest;
  int rID =  r->GetID();
  //int src_router = (int)(src / _router_num_nodes);
  //int dest_router = (int)(dest / _router_num_nodes);
  int grp_ID = (int) (rID / _grp_num_routers);
  int src_grp_ID = (int) (src / _grp_num_nodes);
  int dest_grp_ID = int(dest/_grp_num_nodes);

  int debug = f->watch;
  int out_port = -1;
  int out_vc = 0;
  int min_queue_size;
  int nonmin_queue_size;
  int min_hopcnt, nonmin_hopcnt;
  int intm_grp_ID;
  int intm_rID;

  if(debug){
    cout<<"At router "<<rID<<endl;
  }
  int min_router_output, nonmin_router_output;

  //at the source router, make the adaptive routing decision
  if ( in_channel < gP )   {
    //dest are in the same group, only use minimum routing
    if (dest_grp_ID == grp_ID) {
      f->ph  = 2;
      f->min = 1;
    } else {
      //select a random node
      //f->intm =RandomInt(_network_size - 1);
      f->intm = GetIntmNode(f->src, f->dest);
      intm_grp_ID = (int)(f->intm/_grp_num_nodes);
      if (debug){
	     cout<<"Intermediate node "<<f->intm<<" grp id "<<intm_grp_ID<<endl;
      }

      //random intermediate are in the same group, use minimum routing
      if(grp_ID == intm_grp_ID){
	      f->ph  = 1;
	      f->min = 1;
      } else {
	      //congestion metrics using queue length, obtained by GetUsedCredit()
	      min_router_output = dragonfly_port(rID, f->src, f->dest);
	      min_hopcnt = dragonflynew_hopcnt(f->src, f->dest);
      	min_queue_size = max(r->GetUsedCredit(min_router_output), 0) ;
        //min_queue_size = max(r->GetUsedCreditVC(min_router_output, 1), 0);

	      nonmin_router_output = dragonfly_port(rID, f->src, f->intm);
	      nonmin_hopcnt = dragonflynew_hopcnt(f->src, f->intm) + dragonflynew_hopcnt(f->intm, f->dest);
	      nonmin_queue_size = max(r->GetUsedCredit(nonmin_router_output), 0);
        //nonmin_queue_size = max(r->GetUsedCreditVC(nonmin_router_output, 0), 0);

        // Normalize queue count
        // if ((min_router_output >= (3*gC - 1)) && (nonmin_router_output < (3*gC - 1))){
        //   nonmin_queue_size = nonmin_queue_size * 8;
        // }

        // if ((min_router_output < (3*gC - 1)) && (nonmin_router_output >= (3*gC - 1))){
        //   min_queue_size = min_queue_size * 8;
        // }

	      //congestion comparison, could use hopcnt instead of 1 and 2
	      //if ((min_hopcnt * min_queue_size ) <= (nonmin_hopcnt * nonmin_queue_size)+adaptive_threshold ) {
        //if ((min_hopcnt * (min_queue_size + bias)) <= (nonmin_hopcnt * nonmin_queue_size)) {
        if ((min_hopcnt * min_queue_size) <= (nonmin_hopcnt * (nonmin_queue_size + bias))) {
	        if (debug)  cout << " MINIMAL routing " << endl;

          // FOR RL-VISUALIZATION: DELETE IF NOT NEEDED
          // if ((src_router == 0) && (dest_router == 8)){
          //   cout << GetSimTime() << ": MIN ROUTING at MinOut: " << min_router_output << " with: " << (min_hopcnt * min_queue_size) << " | " << (nonmin_hopcnt * nonmin_queue_size) << endl;
          // }

	        f->ph  = 1;
	        f->min = 1;
	      } else {
	        f->ph  = 0;
	        f->min = 0;

          // FOR RL-VISUALIZATION: DELETE IF NOT NEEDED
          // if ((src_router == 0) && (dest_router == 8)){
          //   cout << GetSimTime() << ": NON ROUTING at NonOut: " << nonmin_router_output << " with: " << (min_hopcnt * min_queue_size) << " | " << (nonmin_hopcnt * nonmin_queue_size) << endl;
          // }
	      }

        //if (rID == 0) // Looking inside group 0
          //cout << GetSimTime() << " | " << rID << " | " << dest_router << " | " << f->min << " | " << min_hopcnt << " | " << min_queue_size << " | " << nonmin_hopcnt << " | " << nonmin_queue_size << " | DECISION_UGAL" << endl;
      }
    }
  }

  //transition from nonminimal phase to minimal
  if(f->ph==0){
    intm_rID= (int)(f->intm/gP);
    if( rID == intm_rID){
      f->ph = 1;
    }
  }

  //port assignement based on the phase
  if(f->ph == 0){
    out_port = dragonfly_port(rID, f->src, f->intm);
  } else if(f->ph == 1){
    out_port = dragonfly_port(rID, f->src, f->dest);
  } else if(f->ph == 2){
    out_port = dragonfly_port(rID, f->src, f->dest);
  } else {
    assert(false);
  }

  //optical dateline
  //if (f->ph == 1 && out_port >=gP + (gA-1)) {
  if (f->ph == 1 && grp_ID == dest_grp_ID) { // Send to global port
    f->ph = 2;
  }

  // 3 VCS - vc assignemnt based on phase
  // assert(gNumVCs==3);
  // out_vc = f->ph;

  // 4 VCs
  assert(gNumVCs == 4);

  if (f->ph == 0){
    assert(f->min == 0);

    if (grp_ID == src_grp_ID){
      out_vc = 0;
    } else {
      assert(grp_ID == int(f->intm/_grp_num_nodes));
      out_vc = 1;
    }

  } else if (f->ph == 1){
    if (f->min == 1){
      assert(grp_ID == src_grp_ID);
      out_vc = 0; //0 (Good), 1 (Bad)
    } else {
      assert(f->min == 0);
      assert(grp_ID == int(f->intm/_grp_num_nodes));
      out_vc = 2;
    }
    
  } else {
    assert(f->ph == 2);
    assert(grp_ID == dest_grp_ID);

    out_vc = 3;
  }

  // 5 VCS
  /*
  assert(gNumVCs==5);

  if (f->ph == 0){
    assert(f->min == 0);

    if (grp_ID == src_grp_ID){
      out_vc = 0;
    } else {
      assert(grp_ID == int(f->intm/_grp_num_nodes));
      out_vc = 1;
    }

  } else if (f->ph == 1){
    if (f->min == 1){
      assert(grp_ID == src_grp_ID);
      out_vc = 3;
    } else {
      assert(f->min == 0);
      assert(grp_ID == int(f->intm/_grp_num_nodes));
      out_vc = 2;
    }
    
  } else {
    assert(f->ph == 2);
    assert(grp_ID == dest_grp_ID);
    out_vc = 4;
  }
  */

  outputs->AddRange( out_port, out_vc, out_vc );

}

void valn_dragonflynew( const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject )
{
  outputs->Clear();

  if(inject) {
    int inject_vc= RandomInt(gNumVCs-1);
    outputs->AddRange(-1, inject_vc, inject_vc);
    return;
  }

  int rID = r->GetID();

  int out_port = -1;
  int out_vc = -1;

  if(in_channel < gP){
    f->intm = RandomInt((gA * gP * (gA*gGK+1))-1);
    f->ph = 1;

    int grp_num_nodes = gA * gP;

    int src_grp = f->src / grp_num_nodes;
    int intm_grp = f->intm / grp_num_nodes;
    int dest_grp = f->dest / grp_num_nodes;

    if ((src_grp == intm_grp) || (intm_grp == dest_grp)){
      f->min = 1;
    } else {
      f->min = 0;
    }

    f->lat_start = GetSimTime();
  }

  if(f->ph == 1 && IsTheRouter(rID, f->intm/gP)) f->ph = 2;

  if(f->ph == 1) out_port = GetRoutingPort(rID, f->intm, &out_vc, 0, 1);
  else if(f->ph == 2) out_port = GetRoutingPort(rID, f->dest, &out_vc, 2, 3);

  assert(out_vc != -1 && out_port != -1); // Check error
  outputs->AddRange(out_port, out_vc, out_vc);
}

void ugal_inflight_avg_dragonflynew( const Router *r, const Flit *f, int in_channel,
      OutputSet *outputs, bool inject )
{
  //need 3 VCs for deadlock freedom
  //assert(gNumVCs==3);

  outputs->Clear( );
  if(inject) {
    int inject_vc= RandomInt(gNumVCs-1);
    outputs->AddRange(-1, inject_vc, inject_vc);
    return;
  }

  //this constant biases the adaptive decision toward minimum routing
  //negative value woudl biases it towards nonminimum routing
  //int adaptive_threshold = 0;
  int bias = 0;

  int _router_num_nodes = gP;
  int _grp_num_routers= gA;
  int _grp_num_nodes =_grp_num_routers*gP;
  int _network_size =  gA * gP * gG;

  int src = f->src;
  int dest  = f->dest;
  int rID =  r->GetID();
  int src_router = (int)(src / _router_num_nodes);
  int dest_router = (int)(dest / _router_num_nodes);
  int grp_ID = (int) (rID / _grp_num_routers);
  int src_grp_ID = int(src / _grp_num_nodes);
  int dest_grp_ID = int(dest / _grp_num_nodes);

  int debug = f->watch;
  int out_port = -1;
  int out_vc = 0;
  int min_queue_size;
  int nonmin_queue_size;
  int min_hopcnt, nonmin_hopcnt;
  int intm_grp_ID;
  int intm_rID;

  if(debug){
    cout<<"At router "<<rID<<endl;
  }
  int min_router_output, nonmin_router_output;

  //at the source router, make the adaptive routing decision
  if ( in_channel < gP )   {
    //dest are in the same group, only use minimum routing
    if (dest_grp_ID == grp_ID) {
      f->ph  = 2;
      f->min = 1;
    } else {
      //select a random node
      f->intm =RandomInt(_network_size - 1);
      intm_grp_ID = (int)(f->intm/_grp_num_nodes);
      if (debug){
       cout<<"Intermediate node "<<f->intm<<" grp id "<<intm_grp_ID<<endl;
      }

      //random intermediate are in the same group, use minimum routing
      if(grp_ID == intm_grp_ID){
        f->ph  = 1;
        f->min = 1;
      } else {
        //congestion metrics using queue length, obtained by GetUsedCredit()
        min_router_output = dragonfly_port(rID, f->src, f->dest);
        min_hopcnt = dragonflynew_hopcnt(f->src, f->dest);
        min_queue_size = max(r->GetUsedCredit(min_router_output), 0);
        int min_inflight = r->GetInFlight(min_router_output);

        nonmin_router_output = dragonfly_port(rID, f->src, f->intm);
        nonmin_hopcnt = dragonflynew_hopcnt(f->src, f->intm) + dragonflynew_hopcnt(f->intm, f->dest);
        nonmin_queue_size = r->GetUsedCreditAvg(min_router_output);
        int nonmin_inflight = r->GetInFlightAvg(min_router_output);

        int min_noinflight = min_queue_size - min_inflight;
        int non_noinflight = nonmin_queue_size - nonmin_inflight;

        if (min_noinflight < 0)   min_noinflight = 0;
        if (non_noinflight < 0)   non_noinflight = 0;

        // Normalize queue count
        if ((min_router_output >= (3*gC - 1)) && (nonmin_router_output < (3*gC - 1))){
          non_noinflight = non_noinflight * 8;
        }

        if ((min_router_output < (3*gC - 1)) && (nonmin_router_output >= (3*gC - 1))){
          min_noinflight = min_noinflight * 8;
        }

        //congestion comparison, could use hopcnt instead of 1 and 2
        if ((min_hopcnt * (min_noinflight)) <= (nonmin_hopcnt * ((non_noinflight) + 1))) {
        //if ((min_hopcnt * (min_noinflight)) <= (nonmin_hopcnt * (non_noinflight))) {
        //if ((min_hopcnt * min_queue_size) <= (nonmin_hopcnt * (nonmin_queue_size + 1))) {
        
          if (debug)  cout << " MINIMAL routing " << endl;

          f->ph  = 1;
          f->min = 1;
        } else {
          f->ph  = 0;
          f->min = 0;

        }
      }
    }
  }

  //transition from nonminimal phase to minimal
  if(f->ph==0){
    intm_rID= (int)(f->intm/gP);
    if( rID == intm_rID){
      f->ph = 1;
    }
  }

  //port assignement based on the phase
  if(f->ph == 0){
    out_port = dragonfly_port(rID, f->src, f->intm);
  } else if(f->ph == 1){
    out_port = dragonfly_port(rID, f->src, f->dest);
  } else if(f->ph == 2){
    out_port = dragonfly_port(rID, f->src, f->dest);
  } else {
    assert(false);
  }

  //optical dateline
  //if (f->ph == 1 && out_port >=gP + (gA-1)) {
  if (f->ph == 1 && grp_ID == dest_grp_ID) { // Send to global port
    f->ph = 2;
  }

  //3VCs: vc assignemnt based on phase
  // assert(gNumVCs == 3);
  // out_vc = f->ph;

  // 4 VCs
  assert(gNumVCs == 4);

  if (f->ph == 0){
    assert(f->min == 0);

    if (grp_ID == src_grp_ID){
      out_vc = 0;
    } else {
      assert(grp_ID == int(f->intm/_grp_num_nodes));
      out_vc = 1;
    }

  } else if (f->ph == 1){
    if (f->min == 1){
      assert(grp_ID == src_grp_ID);
      out_vc = 0; //0 (Good), 1 (Bad)
    } else {
      assert(f->min == 0);
      assert(grp_ID == int(f->intm/_grp_num_nodes));
      out_vc = 2;
    }
    
  } else {
    assert(f->ph == 2);
    assert(grp_ID == dest_grp_ID);

    out_vc = 3;
  }

  outputs->AddRange( out_port, out_vc, out_vc );
}

void par_inflight_avg_dragonflynew( const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject )
{
 	outputs->Clear( );

	if(inject) {
   	  int inject_vc= RandomInt(gNumVCs-1);
    	  outputs->AddRange(-1, inject_vc, inject_vc);
	  return;
  	}

	int rID = r->GetID();

	int _grp_num_nodes = gA * gP; //gRouterPerGroup
	//int _network_size = gA * gP * gG;

	int src_grp_ID = f->src/_grp_num_nodes;
	//int intm_grp_ID;
	int dest_grp_ID = f->dest/_grp_num_nodes;

	int out_port = -1;
	int out_vc = -1;

	if(in_channel >= gP && IsInGroup(rID, src_grp_ID) == true && IsInGroup(rID, dest_grp_ID) == false && f->min == 1 && f->ph == 2) f->ph = 3;
	if(in_channel < gP || f->ph == 3){
		if(f->ph != 3){
			if(src_grp_ID == dest_grp_ID) f->intm = (src_grp_ID * gA * gP) + RandomInt((gA * gP) - 1);
			else f->intm = RandomInt((gA * gP * (gA*gGK+1))-1);
			f->min = -1;
		}
		//intm_grp_ID = f->intm/gP/gA;

		int min_global=0, non_global=0;
		int min_port = GetRoutingPort(rID, f->dest, &min_global),
				non_port = GetRoutingPort(rID, f->intm, &non_global);

		int min_hop = HopCount(rID, f->dest/gP),
				non_hop = HopCount(rID, f->intm/gP) + HopCount(f->intm/gP, f->dest/gP);

		int min_queue = r->GetUsedCredit(min_port);
		int non_queue = r->GetUsedCreditAvg(min_port);

    int min_inflight = r->GetInFlight(min_port);
    int nonmin_inflight = r->GetInFlightAvg(min_port);
    //int nonmin_inflight = r->GetInFlight(non_port);

    int min_noinflight = min_queue - min_inflight;
    int non_noinflight = non_queue - nonmin_inflight;

    if (min_noinflight < 0) min_noinflight = 0;
    if (non_noinflight < 0) non_noinflight = 0;

    // Normalize queue count
    // if ((min_port >= (3*gC - 1)) && (non_port < (3*gC - 1))){
    //   non_noinflight = non_noinflight * 8;
    // }

    // if ((min_port < (3*gC - 1)) && (non_port >= (3*gC - 1))){
    //   min_noinflight = min_noinflight * 8;
    // }

		if(min_hop * (min_noinflight) <= non_hop * (non_noinflight + 1)){
		//if(min_hop * (min_noinflight) <= non_hop * (non_noinflight)){

			f->ph = 2;
			if(f->min != 1){
				f->min = 1;
				//if(gWarmUpComplete == 1) gMinCount++;
			}
		} else {
			if(f->min == 1){
				//if(gWarmUpComplete == 1) gMinCount--;
			}
			f->min = 0;
			f->ph = 1;
			//if(gWarmUpComplete == 1) gValCount++;
		}
	}

	if(f->ph == 1 && IsTheRouter(rID, f->intm/gP)) f->ph = 2;

  // DEFAULT VC CONFIGURATION: 5 VCs
  /*
  assert(gNumVCs==5);

	if(f->ph == 1) out_port = GetRoutingPort(rID, f->intm, &out_vc, 1, 2);
	else if(f->ph == 2){
		int isGlobal;
		out_port = GetRoutingPort(rID, f->dest, &out_vc, 3, 4, &isGlobal);

		if( IsInGroup(rID, src_grp_ID) == true && isGlobal == 0) out_vc = 0;
	}
  */

  // ALTERNATIVE VC CONFIGURATION: 4 VCs
  assert(gNumVCs==4);

  if(f->ph == 1) out_port = GetRoutingPort(rID, f->intm, &out_vc, 0, 1);
	else if(f->ph == 2){
		int isGlobal;
		out_port = GetRoutingPort(rID, f->dest, &out_vc, 2, 3, &isGlobal);

		if( IsInGroup(rID, src_grp_ID) == true && isGlobal == 0) out_vc = 0; // 0
	}

	assert(out_vc != -1 && out_port != -1); // Check error
	outputs->AddRange(out_port, out_vc, out_vc);
	outputs->AddRange(0, 0, 0);
}

#ifdef DGB_ON
void dgb_dragonflynew( const Router *r, const Flit *f, int in_channel,
			OutputSet *outputs, bool inject )
{
  // Useful variables
  int src_router = f->src / gC;
  int dest_router = f->dest / gC;
  int intm_router;

  //need 5 VCs for deadlock freedom with VALn

  //assert(gNumVCs==5);
  outputs->Clear( );
  if(inject) {
    int inject_vc= RandomInt(gNumVCs-1);
    outputs->AddRange(-1, inject_vc, inject_vc);
    return;
  }

  //this constant biases the adaptive decision toward minimum routing
  //negative value would biases it towards nonminimum routing
  //int adaptive_threshold = 0;
  int _router_num_nodes = gP;
  int _grp_num_routers= gA;
  int _grp_num_nodes =_grp_num_routers*gP;
  int _network_size =  gA * gP * gG;

  int src = f->src;
  int dest  = f->dest;
  f->tail_flit->dest = dest;
  int rID =  r->GetID();
  //int src_router = (int)(src / _router_num_nodes);
  //int dest_router = (int)(dest / _router_num_nodes);
  int grp_ID = (int) (rID / _grp_num_routers);
  int src_grp_ID = (int) (src / _grp_num_nodes);
  int dest_grp_ID = int(dest/_grp_num_nodes);

  int debug = f->watch;
  int out_port = -1;
  int out_vc = 0;
  int min_queue_size;
  int nonmin_queue_size;
  int min_hopcnt, nonmin_hopcnt;
  int min_inflight, nonmin_inflight;
  int intm_grp_ID;
  int intm_rID;

  int vcBegin, vcEnd;

  if(debug){
    cout<<"***** Flit: " << f->id << " at router "<< rID << " at time " << GetSimTime() << " *****" << endl;
  }
  int min_router_output, nonmin_router_output;

  //at the source router, make the adaptive routing decision
  if ( in_channel < gP )   {
    f->lat_start = GetSimTime();
    f->wire_total = 0;

    f->tail_flit->lat_start = GetSimTime();
    f->tail_flit->wire_total = 0;

    //dest are in the same group, only use minimum routing
    if (dest_grp_ID == grp_ID) {
      f->ph  = 2;
      f->min = 1;
      f->tail_flit->min = 1;
      f->force_min = true;
      
    } else {
      //select a random node
      //f->intm = RandomInt(_network_size - 1);
      f->intm = GetIntmNode(f->src, f->dest);
      f->tail_flit->intm = f->intm;
      intm_router = f->intm / gC;

      intm_grp_ID = (int)(f->intm/_grp_num_nodes);
      if (debug){
	     cout<<"Intermediate node "<<f->intm<<" grp id "<<intm_grp_ID<<endl;
      }

      // Should we put it here?
      f->dgb_train = true;
      f->tail_flit->dgb_train = true;

      //random intermediate are in the same group, use minimum routing
      if (grp_ID == intm_grp_ID){
	      f->ph  = 1;
	      f->min = 1;
        f->force_min = true;
        f->tail_flit->min = 1;
        f->tail_flit->force_min = true;

        f->h_min = dragonflynew_hopcnt(f->src, f->dest);
	      min_router_output = dragonfly_port(rID, f->src, f->dest);
        f->q_min = max(r->GetUsedCredit(min_router_output), 0) - r->GetInFlight(min_router_output);
        f->min_port = min_router_output - gC;
        f->min_global_port = GetGroupOutPort(src_router, dest_router);

        f->tail_flit->h_min = f->h_min;
        f->tail_flit->q_min = f->q_min;
        f->tail_flit->min_port = f->min_port;
        f->tail_flit->min_global_port = f->min_global_port;

        int min_out_router = GetGroupOutRouter(src_grp_ID, dest_grp_ID);
        int min_out_router_port = dragonfly_port(min_out_router, f->src, f->dest);
        int global_q_min = r->_next_routers[min_out_router]->GetUsedCredit(min_out_router_port);
        int global_q_noinflight_min = global_q_min - r->_next_routers[min_out_router]->GetInFlight(min_out_router_port);
        f->q_min_global = global_q_noinflight_min;
        f->tail_flit->q_min_global = global_q_noinflight_min;

        if (min_router_output >= (3 * gC - 1))
          assert(f->q_min == f->q_min_global);

      } else {

        if (intm_grp_ID == dest_grp_ID){
          f->force_min = true;
          f->tail_flit->force_min = true;
        }

	      //congestion metrics using queue length, obtained by GetUsedCredit()
	      min_router_output = dragonfly_port(rID, f->src, f->dest);
	      min_hopcnt = dragonflynew_hopcnt(f->src, f->dest);
      	min_queue_size = max(r->GetUsedCredit(min_router_output), 0) ;
        min_inflight = r->GetInFlight(min_router_output);
        //min_queue_size = max(r->GetUsedCreditVC(min_router_output, 1), 0);

	      nonmin_router_output = dragonfly_port(rID, f->src, f->intm);
	      nonmin_hopcnt = dragonflynew_hopcnt(f->src, f->intm) + dragonflynew_hopcnt(f->intm, f->dest);
	      nonmin_queue_size = max(r->GetUsedCredit(nonmin_router_output), 0);
        nonmin_inflight = r->GetInFlight(nonmin_router_output);
        //nonmin_queue_size = max(r->GetUsedCreditVC(nonmin_router_output, 0), 0);

        // Record values
        f->min_port = min_router_output - gC;
        f->non_port = nonmin_router_output - gC;
        f->min_global_port = GetGroupOutPort(r->GetID(), dest_router);
        f->non_global_port = GetGroupOutPort(r->GetID(), intm_router);

        f->tail_flit->min_port = f->min_port;
        f->tail_flit->non_port = f->non_port;
        f->tail_flit->min_global_port = f->min_global_port;
        f->tail_flit->non_global_port = f->non_global_port;

        f->h_min = min_hopcnt;
        f->h_non = nonmin_hopcnt;
        f->q_min = min_queue_size - min_inflight;
        //f->q_min = min_queue_size;
        f->q_non = nonmin_queue_size - nonmin_inflight;
        //f->q_non = nonmin_queue_size;
        //f->q_non = r->GetUsedCreditAvg(-1) - r->GetInFlightAvg(-1);

        f->tail_flit->h_min = f->h_min;
        f->tail_flit->h_non = f->h_non;
        f->tail_flit->q_min = f->q_min;
        f->tail_flit->q_non = f->q_non;

        // f->q_min = r->GetUsedCreditVC(min_router_output, 3) - r->GetInFlightVC(min_router_output, 3);
        // f->q_non = r->GetUsedCreditVC(nonmin_router_output, 0) - r->GetInFlightVC(nonmin_router_output, 0);

        // f->hq_min = min_hopcnt * min_queue_size;
        // f->hq_non = nonmin_hopcnt * nonmin_queue_size;

        // f->tail_flit->hq_min = f->hq_min;
        // f->tail_flit->hq_non = f->hq_non;

        // Get bias from DGB
        //float max_explore_bound = ((float)min_hopcnt / (float)nonmin_hopcnt) * min_queue_size - nonmin_queue_size;

        // Get queue at next hop router (ONLY for ideal condition!)
        int min_out_router = GetGroupOutRouter(src_grp_ID, dest_grp_ID);
        int min_out_router_port = dragonfly_port(min_out_router, f->src, f->dest);
        int global_q_min = r->_next_routers[min_out_router]->GetUsedCredit(min_out_router_port);
        int global_q_noinflight_min = global_q_min - r->_next_routers[min_out_router]->GetInFlight(min_out_router_port);
        f->q_min_global = global_q_noinflight_min;
        f->tail_flit->q_min_global = global_q_noinflight_min;

        int non_out_router = GetGroupOutRouter(src_grp_ID, intm_grp_ID);
        int non_out_router_port = dragonfly_port(non_out_router, f->src, f->intm);
        int global_q_non = r->_next_routers[non_out_router]->GetUsedCredit(non_out_router_port);
        int global_q_noinflight_non = global_q_non - r->_next_routers[non_out_router]->GetInFlight(non_out_router_port);
        f->q_non_global = global_q_noinflight_non;
        f->tail_flit->q_non_global = global_q_noinflight_non;

        float bias;
        if (f->non_port >= 0){ // The intermediate node is not located at the source router
          assert(f->min_port >= 0);
          bias = r->dgb->GetBias(f->min_port, f->non_port, f->min_global_port, f->non_global_port, min_hopcnt, nonmin_hopcnt, min_queue_size, nonmin_queue_size, f->q_min, f->q_non, dest_router, GetChanDiff(f->src, f->intm, f->dest), global_q_min, global_q_non, global_q_noinflight_min, global_q_noinflight_non);
          // f->bias = bias;
          // f->tail_flit->bias = bias;

        } else {
          // f->bias = 0;
          // f->tail_flit->bias = 0;
          bias = 0.0;
        }

        // Normalize queue count
        if ((min_router_output >= (3*gC - 1)) && (nonmin_router_output < (3*gC - 1))){
          nonmin_queue_size = nonmin_queue_size * 8;
        }

        if ((min_router_output < (3*gC - 1)) && (nonmin_router_output >= (3*gC - 1))){
          min_queue_size = min_queue_size * 8;
        }

        int min_hq = min_hopcnt * min_queue_size;
        int non_hq = nonmin_hopcnt * (nonmin_queue_size + bias);

        if (non_hq < 0) non_hq = 0;

	      //congestion comparison, could use hopcnt instead of 1 and 2
        //if ((min_hq <= non_hq) || (intm_router == dest_router)){
        if (min_hq <= non_hq) {
	        if (debug)  cout << " MINIMAL routing " << endl;

	        f->ph  = 1;
	        f->min = 1;
          f->tail_flit->min = 1;

          // if (min_router_output < (3 * gC - 1)){
          //   f->q_total_srcgrp = f->q_min + f->q_min_global;
          // } else {
          //   assert(f->q_min == f->q_min_global);
          //   f->q_total_srcgrp = f->q_min;
          // }

          if (min_router_output >= (3 * gC - 1))  assert(f->q_min == f->q_min_global);

          // if ((rID == 4) && (dest_router == 10) && (!f->force_min))
          // if (!f->force_min)
          //   cout << GetSimTime() << " - ROUTE MIN DGB - fID: " << f->id << " | SrcRouter: " << r->GetID() << " | DestRouter: " << dest_router << " | MinPort: " << min_router_output << " | NonPort: " << nonmin_router_output << " | Hmin: " << min_hopcnt << " | Qmin: " << min_queue_size << " | QminNet: " << f->q_min << " | Hnon: " << nonmin_hopcnt << " | Qnon: " << nonmin_queue_size << " | QnonNet: " << f->q_non << " | Bias: " << bias << " | BanditTrain: " << f->bandit_train << " | ForceMin: " << f->force_min << endl;

          // HANS: REGISTER ESTIMATED LATENCY
          /*
          unsigned int min_size;
          float diff = r->dgb->GetMyDiffMin(dest_router, &min_size);
          int latency;

          if (min_size > 0){
#ifdef DGB_PIGGYBACK
            int global_queue = r->dgb->GetGlobalQNet(f->min_global_port);
            if (global_queue < 0)   global_queue = f->q_min;

            latency = f->q_min + global_queue + diff + f->h_min;
#else
            latency = f->q_min + f->q_min_global + diff + f->h_min;
#endif
          
          } else {
            assert(min_size == 0);

#ifdef DGB_PIGGYBACK
            int global_queue = r->dgb->GetGlobalQNet(f->min_global_port);
            if (global_queue < 0)   global_queue = f->q_min;

            latency = f->q_min + (f->h_min - 1) * global_queue;
#else
            latency = f->h_min * f->q_min;
#endif
          }

          if (latency < f->h_min)   latency = f->h_min;
          r->dgb->RegisterMinLocalLatency(dest_router, latency);

          // HANS: SPECIAL CASE OF LATENCY REGISTRATION
          if (f->min_port == f->non_port){
            unsigned int non_size_sameport;
            int latency_sameport;

            float diff_sameport = r->dgb->GetMyDiffNon(dest_router, f->non_port, &non_size_sameport);

            if (non_size_sameport > 0){
#ifdef DGB_PIGGYBACK
              int global_queue = r->dgb->GetGlobalQNet(f->non_global_port);
              if (global_queue < 0)   global_queue = f->q_non;

              latency_sameport = f->q_non + global_queue + diff_sameport + f->h_non;
#else
              latency_sameport = f->q_non + f->q_non_global + diff_sameport + f->h_non;
#endif

              if (latency_sameport < f->h_non)    latency_sameport = f->h_non;
              r->dgb->RegisterNonLocalLatency(dest_router, f->non_port, latency_sameport);
            }
          }
          */

	      } else {
	        f->ph  = 0;
	        f->min = 0;
          f->tail_flit->min = 0;

          // if (nonmin_router_output < (3 * gC - 1)){
          //   f->q_total_srcgrp = f->q_non + f->q_non_global;
          // } else {
          //   assert(f->q_non == f->q_non_global);
          //   f->q_total_srcgrp = f->q_non;
          // }

          if (nonmin_router_output >= (3 * gC - 1))  assert(f->q_non == f->q_non_global);

          // if ((rID == 4) && (dest_router == 10) && (nonmin_router_output == 13))
            // cout << GetSimTime() << " - ROUTE NON DGB - fID: " << f->id << " | SrcRouter: " << r->GetID() << " | DestRouter: " << dest_router << " | MinPort: " << min_router_output << " | NonPort: " << nonmin_router_output << " | Hmin: " << min_hopcnt << " | Qmin: " << min_queue_size << " | QminNet: " << f->q_min << " | Hnon: " << nonmin_hopcnt << " | Qnon: " << nonmin_queue_size << " | QnonNet: " << f->q_non << " | Bias: " << bias << " | DGBTrain: " << f->dgb_train << endl;

          /*
          // REGISTER ESTIMATED LATENCY
          unsigned int non_size;
          float diff = r->dgb->GetMyDiffNon(dest_router, f->non_port, &non_size);
          int latency;

          if (non_size > 0){
#ifdef DGB_PIGGYBACK
            int global_queue = r->dgb->GetGlobalQNet(f->non_global_port);
            if (global_queue < 0)   global_queue = f->q_non;

            latency = f->q_non + global_queue + diff + f->h_non;
#else
            latency = f->q_non + f->q_non_global + diff + f->h_non;
#endif

          } else {
            assert(non_size == 0);

#ifdef DGB_PIGGYBACK
            int global_queue = r->dgb->GetGlobalQNet(f->non_global_port);
            if (global_queue < 0)   global_queue = f->q_non;

            latency = f->q_non + (f->h_non - 1) * global_queue;
#else
            latency = f->h_non * f->q_non;
#endif
          }

          if (latency < f->h_non)   latency = f->h_non;
          r->dgb->RegisterNonLocalLatency(dest_router, f->non_port, latency);

          // HANS: SPECIAL CASE OF LATENCY REGISTRATION
          if (f->min_port == f->non_port){
            unsigned int min_size_sameport;
            int latency_sameport;

            float diff_sameport = r->dgb->GetMyDiffMin(dest_router, &min_size_sameport);

            if (min_size_sameport > 0){
#ifdef DGB_PIGGYBACK
              int global_queue = r->dgb->GetGlobalQNet(f->min_global_port);
              if (global_queue < 0)   global_queue = f->q_min;

              latency_sameport = f->q_min + global_queue + diff_sameport + f->h_min;
#else
              latency_sameport = f->q_min + f->q_min_global + diff_sameport + f->h_min;
#endif

              if (latency_sameport < f->h_min)    latency_sameport = f->h_min;
              r->dgb->RegisterMinLocalLatency(dest_router, latency_sameport);
            }
          }
          */

	      }

        // FOR DEBUGGING: Print all net queues
        // if ((rID == 41) && (dest_router == 64)){
        //   for (int iter = gC; iter < 15; iter++){
        //     cout << r->GetUsedCredit(iter) - r->GetInFlight(iter) << " | ";
        //   }
        //   cout << endl;
        // }

        //if (rID == 0) // Looking inside group 0
          //cout << GetSimTime() << " | " << rID << " | " << dest_router << " | " << f->min << " | " << min_hopcnt << " | " << min_queue_size << " | " << nonmin_hopcnt << " | " << nonmin_queue_size << " | DECISION_UGAL" << endl;
      }
    }
  }

  // Record time when arriving at RC stage of the 2nd router
  if (f->hops == 1){
    assert(r->GetID() != (f->src / gC));
    f->until_2ndrouter = GetSimTime() - f->lat_start - f->wire_total;
    f->tail_flit->until_2ndrouter = GetSimTime() - f->lat_start - f->wire_total;
  }

  // Record queue count of the NON path
  if ((f->hops == 1) && (!f->force_min)){ // At 2nd hop router
    assert(r->GetID() != (f->src / gC));

    int temp_port;
    int netto;

    if (f->min == 1){
      temp_port = dragonfly_port(r->GetID(), f->src, f->intm);
    } else {
      assert(f->min == 0);
      temp_port = dragonfly_port(r->GetID(), f->src, f->dest);
    }

    netto = r->GetUsedCredit(temp_port) - r->GetInFlight(temp_port);
    assert(netto >= 0);
    f->q_count_at_2ndrouter = netto;
    f->tail_flit->q_count_at_2ndrouter = netto;
  }

  // Record global queue occupancy

  // Record RC time at every hop
  // f->rc_start = GetSimTime();

  //transition from nonminimal phase to minimal
  if(f->ph==0){
    intm_rID= (int)(f->intm/gP);

    if (rID == intm_rID){
      f->ph = 1;
    }
  }

  //port assignment based on the phase
  if(f->ph == 0){
    out_port = dragonfly_port(rID, f->src, f->intm);
  } else if(f->ph == 1){
    out_port = dragonfly_port(rID, f->src, f->dest);
  } else if(f->ph == 2){
    out_port = dragonfly_port(rID, f->src, f->dest);
  } else {
    assert(false);
  }

  // Record accumulated queue count at the source group
  if (grp_ID == src_grp_ID){
    f->q_total_srcgrp += r->GetUsedCredit(out_port) - r->GetInFlight(out_port);
    f->tail_flit->q_total_srcgrp = f->q_total_srcgrp;
  }

#ifdef DGB_PIGGYBACK
  // If the packet is going to the same group, carry global queue info
  if ((out_port >= gC) && (out_port < (3*gK - 1))){
    int idxmod = r->GetID() % gA;

    f->carry_qlobalq = true;

    for (int iter = 0; iter < gK; iter++){
      f->globalq.push(make_pair(idxmod * gK + iter, r->GetUsedCredit(3 * gK - 1 + iter)));
      f->globalq_net.push(make_pair(idxmod * gK + iter, r->GetUsedCredit(3 * gK - 1 + iter) - r->GetInFlight(3 * gK - 1 + iter)));
      //f->globalq.push(make_pair(idxmod * gK + iter, r->GetUsedCredit(3 * gK - 1 + iter)));
    }
  }
#endif

  // HANS: For debugging
  // if ((r->GetID() == 214) && (out_port == 12)){
  //   cout << GetSimTime() << " - TRANSIT - fID: " << f->id << " | SrcRouter: " << f->src / gC << " | DestRouter: " << f->dest / gC << " | Min: " << f->min << endl;
  // }

  // Record queue count at next hop routers
  // if ((r->GetID() != (f->src / gC)) && (out_port >= gC)){ // At next-hop router
  //   assert(r->GetID() != (f->dest / gC));

  //   int netto = r->GetUsedCredit(out_port) - r->GetInFlight(out_port);
  //   assert(netto >= 0);
  //   f->q_count_from_2ndrouter += netto;
  // }

  //optical dateline
  //if (f->ph == 1 && out_port >=gP + (gA-1)) {
  if (f->ph == 1 && grp_ID == dest_grp_ID) { // Send to global port
    f->ph = 2;
  }

  // VC ASSIGNMENT FOR BANDIT
  //3 VCs (VC assignment based on the phase)
  //out_vc = f->ph;

  // 4 VCs
  assert(gNumVCs == 4);

  if (f->ph == 0){
    assert(f->min == 0);

    if (grp_ID == src_grp_ID){
      vcBegin = 0;
      vcEnd = 0;
    } else {
      assert(grp_ID == int(f->intm/_grp_num_nodes));
      vcBegin = 1;
      vcEnd = 1;
    }

  } else if (f->ph == 1){
    if (f->min == 1){
      assert(grp_ID == src_grp_ID);
      vcBegin = 0; //0 (Good), 1 (Bad)
      vcEnd = 0; //0 (Good), 1 (Bad)
    } else {
      assert(f->min == 0);
      assert(grp_ID == int(f->intm/_grp_num_nodes));
      vcBegin = 2;
      vcEnd = 2;
    }
    
  } else {
    assert(f->ph == 2);
    assert(grp_ID == dest_grp_ID);

    vcBegin = 3;
    vcEnd = 3;
  }

  // 5 VCS
  /*
  assert(gNumVCs == 5);

  if (f->ph == 0){
    assert(f->min == 0);

    if (grp_ID == src_grp_ID){
      vcBegin = 0;
      vcEnd = 0;
    } else {
      assert(grp_ID == int(f->intm/_grp_num_nodes));
      vcBegin = 1;
      vcEnd = 1;
    }

  } else if (f->ph == 1){
    if (f->min == 1){
      assert(grp_ID == src_grp_ID);
      vcBegin = 3;
      vcEnd = 3;
    } else {
      assert(f->min == 0);
      assert(grp_ID == int(f->intm/_grp_num_nodes));
      vcBegin = 2;
      vcEnd = 2;
    }
    
  } else {
    assert(f->ph == 2);
    assert(grp_ID == dest_grp_ID);

    // 5 VCs
    vcBegin = 4;
    vcEnd = 4;

    // 6 VCs
    // if (f->min == 1){
    //   out_vc = 4;
    // } else {
    //   assert(f->min == 0);
    //   out_vc = 5;
    // }
  }
  */

  outputs->AddRange( out_port, vcBegin, vcEnd );
}
#endif

bool IsInGroup(int rID, int grp_ID){
  if((rID/gA) == grp_ID) return true;
  else return false;
}

bool IsTheRouter(int rID, int dest_rID){
  if(rID == dest_rID) return true;
  else return false;
}

int GetLocalPort(int rID, int dest_rID){/*{{{*/
  if(gN == 1){
  	if(rID < dest_rID) return gP*gPX + ((dest_rID % gA) - 1) * gN1X;
  	else if(rID > dest_rID) return gP*gPX + (dest_rID % gA) * gN1X;
  	else if(rID == dest_rID) return 0;
  	else{ cout<<"[Error] in GetLocalPort()"<<endl; exit(-1); }
  }/*else if(gN == 2){
  	int dim1_ID = rID % gN1K;
  	int dim2_ID = (rID % gRouterPerGroup) / gN1K;
  	int dest_dim1_ID = dest_rID % gN1K;
  	int dest_dim2_ID = (dest_rID % gRouterPerGroup) / gN1K;

  	if(dim1_ID == dest_dim1_ID){
    		if(dim2_ID < dest_dim2_ID) return gP*gPX + (gN1K-1)*gN1X + (dest_dim2_ID - 1)*gN2X;
  		else if(dim2_ID < dest_dim2_ID) return gP*gPX + (gN1K-1)*gN1X + dest_dim2_ID*gN2X;
  		else if(dim2_ID == dest_dim2_ID) return 0;
  		else{ cout<<"[Error] in GetLocalPort()"<<endl; exit(-1); }
  	}else{
  		if(dim1_ID < dest_dim1_ID) return gP*gPX + (dest_dim1_ID - 1)*gN1X;
  		else if(dim1_ID > dest_dim1_ID) return gP*gPX + dest_dim1_ID*gN1X;
  		else{ cout<<"[Error] in GetLocalPort()"<<endl; exit(-1); }
  	}
  }*/else{ cout<<"[Error] n should be <= 2"<<endl; exit(-1); }
}

int GetGlobalPort(int src_grp_ID, int dest_grp_ID){/*{{{*/
  int grp_output;

  if(src_grp_ID > dest_grp_ID) grp_output = dest_grp_ID;
  else if(src_grp_ID < dest_grp_ID) grp_output = dest_grp_ID - 1;
  else{ cout<<"[Error] in GetGlobalPort()"<<endl; exit(-1); }

  if(gN == 1) return gP*gPX + (gN1K-1)*gN1X + (grp_output % gGK)*gGX;
  //else if(gN == 2) return gP*gPX + (gN1K-1)*gN1X + (gN2K-1)*gN2X + (grp_output % gGK)*gGX;
  else{ cout<<"[Error] n should be <= 2"<<endl; exit(-1); }
}/*}}}*/

int HopCount(int rID, int dest_rID){/*{{{*/
  int grp_ID = rID/gA;
  int dest_grp_ID = dest_rID/gA;
  int hop_count = 0;

  if(grp_ID == dest_grp_ID){
	  if(gN == 1){
		  if(rID != dest_rID)
		  	hop_count = 1;
	  }/*else if(gN == 2){
	  	int dim1_ID = rID % gN1K;
  		int dim2_ID = (rID % gRouterPerGroup) / gN1K;
		int dest_dim1_ID = dest_rID % gN1K;
		int dest_dim2_ID = (dest_rID % gRouterPerGroup) / gN1K;
		if(dim1_ID == dest_dim1_ID && dim2_ID == dest_dim2_ID) hop_count = 0;
		else if(dim1_ID == dest_dim1_ID) hop_count = 1;
		else{
			if(dim2_ID == dest_dim2_ID) hop_count = 1;
			else hop_count = 2;
		}
	}*/else{ cout<<"[Error] n should be <= 2"<<endl; exit(-1); }
  }else{
    int grp_output_RID = GetGroupOutRouter(grp_ID, dest_grp_ID);
    int grp_input_RID = GetGroupInRouter(grp_ID, dest_grp_ID);

    if(gN == 1){
      if(IsTheRouter(rID, grp_output_RID)) hop_count = 1;
      else hop_count = 2;

      if(grp_input_RID != dest_rID)
        hop_count += 1;
    }/*else if(gN == 2){
			int dim1_ID = rID % gN1K;
			int dim2_ID = (rID % gRouterPerGroup) / gN1K;
			int dest_dim1_ID = grp_output_RID % gN1K;
			int dest_dim2_ID = (grp_output_RID % gRouterPerGroup) / gN1K;

			if(dim1_ID == dest_dim1_ID && dim2_ID == dest_dim2_ID) hop_count = 1;
			else if(dim1_ID == dest_dim1_ID) hop_count = 2;
			else{
				if(dim2_ID == dest_dim2_ID) hop_count = 2;
				else hop_count = 3;
			}

			dim1_ID = grp_input_RID % gN1K;
			dim2_ID = (grp_input_RID % gRouterPerGroup) / gN1K;
			dest_dim1_ID = dest_rID % gN1K;
			dest_dim2_ID = (dest_rID % gRouterPerGroup) / gN1K;

			if(dim1_ID == dest_dim1_ID && dim2_ID == dest_dim2_ID) ;
			else if(dim1_ID == dest_dim1_ID || dim2_ID == dest_dim2_ID) hop_count += 1;
			else hop_count += 2;
		}*/else{ cout<<"[Error] n should be <= 2"<<endl; exit(-1); }
  }

  return hop_count;
}

int GetRoutingPort(int rID, int dest, int* out_vc, int src_vc, int dest_vc, int* isGlobal){
  int out_port;
  int src_grp_ID = (int)(rID/gA);
  int dest_grp_ID = (int)(dest/gP/gA);
  int global = 0;

  if(IsInGroup(rID, dest_grp_ID)){
          if(IsTheRouter(rID, dest/gP)) out_port = dest % gP;
          else out_port = GetLocalPort(rID, dest / gP);
          *out_vc = dest_vc;
  }else if(IsInGroup(rID, src_grp_ID)){
    int grp_output_RID = GetGroupOutRouter(src_grp_ID, dest_grp_ID);

    if(IsTheRouter(rID, grp_output_RID)){
      out_port = GetGlobalPort(src_grp_ID, dest_grp_ID);
      global =1;
    }else out_port = GetLocalPort(rID, grp_output_RID);

    *out_vc = src_vc;
  }else{ cout<<"[Error] This packet is in unknown group"<<endl; exit(-1); }

  if(global == 1) *isGlobal = 1;
  else *isGlobal = 0;
  return out_port;
}

int GetRoutingPort(int rID, int dest, int* out_vc, int src_vc, int dest_vc){
  int dummy = 0;
  return GetRoutingPort(rID, dest, out_vc, src_vc, dest_vc, &dummy);
}

int GetRoutingPort(int rID, int dest, int* isGlobal){
  int dummy = 0;
  return GetRoutingPort(rID, dest, &dummy, 0, 0, isGlobal);
}

int GetRoutingPort(int rID, int dest){
  int dummy;
  return GetRoutingPort(rID, dest, &dummy, 0, 0, &dummy);
}

int GetGroupOutRouter(int src_grp_ID, int dest_grp_ID){/*{{{*/
	int grp_output;

	if(src_grp_ID > dest_grp_ID) grp_output = dest_grp_ID;
	else if(src_grp_ID < dest_grp_ID) grp_output = dest_grp_ID - 1;
	else{ cout<<"[Error] in GetGroupOutRouter()"<<endl; exit(-1); }

	return (int)(grp_output / gGK) + src_grp_ID * gA;
}/*}}}*/

int GetGroupInRouter(int src_grp_ID, int dest_grp_ID){/*{{{*/
	int grp_input;

//	if(src_grp_ID > dest_grp_ID) grp_input = dest_grp_ID - 1;
//	else if(src_grp_ID < dest_grp_ID) grp_input = dest_grp_ID;
//	else{ cout<<"[Error] in GetGroupInRouter()"<<endl; exit(-1); }

	if(src_grp_ID > dest_grp_ID) grp_input = src_grp_ID - 1;
	else if(src_grp_ID < dest_grp_ID) grp_input = src_grp_ID;
	else{ cout<<"[Error] in GetGroupInRouter()"<<endl; exit(-1); }

	return (int)(grp_input / gGK) + dest_grp_ID * gA;
}

int GetGroupOutPort(const int rID, const int dest_router){
  int grp_output;

  int src_grp_ID = (int)(rID/gA);
  int dest_grp_ID = (int)(dest_router/gA);

  if(src_grp_ID > dest_grp_ID) grp_output = dest_grp_ID;
  else if(src_grp_ID < dest_grp_ID) grp_output = dest_grp_ID - 1;
  else{ cout<<"[Error] in GetGroupOutPort()" << endl; exit(-1); }

  return grp_output;
}

// HANS: To calculate channel length difference between minimal and non-minimal path
int GetChanDiff(int src_node, int intm_node, int dest_node){
    int src_router = src_node / gC;
    int intm_router = intm_node / gC;
    int dest_router = dest_node / gC;

    int src_group = src_router / (2 * gC);
    int intm_group = intm_router / (2 * gC);
    int dest_group = dest_router / (2 * gC);

    int local_chan_length = 10; //10
    int global_chan_length = 100; //100

    // Calculate global (inter-group) hop counts
    int min_global_hop = 0;
    if (src_group != dest_group)  min_global_hop += 1;

    int non_global_hop = 0;
    if (src_group != intm_group)  non_global_hop += 1;
    if (intm_group != dest_group) non_global_hop += 1;

    // Calculate intra-group hop counts
    int min_total_hop = dragonflynew_hopcnt(src_node, dest_node);
    int non_total_hop = dragonflynew_hopcnt(src_node, intm_node) + dragonflynew_hopcnt(intm_node, dest_node);

    int min_local_hop = min_total_hop - min_global_hop;
    int non_local_hop = non_total_hop - non_global_hop;

    // Sanity checks
    assert(min_global_hop <= 1);
    assert(non_global_hop <= 2);
    assert(min_local_hop >= 0);
    assert(non_local_hop >= 0);

    // Calculate channel length difference
    int diff = (non_local_hop - min_local_hop) * local_chan_length + (non_global_hop - min_global_hop) * global_chan_length;

    // FOR DEBUGGING
    //cout << GetSimTime() << " - CHANDIFF - SrcRouter: " << src_router << " | IntmRouter: " << intm_router << " | DestRouter: " << dest_router << " | Hmin_Local: " << min_local_hop << " | Hnon_Local: " << non_local_hop << " | Hmin_Global: " << min_global_hop << " | Hnon_Global: " << non_global_hop << " | Hmin_Total: " << min_total_hop << " | Hnon_Total: " << non_total_hop << " | Diff: " << diff << endl;
    // if (diff == 0)
    //   cout << "0DIFF - SrcRouter: " << src_router << " | IntmRouter: " << intm_router << " | DestRouter: " << dest_router << endl;

    //assert(diff >= 0);
    return diff;
}

// HANS: Find intermediate node that avoid useless bandwidth usage
int GetIntmNode(int src_node, int dest_node){
  int _network_size =  gA * gP * gG;
  int intm_node = RandomInt(_network_size - 1);

  int src_router = src_node / gP;
  int intm_router = intm_node / gP;
  int dest_router = dest_node / gP;

  int src_grp = src_router / gA;
  int intm_grp = intm_router / gA;
  int dest_grp = dest_router / gA;
   

  // 1ST SCHEME: If passed by destination router en-route to the intermediate router, don't do VALn
  if ((src_grp != intm_grp) && (intm_grp == dest_grp)){
    int gin_router = GetGroupInRouter(src_grp, intm_grp);
    if (gin_router == dest_router){
      intm_node = dest_node;
    }
  // 2ND SCHEME: If the intermediate group en-route to the intermediate router, passed by the router with the global port to the destination group, don't do VALn
  } else if ((src_grp != intm_grp) && (intm_grp != dest_grp)){
    int gin_router = GetGroupInRouter(src_grp, intm_grp);
    int gout_router = GetGroupOutRouter(intm_grp, dest_grp);

    if (gin_router == gout_router){
      intm_node = gin_router * gP + RandomInt(gP - 1);
    }
  }

  return intm_node;
}