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

#include "fragmentmgr.h"

namespace
{
  const char * pzLayerName{"TdmaMACLayer"};
}

EMANE::Models::TDMA::FragmentManager::FragmentManager(NEMId id, PlatformServiceProvider * pPlatformServiceProvider)
  : pPlatformService_(pPlatformServiceProvider),
    timeout_(101000000),
    id_(id)
{
}


EMANE::Models::TDMA::FragmentManager::~FragmentManager() 
{  
} 

EMANE::UpstreamPacket 
EMANE::Models::TDMA::FragmentManager::process(EMANE::UpstreamPacket & pkt, EMANE::PacketInfo info,  struct MacHeader * xmac)
{
    EMANE::Models::TDMA::MACHeaderMessage mac(xmac->sequence,xmac->fragflag,xmac->datarate,xmac->len);
    size_t num = buffer_.size();
    bool exist = false;
    EMANE::UpstreamPacket ret(EMANE::PacketInfo(0,0,info.getPriority(),info.getCreationTime()),0,0);
    // get current time
    TimePoint currTime{Clock::now()};

    for (size_t i=0;i<num;i++) {
	FragmentItem item = buffer_.front();
	buffer_.pop_front();
	if (item.sour_ == info.getSource() && item.dest_ == info.getDestination() && item.pktseq_ == mac.getSequence()) {
	    exist = true;
	    int seq = mac.getFlag();
	    if (seq>128) {
		seq = seq-128;
		item.total_ = seq;
	    }
	    if (seq == item.nextFrag_) {
		item.nextFrag_++;
		item.readylen_ += pkt.length();
	    }
	    FragPkt fpkt;
	    fpkt.pkt_ = pkt;
	    fpkt.seq_ = seq;
	    item.readyFrag_.push(fpkt);
	    if (item.nextFrag_>item.total_ && item.total_>0) {
		// packet ready
		unsigned char* buffer = new unsigned char[item.readylen_];
		int loc = 0;
		for (int i=1;i<=item.total_;i++) {
		    bool ok = false;
		    int k = item.readyFrag_.size();
		    for ( int m=0; m<k; m++) {
			FragPkt fp = item.readyFrag_.front();
			item.readyFrag_.pop();
			if (fp.seq_ == i) {
			    ok = true;
			    memcpy(&buffer[loc], fp.pkt_.get(), fp.pkt_.length());
			    loc += fp.pkt_.length();
			    break;
			}
			item.readyFrag_.push(fp);
		    }
		    if (!ok) {
			// big trouble
                      LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                                              ERROR_LEVEL,
                                              "MACI %03hu %s::%s fragmentation error. source %d  dest %d",
                                              id_,
                                              pzLayerName,
                                              __func__,
						info.getSource(),
						info.getDestination());
		    }
		}
                ret = EMANE::UpstreamPacket(info,(void*)(buffer),(size_t) (item.readylen_));

		delete [] buffer;
	    }
	    else
		buffer_.push_back(item);
	}
	else {
	    // check, remove if timeout
	    if (currTime - item.tpTimeout_ > timeout_) {
		// timeout remove it
	    }
	    else {  // keep it
		buffer_.push_back(item);
	    }
	}

    }
    if (!exist) {
	// first frag of a packet
	std::queue<FragPkt> q;
	FragPkt fpkt;
	fpkt.seq_ = mac.getFlag();
	fpkt.pkt_ = pkt;
	if (fpkt.seq_>128) fpkt.seq_ = fpkt.seq_ - 128;
	ACE_UINT8 total=0,next = 1;
	q.push(fpkt);
	if (mac.getFlag()>128) total = mac.getFlag()-128;
	if (mac.getFlag()==next) next++;
	FragmentItem item(info.getSource(),info.getDestination(),mac.getSequence(),q,next,false,currTime+timeout_,total,next==1?0:pkt.length());
	buffer_.push_back(item);
    }

    return ret;
}
