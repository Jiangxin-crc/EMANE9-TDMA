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

#ifndef TDMAMAC_DOWNSTREAMMGR_HEADER_
#define TDMAMAC_DOWNSTREAMMGR_HEADER_

#include "emane/types.h"
#include "emane/phylayerimpl.h"
#include "emane/platformserviceprovider.h"
#include "emane/statisticnumeric.h"
#include "tdmamacheadermessage.h"

#include <queue>
#include <vector>

namespace EMANE
{
  namespace Models
  {
    namespace TDMA
    {

      /**
       * Downstream queue entry definition
       *
       * @brief Tdma MAC downstream queue entry definition
       */
      struct DownstreamQueueEntry 
      {
        DownstreamPacket pkt_;              // packet payload
        std::uint64_t u64SequenceNumber_;   // packet sequence number
        TimePoint acquireTime_;             // packet acquire time (absolute time)
        Microseconds durationMicroseconds_; // packet transmission duration
        std::uint64_t u64DataRatebps_;      // packet data rate bps
    	std::uint8_t  sequence_;         // sequence number
    	std::uint8_t  fragflag_;         
    	std::uint8_t  datarate_;
    	std::uint8_t  len_;
	std::uint8_t  u8Priority_;

        DownstreamQueueEntry() :
          pkt_{DownstreamPacket{EMANE::PacketInfo{0,0,0,{}},nullptr,0}},
          u64SequenceNumber_{},
          acquireTime_{},
          durationMicroseconds_{},
          u64DataRatebps_{},
	  sequence_{},fragflag_{},datarate_{},len_{},
	  u8Priority_{}
        {}


        /**
         *
         * @brief  initializer
         *
         * @param pkt                  reference to the downstream packet
         * @param u64SequenceNumber    sequence number
         * @param acquireTime          acquireTimetart of transmission
         * @param durationMicroseconds duration of transmision
         * @param u64DataRatebps       data rate bps
         */

        DownstreamQueueEntry(DownstreamPacket & pkt, 
                             std::uint64_t u64SequenceNumber, 
                             const TimePoint & acquireTime,
                             const Microseconds & durationMicroseconds,
                             std::uint64_t u64DataRatebps,
			     std::uint8_t seq,std::uint8_t frag, std::uint8_t dr, std::uint8_t len,
			     std::uint8_t  u8Priority) :
          pkt_{std::move(pkt)},
          u64SequenceNumber_{u64SequenceNumber},
          acquireTime_{acquireTime},
          durationMicroseconds_{durationMicroseconds},
          u64DataRatebps_{u64DataRatebps},
	  sequence_{seq},fragflag_{frag},datarate_{dr},len_{len},
	  u8Priority_{u8Priority}
        {}
      };


      typedef std::list<DownstreamQueueEntry> DownstreamPacketQueue;

      /**
       * @class DownstreamQueue
       *
       * @brief Provides a queue implementation for the Tdma Mac layer.
       *
       */
      class DownstreamQueueMgr
      {
      public:

        /**
         * @brief Constructor
         *
         */ 
        DownstreamQueueMgr();

        /**
         *
         * @brief Destructor
         *
         */
        ~DownstreamQueueMgr();

        /**
         * 
         * @brief Returns the number of discards
         * @param bClear clear counter
         *
         * @retval size_t num of packets discarded
         *
         */
        size_t getNumDiscards(bool bClear);


        /**
         * 
         * @brief Returns the current size of the queue
         *
         * @retval size_t current size of the queue
         *
         */
        size_t getCurrentDepth();

        /**
         * 
         * @brief Returns the max size of the queue
         *
         * @retval size_t max size of the queue
         *
         */
        size_t getMaxCapacity();

        /**
         * 
         * @brief removes an element from the queue
         *
         * @return entry the pop'd entry
         *
         */
        std::pair<DownstreamQueueEntry,bool> dequeue();

        /**
         * 
         * @brief Adds an element to the queue
         *
         * @param entry the entry to be added to the queue
         *
         * @return return true if the queue was full and the old entry
         *
         */
        std::vector<DownstreamQueueEntry>
        enqueue(DownstreamQueueEntry &entry);

        void enqueue_front(DownstreamQueueEntry &entry);

        /**
         * 
         * @brief Returns a reference to the element to be pop'd next
         *
         * @return entry the entry to be pop'd next
         *
         */
        const DownstreamQueueEntry & peek();

	bool empty();

      private:
        DownstreamPacketQueue queue_;
        const size_t maxQueueSize_;
        size_t numDiscards_;
      };
    }
  }
}

#endif //TDMAMAC_DOWNSTREAMQUEUE_HEADER_
