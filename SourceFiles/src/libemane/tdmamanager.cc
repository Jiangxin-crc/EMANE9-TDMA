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

#include "tdmamanager.h"
#include <iostream>
#include <sstream>

namespace
{

std::vector<std::string> & splitstr(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> splitstr(const std::string &s, char delim) {
    std::vector<std::string> elems;
    splitstr(s, delim, elems);
    return elems;
}

}

EMANE::Application::TDMAManager::TDMAManager() :
 isTdmaManager_(true),
 isTdmaInited_(false),
 eventLock_{},
 dyn_send_timer_{},
 dyn_timer_{},
 dyn_net_{}
{
}
EMANE::Application::TDMAManager::~TDMAManager()
{
}

void EMANE::Application::TDMAManager::notManager()    { isTdmaManager_ = false;  }
bool EMANE::Application::TDMAManager::isManager()     { return isTdmaManager_;  }
void EMANE::Application::TDMAManager::setInited()     { isTdmaInited_ = true;   }
bool EMANE::Application::TDMAManager::isInited()      { return isTdmaInited_;   }

void EMANE::Application::TDMAManager::initialize(Registrar & registrar, BuildId bid)
{
  buildId_ = bid;
  EventServiceSingleton::instance()->registerEventServiceUser(bid,
                                                              this,
                                                              60000);

   auto & eventRegistrar = registrar.eventRegistrar();

   eventRegistrar.registerEvent(EMANE::Models::TDMA::TdmaREvent::IDENTIFIER);

}
void EMANE::Application::TDMAManager::sendInitEvent(std::string uuid)
{
     strUuid_ = uuid;
     EMANE::Models::TDMA::TdmaREvent event(0,EMANE::Models::TDMA::TDMA_TYPE_INIT,uuid,0,0,0);
     EMANE::EventServiceSingleton::instance()->sendEvent(
		buildId_,
		0,
		event);
}

void
EMANE::Application::TDMAManager::processTimedEvent(TimerEventId eventid,
                                                         const TimePoint &,
                                                         const TimePoint &,
                                                         const TimePoint &,
                                                         const void *)
{
  if (dyn_timer_.find(eventid)==dyn_timer_.end()) {
    if (!isTdmaInited_) {
      isTdmaInited_ = true;
      if (isTdmaManager_) {
	auto timeNow = Clock::now();
	slotBaseTime_ = (timeNow).time_since_epoch().count();
	// declear self as a manager
     	EMANE::Models::TDMA::TdmaREvent event(0,EMANE::Models::TDMA::TDMA_TYPE_NOTIFY,strUuid_,0,0,0);
     	EMANE::EventServiceSingleton::instance()->sendEvent(
		buildId_,
		0,
		event);
      }
    }
    LOGGER_STANDARD_LOGGING(*LogServiceSingleton::instance(),
	                          INFO_LEVEL,
	                          "TDMAManager::configure tdma init timer exec. Manager? %s",
	                          isTdmaManager_?"Yes":"No");
  }
  else {
     // dynamic send slot
     dyn_timer_.erase(eventid);
     if (dyn_send_timer_.find(eventid) != dyn_send_timer_.end()) {
   	std::uint16_t netid = dyn_send_timer_[eventid];
	dyn_send_timer_.erase(eventid);
	sendSlotMap(networks_[netid]);
	if (dyn_net_.find(netid) != dyn_net_.end())
	    dyn_net_.erase(netid);
     }
  }
}

void 
EMANE::Application::TDMAManager::sendSlotMap(TDMASlotMap & slotmap)
{
     	EMANE::Models::TDMA::TdmaBEvent event(slotBaseTime_,slotmap.getMap(),slotmap.getSubId());
     	EMANE::EventServiceSingleton::instance()->sendEvent(
		buildId_,
		0,
		event);
}

void
EMANE::Application::TDMAManager::processEvent(const EventId & eventId,
                                                    const Serialization & serialization)
{
   eventLock_.lock();
   LOGGER_STANDARD_LOGGING(*LogServiceSingleton::instance(),
	                          DEBUG_LEVEL,
	                          "TDMAManager::processEvent %s %hu",
	                          "event id: ",eventId);

  // check event id
  switch(eventId)
    {
    case EMANE::Models::TDMA::TdmaREvent::IDENTIFIER:
      {
	EMANE::Models::TDMA::TdmaREvent revent(serialization);
	if (isTdmaInited_) {
	    if (isTdmaManager_) {
		if (EMANE::Models::TDMA::TDMA_TYPE_NEMINIT == revent.getType()) {
		    // node request slot
		    NEMId nodeid = revent.getEventSource();
		    std::uint16_t subid = revent.getSubId();
		    std::uint16_t slotnum = revent.getSlotNum();
		    std::uint32_t slotlen = revent.getSlotLen();
		    std::string cfgstr = revent.getUuid(); // para reused
		    bool exist = false;
		    for (std::uint16_t i=0;i<networks_.size();i++) {
			    if (networks_[i].getSubId()==subid) {
				exist = true;
				if (networks_[i].getSlotNum() != slotnum) {
				    // error
   				    LOGGER_STANDARD_LOGGING(*LogServiceSingleton::instance(),
	                          	ERROR_LEVEL,
	                          	"TDMAManager::processEvent %s nodeid: %u subid: %u slot#: %u newSlot#: %u",
	                          	"slot num does not match: ",nodeid,subid,networks_[i].getSlotNum(),slotnum);
				}
				else {
				    if (cfgstr.length()>0) {
					std::vector<std::string> slots = splitstr(cfgstr,',');
				        for (std::uint16_t k=0;k<slots.size();k++) {
					   if (networks_[i].getOwner(atoi(slots[k].c_str())-1) == 0) {
					      networks_[i].setOwner(atoi(slots[k].c_str())-1,nodeid);
					   }
					   else {
						std::uint16_t nid = networks_[i].getOwner(atoi(slots[k].c_str())-1);
						if (nid!=nodeid)
						  // slot used, move it another empty one
				      		  for (int m=0;m<slotnum;m++) {
						   if (networks_[i].getOwner(m) == 0) {
					    	      networks_[i].setOwner(m,nid);
					   	       break;
						   }
				      		  }
					      networks_[i].setOwner(atoi(slots[k].c_str())-1,nodeid);
					   }
				        }
				    }
				    else {
				      for (int k=0;k<slotnum;k++) {
					if (networks_[i].getOwner(k) == 0) {
					    networks_[i].setOwner(k,nodeid);
					    break;
					}
				      }
				    }
				    sendSlotMap(networks_[i]);
				}
				break;
			    }
		    }
		    if (! exist) {
			TDMASlotMap slotmap(subid,slotnum);
			slotmap.setSlotLen(slotlen);
			networks_.push_back(slotmap);
			if (cfgstr.length()>0) {
				std::vector<std::string> slots = splitstr(cfgstr,',');
				for (std::uint16_t k=0;k<slots.size();k++) {
					if (networks_[0].getOwner(atoi(slots[k].c_str())-1) == 0) {
					      networks_[0].setOwner(atoi(slots[k].c_str())-1,nodeid);
					}
					else {
						std::uint16_t nid = networks_[0].getOwner(atoi(slots[k].c_str())-1);
						if (nid!=nodeid)
						  // slot used, move it another empty one
				      		  for (int m=0;m<slotnum;m++) {
						      if (networks_[0].getOwner(m) == 0) {
					    	          networks_[0].setOwner(m,nid);
					   	          break;
						      }
				      		  }
					          networks_[0].setOwner(atoi(slots[k].c_str())-1,nodeid);
					   }
				        }
				    }
			else {
				for (int k=0;k<slotnum;k++) {
					if (networks_[0].getOwner(k) == 0) {
					    networks_[0].setOwner(k,nodeid);
					    break;
				      	}
				}
			}
			sendSlotMap(slotmap);
		    }
		}
		else if (EMANE::Models::TDMA::TDMA_TYPE_FREE_SLOT == revent.getType()) {
		    NEMId nodeid = revent.getEventSource();
		    std::uint16_t subid = revent.getSubId();
		    std::uint32_t dynlen = revent.getSlotLen();  // dynamic overhead
		    for (std::uint16_t i=0;i<networks_.size();i++) {
			if (networks_[i].getSubId()==subid) {
			    networks_[i].free(nodeid);
			    if (networks_[i].needReassign()) {
				if (networks_[i].reassign(0,0))
				    setDynTimer(i,dynlen);
			    }
			    break;
			}
		    }
		}
		else if (EMANE::Models::TDMA::TDMA_TYPE_REQ_SLOT == revent.getType()) {
		    NEMId nodeid = revent.getEventSource();
		    std::uint16_t subid = revent.getSubId();
		    std::uint16_t slot = revent.getSlotNum();	// required slot
		    std::uint32_t dynlen = revent.getSlotLen();  // dynamic overhead
		    for (std::uint16_t i=0;i<networks_.size();i++) {
			if (networks_[i].getSubId()==subid) {
			    std::uint16_t usedslot = networks_[i].getUsedNum(nodeid);
			    networks_[i].setReqNum(nodeid,slot);
			    if (slot>usedslot) {
				if (networks_[i].reassign(slot-usedslot,nodeid))
				    setDynTimer(i,dynlen);
			    }
			    break;
			}
		    }
		}
		else {
		    // notify other
     		    EMANE::Models::TDMA::TdmaREvent event(0,EMANE::Models::TDMA::TDMA_TYPE_NOTIFY,strUuid_,0,0,0);
     		    EMANE::EventServiceSingleton::instance()->sendEvent(
		    		buildId_,
		    		0,
		    		event);
		}
	    }
	}
	else {
	    // during initial phase
	    if (isTdmaManager_) {
		switch (revent.getType())
		{
		case EMANE::Models::TDMA::TDMA_TYPE_NOTIFY:
		    // manager exist
		    isTdmaManager_ = false;
		    isTdmaInited_ = true;
		    break;
		case EMANE::Models::TDMA::TDMA_TYPE_INIT:
		    if (strUuid_.compare(revent.getUuid())>0) {
		       isTdmaManager_ = false;
		       isTdmaInited_ = true;
		    }
		    else {
     			EMANE::Models::TDMA::TdmaREvent event(0,EMANE::Models::TDMA::TDMA_TYPE_INIT,strUuid_,0,0,0);
     			EMANE::EventServiceSingleton::instance()->sendEvent(
				buildId_,
				0,
				event);
		    }
		    break;
		default:
		    break;
		}
	    }
	}
      }
      break;
    }

  // no other events to be handled
  eventLock_.unlock();
}

void 
EMANE::Application::TDMAManager::setDynTimer(std::uint16_t netid, std::uint32_t dynlen)
{
    if (dyn_net_.find(netid) != dyn_net_.end()) return;

    TDMASlotMap slot = networks_[netid];
    std::uint16_t slotnum = slot.getSlotNum();
    std::uint32_t slotlen = slot.getSlotLen();
    // calculate next cycle start time
    // get current time
    TimePoint timeNow{Clock::now()};

    std::uint64_t nowus = (timeNow).time_since_epoch().count()+10;
    std::uint64_t cycleid = (nowus - slotBaseTime_)/(dynlen+slotlen*slotnum);
    std::uint64_t nextcycle = slotBaseTime_+(cycleid+1)*(dynlen+slotlen*slotnum);

    dyn_net_.insert(netid);
    auto timenext = Microseconds(nextcycle-nowus);
    TimerEventId eventid = EMANE::TimerService::instance()->scheduleTimedEvent
		  	  	  (timeNow+timenext,
		  	  	  nullptr,
		  	  	  Microseconds(0),
		  	  	  this);
    dyn_send_timer_[eventid] = netid;
    dyn_timer_.insert(eventid);
}

//================================= TDMASlotMap ==========================================

EMANE::Application::TDMASlotMap::TDMASlotMap(std::uint16_t id, std::uint16_t num) :
	sub_id_(id),
	slot_a_cycle_(num),
	used_slot_{},
	avai_slot_{},
	reqed_slot_{},
	slot_length_(0),
	dynamic_(false)
{
  used_slot_.resize(num);
  for (int i=0;i<num;i++) used_slot_[i] = 0;
  avai_slot_.resize(num);
  for (int i=0;i<num;i++) avai_slot_[i] = 0;
}
EMANE::Application::TDMASlotMap::~TDMASlotMap() 
{
}

std::uint16_t 
EMANE::Application::TDMASlotMap::getUsedNum(EMANE::NEMId nodeid)
{
  std::uint16_t ret = 0;
  for (int i=0;i< slot_a_cycle_;i++) if (used_slot_[i] == nodeid) ret++;
  return ret;
}

void 
EMANE::Application::TDMASlotMap::free(EMANE::NEMId nodeid)
{
  for (int i=0;i< slot_a_cycle_;i++) {
    if (used_slot_[i] = nodeid) {
	avai_slot_[i] = nodeid;
    }
  }
}

bool 
EMANE::Application::TDMASlotMap::needReassign()
{
    bool ret = false;
    std::uint16_t used;
    for (std::map<EMANE::NEMId,std::uint16_t>::iterator it = reqed_slot_.begin(); it != reqed_slot_.end(); ++it) {
	used = getUsedNum(it->first);
	if (it->second>used) {
	    ret = true;
	    break;
	}
    }
    return ret;
}

bool 
EMANE::Application::TDMASlotMap::reassign(std::uint16_t need, EMANE::NEMId nodeid)
{
    std::uint16_t freeslot = 0;
    for (int i=0;i< slot_a_cycle_;i++) if (avai_slot_[i]>0 || used_slot_[i]==0) freeslot++;
    if (need>0 && nodeid!=0) {
	if (need<=freeslot) {
	    for (int k=0;k<need;k++) {
		for (int i=0;i< slot_a_cycle_;i++) {
		    if (avai_slot_[i]>0) {
			avai_slot_[i] = 0;  used_slot_[i] = nodeid;  break;
		    } else if(used_slot_[i]==0) {
			 used_slot_[i] = nodeid;   break;
		    }
		}
	    }
	    return true;
	}
    }
    // complete re-assign
    size_t nodenum = reqed_slot_.size();
    EMANE::NEMId *nlist = new EMANE::NEMId[nodenum];
    std::uint16_t *reqlist = new std::uint16_t[nodenum];
    int x = 0;
    for (std::map<EMANE::NEMId,std::uint16_t>::iterator it = reqed_slot_.begin(); it != reqed_slot_.end(); ++it) {
	nlist[x] = it->first;
	reqlist[x] = it->second;
	x++;
    }
    for (int i=0;i<slot_a_cycle_;i++) { used_slot_[i] = 0; avai_slot_[i] = 0; }
    int used = 0;
    for (std::uint16_t k=0;k<slot_a_cycle_;k++) {
	for (std::uint16_t i=0;i<nodenum;i++) {
	    if (reqlist[i]>0) {
	    	used_slot_[used] = nlist[i];
	    	reqlist[i]--;
		used++;
		if (used==slot_a_cycle_) break;
	    }
	}
	if (used==slot_a_cycle_) break;
    }
    delete nlist;
    delete reqlist;
    return true;
}

void 
EMANE::Application::TDMASlotMap::setReqNum(EMANE::NEMId nodeid,std::uint16_t reqnum)
{
    reqed_slot_[nodeid] = reqnum;
}


std::uint16_t 
EMANE::Application::TDMASlotMap::getSubId() 
{
	return sub_id_;
}

void 
EMANE::Application::TDMASlotMap::setSlotLen(std::uint32_t len)
{
	slot_length_ = len;
	dynamic_ = len>0;
}

std::uint16_t 
EMANE::Application::TDMASlotMap::getSlotNum() 
{
	return slot_a_cycle_;
}

std::uint32_t 
EMANE::Application::TDMASlotMap::getSlotLen() 
{
	return slot_length_;
}

EMANE::NEMId 
EMANE::Application::TDMASlotMap::getOwner(std::uint16_t slot)
{
	return used_slot_[slot];
}

void 
EMANE::Application::TDMASlotMap::setOwner(std::uint16_t slot, EMANE::NEMId owner)
{
	used_slot_[slot] = owner;
}
std::vector<EMANE::NEMId> & 
EMANE::Application::TDMASlotMap::getMap()
{
	return used_slot_;
}

