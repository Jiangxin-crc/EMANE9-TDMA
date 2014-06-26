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

#include "tdmaevent.h"
#include "tdmabevent.pb.h"
#include "tdmarevent.pb.h"

class EMANE::Models::TDMA::TdmaBEvent::Implementation
 {
   public:
     Implementation(std::uint64_t slot0time, const SlotMap & slotmap, const std::uint32_t & subid) :
       slot0time_(slot0time),
       tdmaSubId_(subid),
       slotmap_{slotmap}
     { }

     const SlotMap & getSlotMap() const
      {
        return slotmap_;
      }

     std::uint64_t getSlot0time() const
      {
        return slot0time_;
      }

     std::uint32_t getSubId() const
      {
        return tdmaSubId_;
      }

   private:
     std::uint64_t  slot0time_;
     std::uint32_t  tdmaSubId_;
     SlotMap slotmap_;
 };


EMANE::Models::TDMA::TdmaBEvent::TdmaBEvent(const Serialization & serialization)
  throw(SerializationException):
  Event(IDENTIFIER)
{
  EMANEEventMessage::TdmaBEvent msg;

  try
    {
      if(!msg.ParseFromString(serialization))
        {
          throw SerializationException("unable to deserialize : TdmaBEvent");
        }
    }
  catch(google::protobuf::FatalException & exp)
    {
      throw SerializationException("unable to deserialize : TdmaBEvent");
    }
  
  using RepeatedPtrFieldSlotMap = 
    google::protobuf::RepeatedPtrField<EMANEEventMessage::TdmaBEvent_SlotMapping>;
  
  SlotMap mapping;
  
  for(const auto & iter : RepeatedPtrFieldSlotMap(msg.mappings()))
    {
      mapping.push_back(static_cast<NEMId>(iter.nemid()));
    }

  pImpl_.reset(new Implementation{static_cast<std::uint64_t>(msg.slotzerotime()), mapping,
				  static_cast<std::uint32_t>(msg.tdmasubid())});
}
    
EMANE::Models::TDMA::TdmaBEvent::TdmaBEvent(std::uint64_t slot0time, 
				const SlotMap & slotmap, const std::uint32_t & subid):
  Event{IDENTIFIER},
  pImpl_{new Implementation{slot0time, slotmap,subid}}{}

    

EMANE::Models::TDMA::TdmaBEvent::~TdmaBEvent(){}

    
const EMANE::Models::TDMA::SlotMap & 
EMANE::Models::TDMA::TdmaBEvent::getSlotmap() const
{
  return pImpl_->getSlotMap();
}

std::uint64_t
EMANE::Models::TDMA::TdmaBEvent::getSlot0time() const
{
  return pImpl_->getSlot0time();
}

std::uint32_t
EMANE::Models::TDMA::TdmaBEvent::getSubId() const
{
  return pImpl_->getSubId();
}

EMANE::Serialization EMANE::Models::TDMA::TdmaBEvent::serialize() const
{
  Serialization serialization;

  EMANEEventMessage::TdmaBEvent msg;

  msg.set_slotzerotime(pImpl_->getSlot0time());
  msg.set_tdmasubid(pImpl_->getSubId());

  for(auto & nemid : pImpl_->getSlotMap())
    {
      auto iter = msg.add_mappings();

      iter->set_nemid(nemid);
    }

  try
    {
      if(!msg.SerializeToString(&serialization))
        {
          throw SerializationException("unable to serialize : TdmaBEvent");
        }
    }
  catch(google::protobuf::FatalException & exp)
    {
      throw SerializationException("unable to serialize : TdmaBEvent");
    }

  return serialization;
}

//======================================================================================

