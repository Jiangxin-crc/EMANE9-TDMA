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


#ifndef TDMAMAC_DOWNSTREAMQUEUE_HEADER_
#define TDMAMAC_DOWNSTREAMQUEUE_HEADER_

#include "downstreammgr.h"
#include <vector>

namespace EMANE
{
  namespace Models
  {
    namespace TDMA
    {
      /**
       * @class DownstreamQueue
       *
       * @brief Provides a queue implementation for the Tdma Mac layer.
       *
       */
      class DownstreamQueue
      {
      public:

        /**
         * @brief Constructor
         *
         */ 
        DownstreamQueue();

        /**
         *
         * @brief Destructor
         *
         */
        ~DownstreamQueue();

        void registerStatistics(StatisticRegistrar & statisticRegistrar);

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


      private:
        StatisticNumeric<std::uint32_t> * pNumHighWaterMark_;
        std::vector<DownstreamQueueMgr> queuemgr_;
      };
    }
  }
}

#endif //TDMAMAC_DOWNSTREAMQUEUE_HEADER_
