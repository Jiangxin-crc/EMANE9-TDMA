/*
 * Copyright (c) Her Majesty the Queen in right of Canada  (2014)
 * Copyright (c) 2013-2014 - Adjacent Link LLC, Bridgewater, New Jersey
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of Adjacent Link LLC nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include "downstreamqueue.h"

namespace 
{
  const std::uint16_t QUEUE_SIZE_DEFAULT{0xFF};
  const std::uint16_t QUEUE_PRIORITY_LEVEL{4};
}

EMANE::Models::TDMA::DownstreamQueue::DownstreamQueue():
  pNumHighWaterMark_{}
{
    for (int i=0;i<QUEUE_PRIORITY_LEVEL;i++) {
	queuemgr_.push_back(DownstreamQueueMgr{});
    }
}


EMANE::Models::TDMA::DownstreamQueue::~DownstreamQueue() 
{}


void EMANE::Models::TDMA::DownstreamQueue::registerStatistics(StatisticRegistrar & statisticRegistrar)
{ 
  pNumHighWaterMark_ =
     statisticRegistrar.registerNumeric<std::uint32_t>("numHighWaterMark",
                                                       StatisticProperties::CLEARABLE);
}



size_t 
EMANE::Models::TDMA::DownstreamQueue::getNumDiscards(bool bClear) 
{ 
   int num = 0;
   for (int i=0;i<QUEUE_PRIORITY_LEVEL;i++) {
	num += queuemgr_[i].getNumDiscards(bClear);
   }
   return num;
}


size_t 
EMANE::Models::TDMA::DownstreamQueue::getCurrentDepth()
{ 
   int depth = 0;
   for (int i=0;i<QUEUE_PRIORITY_LEVEL;i++) {
	depth += queuemgr_[i].getCurrentDepth();
   }
   return depth;
}


size_t 
EMANE::Models::TDMA::DownstreamQueue::getMaxCapacity()
{ 
   return queuemgr_[0].getMaxCapacity();
}


std::pair<EMANE::Models::TDMA::DownstreamQueueEntry,bool>
EMANE::Models::TDMA::DownstreamQueue::dequeue()
{ 
   for (int i=0;i<QUEUE_PRIORITY_LEVEL;i++) {
	if (queuemgr_[i].empty() == false)
	    return queuemgr_[i].dequeue();
   }
  return queuemgr_[0].dequeue();
}


std::vector<EMANE::Models::TDMA::DownstreamQueueEntry>
EMANE::Models::TDMA::DownstreamQueue::enqueue(DownstreamQueueEntry &entry) 
{ 
   std::vector<DownstreamQueueEntry> result;
   if (entry.u8Priority_<QUEUE_PRIORITY_LEVEL)
   	result = queuemgr_[entry.u8Priority_].enqueue(entry);
   std::uint16_t size = getCurrentDepth();
   if(size > pNumHighWaterMark_->get()) 
     {
       *pNumHighWaterMark_ = size;
     }

   return result;
}

void
EMANE::Models::TDMA::DownstreamQueue::enqueue_front(DownstreamQueueEntry &entry) 
{ 
   if (entry.u8Priority_<QUEUE_PRIORITY_LEVEL)
   	queuemgr_[entry.u8Priority_].enqueue_front(entry);
}


const EMANE::Models::TDMA::DownstreamQueueEntry & 
EMANE::Models::TDMA::DownstreamQueue::peek()
{ 
   for (int i=0;i<QUEUE_PRIORITY_LEVEL;i++) {
	if (queuemgr_[i].empty() == false)
	    return queuemgr_[i].peek();
   }
   return queuemgr_[0].peek();
}


