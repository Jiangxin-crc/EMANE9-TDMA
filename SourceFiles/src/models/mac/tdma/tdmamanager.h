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

#ifndef EMANEAPPLICATIONTDMAMANAGER_HEADAER_
#define EMANEAPPLICATIONTDMAMANAGER_HEADAER_

#include <mutex>
#include <memory>
#include <thread>
#include <vector>
#include <set>
#include <map>

#include <emane/registrar.h>
#include <emane/timerserviceuser.h>
#include <emane/types.h>
#include <emane/eventserviceuser.h>
#include "emane/platformserviceuser.h"
#include "tdmaevent.h"


namespace EMANE
{
  namespace Models
  {
    namespace TDMA
    {

    class TDMASlotMap
    {
	public:
	TDMASlotMap(std::uint16_t id, std::uint16_t num);
	~TDMASlotMap();

	std::uint16_t getSubId();
	std::uint16_t getSlotNum();
	std::uint32_t getSlotLen();
	NEMId getOwner(std::uint16_t slot);
	void setOwner(std::uint16_t slot, NEMId owner);
	std::vector<EMANE::NEMId> & getMap();

	void setSlotLen(std::uint32_t len);
	std::uint16_t getUsedNum(EMANE::NEMId nodeid);
	void free(EMANE::NEMId nodeid);
	bool needReassign();
	bool reassign(std::uint16_t need, EMANE::NEMId nodeid);
	void setReqNum(EMANE::NEMId nodeid,std::uint16_t reqnum);

	private:
	std::uint16_t sub_id_;
	std::uint16_t slot_a_cycle_;
	std::vector<EMANE::NEMId> used_slot_;
	std::vector<EMANE::NEMId> avai_slot_;
	std::map<EMANE::NEMId,std::uint16_t> reqed_slot_;
	std::uint32_t slot_length_;
	bool dynamic_;
    };

    class MACLayer;

    /**
     * @class TDMAManager
     *
     * @brief Unique point to manage TDMA resources.
     *
     */
    class TDMAManager : public TimerServiceUser
    {
	public:
	TDMAManager(PlatformServiceProvider *pPlatformServiceProvider);
	~TDMAManager();

      	void initialize(std::auto_ptr<MACLayer> eventproxy, NEMId nid);
	void sendInitEvent(std::string uuid);

	void notManager();
	bool isManager();
	void setInited();
	bool isInited();
	NEMId getPid();

     	void processEventTDMA(const EventId &, const Serialization &);
      	void processTimedEvent(TimerEventId eventId,
                                         const TimePoint & expireTime,
                                         const TimePoint & scheduleTime,
                                         const TimePoint & fireTime,
                                         const void * arg);

	private:
	void sendSlotMap(TDMASlotMap & slotmap);
	void setDynTimer(std::uint16_t netid, std::uint32_t dynlen);

	PlatformServiceProvider *pPlatformService_;

      	bool isTdmaManager_;
      	bool isTdmaInited_;
	BuildId buildId_;
	std::string strUuid_;
	std::vector<TDMASlotMap> networks_;
	std::uint64_t slotBaseTime_;

	std::map<TimerEventId,std::uint16_t> dyn_send_timer_;
	std::set<TimerEventId> dyn_timer_;
	std::set<std::uint16_t> dyn_net_;

	std::auto_ptr<MACLayer> eventProxy_;
	NEMId proxyNid_;
    };

    }
  }
}


#endif
