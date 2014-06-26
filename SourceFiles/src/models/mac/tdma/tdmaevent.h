/*
 * Copyright (c) Her Majesty the Queen in right of Canada  (2014)
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
 * * Neither the name of Her Majesty the Queen in right of Canada nor
 *   the names of her contributors may be used to endorse or promote
 *   products derived from this software without specific prior written
 *   permission.
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
 *
 * See toplevel COPYING for more information.
 */

#ifndef EMANEMODELSTDMABEVENT_HEADER_
#define EMANEMODELSTDMABEVENT_HEADER_

#include "emane/event.h"
#include "emane/events/eventids.h"

#include <memory>
#include <vector>


namespace EMANE
{
  namespace Models
  {
    namespace TDMA
    {
         typedef std::vector<EMANE::NEMId> SlotMap;

         enum R_TYPE  { TDMA_TYPE_INVALID   = 0x00,
                        TDMA_TYPE_INIT      = 0x01,
                        TDMA_TYPE_NOTIFY    = 0x02,
			TDMA_TYPE_NEMINIT   = 0x03,
			TDMA_TYPE_FREE_SLOT = 0x04,
			TDMA_TYPE_REQ_SLOT  = 0x05
                        };

      class TdmaBEvent : public Event
      {
      public:
        /**
         * @throw SerializationException
         */
        TdmaBEvent(const std::string & sSerialization)
          throw(SerializationException);
      
        TdmaBEvent(std::uint64_t slot0time, const SlotMap & slotmap, const std::uint32_t & subid);
       
        ~TdmaBEvent();
      
        Serialization serialize() const override;
      
        const SlotMap & getSlotmap() const;

        std::uint64_t getSlot0time() const;
      
        std::uint32_t getSubId() const;

        enum {IDENTIFIER = EMANE_EVENT_TDMA_B};
      
      private:
        class Implementation;

        std::unique_ptr<Implementation> pImpl_;
      };

      class TdmaREvent : public Event
      {
      public:
        /**
         * @throw SerializationException
         */
        TdmaREvent(const std::string & sSerialization)
          throw(SerializationException);
      
        TdmaREvent(NEMId id, const R_TYPE & type, const std::string & uuid, 
		    const std::uint32_t & subid, const std::uint32_t & slotnum, 
		    const std::uint32_t & slotlen);
       
        ~TdmaREvent();
      
        Serialization serialize() const override;
      
        const R_TYPE & getType() const;

	const std::string & getUuid() const;

        std::uint32_t getSubId() const;
      
        std::uint32_t getSlotNum() const;
      
        std::uint32_t getSlotLen() const;
      
        NEMId getEventSource() const;
      
        enum {IDENTIFIER = EMANE_EVENT_TDMA_R};
      
      private:
        class Implementation;

        std::unique_ptr<Implementation> pImpl_;
      };

    }
  }
}


#endif // EMANEMODELSTDMABEVENT_HEADER_