class EMANE::Models::TDMA::TdmaREvent::Implementation
 {
   public:
     Implementation(NEMId id, const R_TYPE & type, const std::string & uuid, const std::uint32_t & subid,
		    const std::uint32_t & slotnum, const std::uint32_t & slotlen ) :
       eventSource_(id),
       type_(type),
       uuid_{uuid},
       tdmaSubId_(subid),
       tdmaSlotNum_(slotnum),
       slotLength_(slotlen)
     { }

     const R_TYPE & getType() const
      {
        return type_;
      }

     const std::string & getUuid() const
      {
        return uuid_;
      }

     std::uint32_t getSubId() const
      {
        return tdmaSubId_;
      }
     std::uint32_t getSlotNum() const
      {
        return tdmaSlotNum_;
      }
     std::uint32_t getSlotLen() const
      {
        return slotLength_;
      }

     NEMId getEventSource() const
      {
        return eventSource_;
      }

   private:
     NEMId  eventSource_;
     const R_TYPE type_;
     const std::string uuid_;
     std::uint32_t  tdmaSubId_;
     std::uint32_t  tdmaSlotNum_;
     std::uint32_t  slotLength_;
};

EMANE::Models::TDMA::TdmaREvent::TdmaREvent(const Serialization & serialization)
  throw(SerializationException):
  Event(IDENTIFIER)
{
  EMANEEventMessage::TdmaREvent msg;

  try
    {
      if(!msg.ParseFromString(serialization))
        {
          throw SerializationException("unable to deserialize : TdmaBEvent");
        }
    }
  catch(google::protobuf::FatalException & exp)
    {
      throw SerializationException("unable to deserialize : TdmaBEvent");
    }
  
  using RepeatedPtrFieldSlotMap = 
    google::protobuf::RepeatedPtrField<EMANEEventMessage::TdmaBEvent_SlotMapping>;
  
  pImpl_.reset(new Implementation{static_cast<NEMId>(msg.eventsource()),
				static_cast<R_TYPE>(msg.eventtype()),
				static_cast<std::string>(msg.uuid()),
				static_cast<std::uint32_t>(msg.tdmasubid()),
				static_cast<std::uint32_t>(msg.tdmaslotnum()),
				static_cast<std::uint32_t>(msg.slotlength())
				});
}
    
EMANE::Models::TDMA::TdmaREvent::TdmaREvent(NEMId id, const R_TYPE & type, const std::string & uuid, 
	const std::uint32_t & subid, const std::uint32_t & slotnum, const std::uint32_t & slotlen):
  Event{IDENTIFIER},
  pImpl_{new Implementation{id,type,uuid,subid,slotnum,slotlen}}{}

    

EMANE::Models::TDMA::TdmaREvent::~TdmaREvent(){}

    
const EMANE::Models::TDMA::R_TYPE & 
EMANE::Models::TDMA::TdmaREvent::getType() const
{
  return pImpl_->getType();
}

const std::string &
EMANE::Models::TDMA::TdmaREvent::getUuid() const
{
  return pImpl_->getUuid();
}

EMANE::NEMId 
EMANE::Models::TDMA::TdmaREvent::getEventSource() const
{
  return pImpl_->getEventSource();
}

std::uint32_t
EMANE::Models::TDMA::TdmaREvent::getSubId() const
{
  return pImpl_->getSubId();
}

std::uint32_t
EMANE::Models::TDMA::TdmaREvent::getSlotNum() const
{
  return pImpl_->getSlotNum();
}

std::uint32_t
EMANE::Models::TDMA::TdmaREvent::getSlotLen() const
{
  return pImpl_->getSlotLen();
}

EMANE::Serialization EMANE::Models::TDMA::TdmaREvent::serialize() const
{
  Serialization serialization;

  EMANEEventMessage::TdmaREvent msg;

  msg.set_eventsource(pImpl_->getEventSource());
  msg.set_eventtype(EMANEEventMessage::TdmaREvent::R_TYPE(pImpl_->getType()));
  msg.set_tdmasubid(pImpl_->getSubId());
  msg.set_tdmaslotnum(pImpl_->getSlotNum());
  msg.set_uuid(pImpl_->getUuid());
  msg.set_slotlength(pImpl_->getSlotLen());

  try
    {
      if(!msg.SerializeToString(&serialization))
        {
          throw SerializationException("unable to serialize : TdmaBEvent");
        }
    }
  catch(google::protobuf::FatalException & exp)
    {
      throw SerializationException("unable to serialize : TdmaBEvent");
    }

  return serialization;
}


