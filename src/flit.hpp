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

#ifndef _FLIT_HPP_
#define _FLIT_HPP_

#include <iostream>
#include <stack>
#include <queue>

#include "booksim.hpp"
#include "outputset.hpp"

#ifdef DGB_ON
struct learnset{
    int id; // Record the fID
    int gen_time;
    int src_router; // Router who generates the learnset
    queue<float> diff_min;
    queue<pair<int, float>> diff_non;
};
#endif

class Flit {

public:

  const static int NUM_FLIT_TYPES = 5;
  enum FlitType { READ_REQUEST  = 0, 
		  READ_REPLY    = 1,
		  WRITE_REQUEST = 2,
		  WRITE_REPLY   = 3,
                  ANY_TYPE      = 4 };
  FlitType type;

  int vc;

  int cl;

  bool head;
  bool tail;
  
//#ifdef BOOKSIM_STANDALONE
  int  ctime;
  int  itime;
  int  atime;
// #else
//   uint64_t  ctime;
//   uint64_t  itime;
//   uint64_t  atime;
// #endif

  int  id;
  int  pid;

  bool record;

  int  src;
  int  dest;

#ifdef BOOKSIM_STANDALONE
  int  pri;
#else
  uint64_t pri;
#endif

  int  hops;
  bool watch;
  int  subnetwork;
  
  // intermediate destination (if any)
  mutable int intm;

  // phase in multi-phase algorithms
  mutable int ph;

  // Fields for arbitrary data
  void* data ;

  // Lookahead route info
  OutputSet la_route_set;

  void Reset();

  static Flit * New();
  void Free();
  static void FreeAll();

  // HANS: Additional entries
  mutable int min;
  mutable bool force_min;

#ifdef DGB_ON
  mutable bool dgb_train;

  mutable int   h_min;
  mutable int   h_non;
  mutable int   q_min;
  mutable int   q_non;
  mutable int   q_min_global;
  mutable int   q_non_global;
 
  mutable int   q_count_at_2ndrouter;
  mutable int   q_total_srcgrp;
 
  mutable int   min_port;
  mutable int   non_port;
  mutable int   min_global_port;
  mutable int   non_global_port;
 
  mutable int   contention;
  mutable int   until_2ndrouter;
  mutable int   lat_start;
  mutable int   wire_s;
  mutable int   wire_total;

  mutable Flit* tail_flit;
#endif

private:

  Flit();
  ~Flit() {}

  static stack<Flit *> _all;
  static stack<Flit *> _free;

};

ostream& operator<<( ostream& os, const Flit& f );

#endif
