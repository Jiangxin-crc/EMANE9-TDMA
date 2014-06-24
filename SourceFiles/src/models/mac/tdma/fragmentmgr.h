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

#ifndef CRCTDMAFRAGMENT_HEADER_
#define CRCTDMAFRAGMENT_HEADER_
#include <queue>
#include "emane/maclayerimpl.h"
#include "emane/mactypes.h"
#include "tdmamacheadermessage.h"

namespace EMANE
{
  namespace Models
  {
    namespace TDMA
    {

 struct FragPktStru {
    std::uint8_t seq_;
    EMANE::UpstreamPacket pkt_;

    FragPktStru() : seq_(0),pkt_(EMANE::PacketInfo(0,0,EMANE::Priority(0),Clock::now()),0,0) {}
 };

 typedef struct FragPktStru FragPkt;

 class FragmentItem 
 {
  public:
   FragmentItem( EMANE::NEMId 	sour, 
		EMANE::NEMId 	dest, 
		std::uint8_t	pktseq,
		std::queue<FragPkt>  readyFrag,
		std::uint8_t	nextFrag, 
		bool		ready,
		TimePoint  	tpTimeout,   
		std::uint8_t 	total, 
		size_t		readylen
			) :
     pktseq_(pktseq),
     nextFrag_(nextFrag),
     readyFrag_(readyFrag),
     tpTimeout_(tpTimeout),
     ready_(ready),
     dest_(dest),
     sour_(sour),
     total_(total),
     readylen_(readylen)
   { }
  
   FragmentItem() :
     pktseq_(0),
     nextFrag_(0),
     tpTimeout_(Clock::now()),
     ready_(false),
     dest_(0),
     sour_(0),
     total_(0),
     readylen_(0)
   { }

   std::uint8_t		pktseq_;
   std::uint8_t		nextFrag_;
   std::queue<FragPkt>  readyFrag_;
   TimePoint    	tpTimeout_;                  // packet timeout time
   bool			ready_;
   EMANE::NEMId 	dest_;
   EMANE::NEMId 	sour_;
   std::uint8_t		total_;
   size_t		readylen_;


 };

 typedef std::list<FragmentItem> FragmentItemList;
 typedef FragmentItemList::iterator FragmentItemListIt; 

 /**
  *
  * @brief .
  *
  */
  class FragmentManager
  {
    public:
  /**
   * @brief Constructor
   *
   * @param pPlatformService reference to the platformservice provider
   *
   */ 
  FragmentManager(NEMId id, PlatformServiceProvider * pPlatformServiceProvider);

  /**
   *
   * @brief Destructor
   *
   */
  virtual ~FragmentManager();
	
   EMANE::UpstreamPacket process(EMANE::UpstreamPacket & pkt, EMANE::PacketInfo info, struct MacHeader * mac);

    private:
	FragmentItemList buffer_;
  	EMANE::PlatformServiceProvider * pPlatformService_;
	Microseconds timeout_;
  	EMANE::NEMId id_;
  };

      }
   }

}
#endif //CRCTDMAFRAGMENT_HEADER_
