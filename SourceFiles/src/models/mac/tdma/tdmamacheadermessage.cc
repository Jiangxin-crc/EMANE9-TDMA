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

#include "tdmamacheadermessage.h"
#include "tdmamacheader.pb.h"


class EMANE::Models::TDMA::MACHeaderMessage::Implementation
{
public:
  Implementation(std::uint8_t sequence, std::uint8_t fragment, std::uint8_t datarate, std::uint8_t len ):
      sequence_(sequence),
      fragflag_(fragment),
      datarate_(datarate),
      len_(len)
    { }
  Implementation():
      sequence_(0),
      fragflag_(0),
      datarate_(0),
      len_(0)
    { }

    bool isFragment()                           {       return fragflag_!=0;            }
    void setLast()                              {       fragflag_ += 129;               }
    void incFrag()                              {       fragflag_++;                    }
    std::uint8_t getFlag()                      {       return fragflag_;               }
    void setSequence(std::uint8_t sequence)     {       sequence_ = sequence;           }
    std::uint8_t getSequence ()                 {       return sequence_;               }
    std::uint8_t getDataRate()			{	return datarate_;		}
    void setDataRate(std::uint8_t datarate)	{	datarate_ = datarate;		}
    std::uint8_t getLen()			{	return len_ - 2;		}

private:
    std::uint8_t        sequence_;         // sequence number
    std::uint8_t        fragflag_;         // 0xxxxxxx seq of fragment
                                           // 1xxxxxxx last fragment
    std::uint8_t	datarate_;
    std::uint8_t	len_;
};


EMANE::Models::TDMA::MACHeaderMessage::MACHeaderMessage(std::uint8_t sequence, std::uint8_t fragment, std::uint8_t datarate, std::uint8_t len) :
  pImpl_{new Implementation{sequence,fragment,datarate,len}}
{ }

EMANE::Models::TDMA::MACHeaderMessage::MACHeaderMessage(const void * p, size_t len) 
{
  EMANEMessage::TdmaMACHeader message;

  try
    {
      if(!message.ParseFromArray(p, len))
        {
          throw SerializationException("unable to deserialize MACHeaderMessage");
        }
    }
  catch(google::protobuf::FatalException & exp)
    {
      throw SerializationException("unable to deserialize MACHeaderMessage");
    }

  using RepeatedPtrFieldDR = 
    google::protobuf::RepeatedPtrField<EMANEMessage::TdmaMACHeader_resv>;
  
  std::uint8_t dataratem=1;
  std::uint8_t lenp = 2;
  
  for(const auto & iter : RepeatedPtrFieldDR(message.datarate()))
    {
     dataratem = (static_cast<std::uint8_t>(iter.byte()));
     lenp++;
    }

  pImpl_.reset(new Implementation{static_cast<std::uint8_t>(message.sequence()),
					static_cast<std::uint8_t>(message.flag()),
					dataratem,  lenp});

}



EMANE::Models::TDMA::MACHeaderMessage::~MACHeaderMessage()
{ }


bool EMANE::Models::TDMA::MACHeaderMessage::isFragment() 
{
  return pImpl_->isFragment();
}

void EMANE::Models::TDMA::MACHeaderMessage::setLast() 
{
  pImpl_->setLast();
}

void  EMANE::Models::TDMA::MACHeaderMessage::incFrag() 
{
  pImpl_->incFrag();
}

std::uint8_t EMANE::Models::TDMA::MACHeaderMessage::getFlag() 
{
  return pImpl_->getFlag();
}

void EMANE::Models::TDMA::MACHeaderMessage::setSequence(std::uint8_t seq) 
{
  pImpl_->setSequence(seq);
}

std::uint8_t EMANE::Models::TDMA::MACHeaderMessage::getSequence() 
{
  return pImpl_->getSequence();
}

void EMANE::Models::TDMA::MACHeaderMessage::setDataRate(std::uint8_t rate) 
{
  pImpl_->setDataRate(rate);
}

std::uint8_t EMANE::Models::TDMA::MACHeaderMessage::getDataRate() 
{
  return pImpl_->getDataRate();
}

std::uint8_t EMANE::Models::TDMA::MACHeaderMessage::getLen() 
{
  return pImpl_->getLen();
}


EMANE::Serialization EMANE::Models::TDMA::MACHeaderMessage::serialize() const
{
  Serialization serialization;

  try
    {
      EMANEMessage::TdmaMACHeader message;

      message.set_sequence(pImpl_->getSequence());
      message.set_flag(pImpl_->getFlag());
      for(int i=0;i<pImpl_->getLen();i++)
    	{
      	   auto iter = message.add_datarate();

      	   iter->set_byte(pImpl_->getDataRate());
    	}

      if(!message.SerializeToString(&serialization))
        {
          throw SerializationException("unable to serialize MACHeaderMessage");
        }
    }
  catch(google::protobuf::FatalException & exp)
    {
      throw SerializationException("unable to serialize MACHeaderMessage");
    }
  
  return serialization;
}
