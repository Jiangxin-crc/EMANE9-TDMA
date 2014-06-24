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

#ifndef EMANEMODELSTDMAMACHEADERMESSAGE_HEDAER_
#define EMANEMODELSTDMAMACHEADERMESSAGE_HEDAER_


#include <cstdint>
#include <memory>

#include "emane/types.h"
#include "emane/serializable.h"

namespace EMANE
{
  namespace Models
  {
    namespace TDMA
    {

      class MACHeaderMessage : public Serializable
      {
      public:
        MACHeaderMessage(std::uint8_t sequence, std::uint8_t fragment, std::uint8_t datarate, std::uint8_t len);
            
        /**
         * @throw SerializationException
         */
        MACHeaderMessage(const void * p, size_t len);

        ~MACHeaderMessage();

    	bool isFragment();
    	void setLast();
    	void incFrag();
    	std::uint8_t getFlag();
	std::uint8_t getDataRate();
	void setDataRate(std::uint8_t datarate);
    	void setSequence(std::uint8_t sequence);
    	std::uint8_t getSequence();
	std::uint8_t getLen();

        Serialization serialize() const override;
     
      private:
        class Implementation;

        std::unique_ptr<Implementation> pImpl_;
      
      };

      struct MacHeader {
    	std::uint8_t    sequence;        
    	std::uint8_t    fragflag;         
    	std::uint8_t	datarate;
    	std::uint8_t	len;
      };


    }
  }
}

#endif //EMANEMODELSTDMAMACHEADERMESSAGE_HEDAER_
