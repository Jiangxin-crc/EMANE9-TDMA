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

#ifndef TDMAMAC_MACLAYER_HEADER_
#define TDMAMAC_MACLAYER_HEADER_

#include "emane/maclayerimpl.h"
#include "emane/mactypes.h"
#include "emane/flowcontrolmanager.h"
#include "emane/neighbormetricmanager.h"
#include "emane/queuemetricmanager.h"
#include "emane/statisticnumeric.h"
#include "emane/events/location.h"
#include "emane/events/locationevent.h"
#include "emane/position.h"
#include "emane/orientation.h"
#include "emane/velocity.h"

#include "emane/utils/runningaverage.h"
#include "emane/utils/randomnumberdistribution.h"
#include "emane/utils/commonlayerstatistics.h"

#include "tdmaevent.h"
#include "downstreamqueue.h"
#include "pcrmanager.h"
#include "fragmentmgr.h"

#include <memory>
#include <netinet/ip.h>
#include "tdmamanager.h"

#include <iostream>
#include <ctime>

namespace EMANE
{
  namespace Models
  {
    namespace TDMA
    {
      /**
       *
       * @class MACLayer
       *
       * @brief Implementation of the tdma mac layer.
       *
       */

      class MACLayer : public MACLayerImplementor
      {
      public:
        /**
         * constructor
         *
         * @param id this NEM id.
         * @param pPlatformServiceProvider reference to the platform service provider
         * @param pRadioServiceProvider reference to the radio service provider
         *
         */
        MACLayer(NEMId id,
                 PlatformServiceProvider *pPlatformServiceProvider,
                 RadioServiceProvider * pRadioServiceProvider);

        /**
         *
         * destructor
         *
         */
        ~MACLayer();


        // mac layer implementor api below
  
        void initialize(Registrar & registrar) override;
        void configure(const ConfigurationUpdate & update) override;
        void start() override;
        void postStart() override;
        void stop() override;
        void destroy() throw() override;
        void processUpstreamControl(const ControlMessages & msgs) override;
        void processUpstreamPacket(const CommonMACHeader & hdr,
                                   UpstreamPacket & pkt,
                                   const ControlMessages & msgs) override;  
        void processDownstreamControl(const ControlMessages & msgs) override;
        void processDownstreamPacket(DownstreamPacket & pkt,
                                     const ControlMessages & msgs) override;
        void processEvent(const EventId &, const Serialization &);
        void processTimedEvent(TimerEventId eventId,
                               const TimePoint & expireTime,
                               const TimePoint & scheduleTime,
                               const TimePoint & fireTime,
                               const void * arg) override;
        void processConfiguration(const ConfigurationUpdate & update) override;

        void proxyEvent (NEMId from, NEMId nemId, Event &event);

      private:
        /**
         *
         * @brief  the emane tdma registration id
         *
         */
        static const RegistrationId type_ = REGISTERED_EMANE_MAC_TDMA;

        std::uint64_t 		u64TxSequenceNumber_;
        FlowControlManager 	flowControlManager_;
        PCRManager		pcrManager_;
        NeighborMetricManager 	neighborMetricManager_;
        QueueMetricManager 	queueMetricManager_;
        StatisticNumeric<std::uint64_t> * pNumDownstreamQueueDelay_;
        TimerEventId radioMetricTimedEventId_;
        Utils::CommonLayerStatistics 		commonLayerStatistics_;
        Utils::RandomNumberDistribution<std::mt19937, 
                                 std::uniform_real_distribution<float>> RNDZeroToOne_;
        std::unique_ptr<Utils::RandomNumberDistribution<std::mt19937, 
                                 std::uniform_real_distribution<float>>> pRNDJitter_;
        TimerEventId downstreamQueueTimedEventId_;
        bool 		bHasPendingDownstreamQueueEntry_;
        DownstreamQueueEntry pendingDownstreamQueueEntry_;

        Utils::RunningAverage<float>  avgDownstreamQueueDelay_;
        DownstreamQueue downstreamQueue_;

	FragmentManager	fragmentManager_;
        TimerEventId nemREventId_;

	bool		tdmaReady_;
	bool		dynamic_;
	std::uint64_t	tdmaBaseTime_;
  	char 		priority_[64];
	NEMId		slot_map_[256];
	std::uint64_t	begin_send_;
	std::uint64_t	slot_send_;
	std::uint8_t	sequence_;
	std::vector<std::uint64_t> dataratebps_;
  	size_t   	timeslotByte_;   

	std::uint16_t	lastReqSlotNum_;
	std::uint16_t	usedSlotNum_;
	std::uint64_t	last_dyn_cycid_;

	std::mutex eventLock_;
	std::mutex mgrLock_;

        // config items
        bool 		bPromiscuousMode_;
	std::uint8_t	datarate_;
        float 		fJitterSeconds_;
        Microseconds 	delayMicroseconds_;
        bool 		bFlowControlEnable_;
        std::uint16_t 	u16FlowControlTokens_;
        bool 		bRadioMetricEnable_;
        std::string 	sPCRCurveURI_;
        Microseconds 	radioMetricReportIntervalMicroseconds_;
        Microseconds 	neighborMetricDeleteTimeMicroseconds_;
  	bool 		bQosEnable_;
	std::string     qos_map_str_;
  	bool            fragmentationEnable_;
  	bool            aggregationEnable_;
  	bool		sendatbeginning_;
	std::uint16_t   macsubid_;
	Microseconds  	timeSlotLength_;
	std::uint64_t	timeSlotLen_;
	Microseconds  	guardTime_; 
	std::uint16_t   slotNumInCycle_;
	std::string     slot_map_str_;
	std::uint16_t   macheaderlen_;
	std::uint16_t   payloadadjustlen_;
	Microseconds  	dynamicLength_;
	std::uint64_t  	dynamicLen_;

	// functions

	bool sendInitRevent();
	std::uint16_t getDataRateIndex(std::uint64_t recvRatebps);
	std::uint64_t getDataRate(std::uint8_t rateIdx);
	size_t getTimeByte(std::uint64_t sendRatebps, EMANE::Microseconds tvLeftTime);
	size_t getPktSize(EMANE::DownstreamPacket & pkt,std::uint8_t fragflag);
	void splitPkt(int totallen, Utils::VectorIO vio, void *part1, int part1len, void *part2);
	int getSynSlotNum();
	bool dynamicSlot(TimePoint any);
	void setQoS();

        bool handleDownstreamQueueEntry(TimePoint sot);  
        Microseconds getDurationMicroseconds(size_t lengthInBytes, std::uint64_t sendRatebps);
        Microseconds getJitter();
        bool checkPOR(float fSINR, size_t packetSize, std::uint16_t dataRateIndex);

      };
    }
  }
}

#endif //TDMAMAC_MACLAYER_HEADER_

