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

#define  _GLIBCXX_USE_NANOSLEEP

#include "maclayer.h"
#include "tdmamacheadermessage.h"

#include "emane/controls/flowcontrolcontrolmessage.h"
#include "emane/controls/receivepropertiescontrolmessage.h"
#include "emane/controls/frequencycontrolmessage.h"
#include "emane/controls/serializedcontrolmessage.h"
#include "emane/controls/r2riselfmetriccontrolmessage.h"
#include "emane/controls/receivepropertiescontrolmessageformatter.h"
#include "emane/controls/frequencycontrolmessageformatter.h"
#include "emane/controls/timestampcontrolmessage.h"

#include "emane/spectrumserviceexception.h"
#include "emane/configureexception.h"
#include "emane/utils/conversionutils.h"
#include "emane/utils/spectrumwindowutils.h"

#include <sstream>

namespace
{
  const char * pzLayerName{"TdmaMACLayer"};

  const std::uint16_t DROP_CODE_SINR               = 1;
  const std::uint16_t DROP_CODE_REGISTRATION_ID    = 2;
  const std::uint16_t DROP_CODE_DST_MAC            = 3;
  const std::uint16_t DROP_CODE_QUEUE_OVERFLOW     = 4;
  const std::uint16_t DROP_CODE_BAD_CONTROL_INFO   = 5;
  const std::uint16_t DROP_CODE_BAD_SPECTRUM_QUERY = 6;
  const std::uint16_t DROP_CODE_FLOW_CONTROL_ERROR = 7;
  const std::uint16_t DROP_CODE_NOT_READY 	   = 8;
  const std::uint16_t DROP_CODE_TOO_BIG 	   = 9;

  EMANE::StatisticTableLabels STATISTIC_TABLE_LABELS 
  {
    "SINR",
      "Reg Id",
      "Dst MAC",
      "Queue Overflow",
      "Bad Control",
      "Bad Spectrum Query",
      "Flow Control",
      "Not Ready",
      "Packet too Big"
      };

std::vector<std::string> & splitstring(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> splitstring(const std::string &s, char delim) {
    std::vector<std::string> elems;
    splitstring(s, delim, elems);
    return elems;
}

void getPktBuf(std::vector<iovec> vio, unsigned char *buf)
{
   size_t m=0;
   for (size_t k=0;k<vio.size();k++) {
 	      memcpy(buf+m,vio[k].iov_base,vio[k].iov_len);
	      m =+ vio[k].iov_len;
   }
}

}

EMANE::Models::TDMA::MACLayer::MACLayer(NEMId id,
                                          PlatformServiceProvider * pPlatformServiceProvider,
                                          RadioServiceProvider * pRadioServiceProvider):
  MACLayerImplementor{id, pPlatformServiceProvider, pRadioServiceProvider},
  u64TxSequenceNumber_{},
  flowControlManager_{*this},
  pcrManager_(id, pPlatformService_),
  neighborMetricManager_(id),
  queueMetricManager_(id),
  pNumDownstreamQueueDelay_{},
  radioMetricTimedEventId_{},
  commonLayerStatistics_{STATISTIC_TABLE_LABELS,{},"0"},
  RNDZeroToOne_{0.0f, 1.0f},
  pRNDJitter_{},
  downstreamQueueTimedEventId_{},
  bHasPendingDownstreamQueueEntry_{},
  pendingDownstreamQueueEntry_{},
  fragmentManager_{id,pPlatformServiceProvider},
  tdmaReady_(false),
  dynamic_(false),
  begin_send_(0),
  slot_send_(0),
  lastReqSlotNum_(0),
  usedSlotNum_(0),
  last_dyn_cycid_(0),
  fJitterSeconds_{},
  slot_map_str_{""}
{}

EMANE::Models::TDMA::MACLayer::~MACLayer(){}


void 
EMANE::Models::TDMA::MACLayer::initialize(Registrar & registrar)
{
  LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                          DEBUG_LEVEL,
                          "MACI %03hu %s::%s", 
                          id_,
                          pzLayerName,
                          __func__);


  auto & configRegistrar = registrar.configurationRegistrar();

  configRegistrar.registerNumeric<bool>("enablepromiscuousmode",
                                        ConfigurationProperties::DEFAULT |
                                        ConfigurationProperties::MODIFIABLE,
                                        {false},
                                        "Defines whether promiscuous mode is enabled or not."
                                        " If promiscuous mode is enabled, all received packets"
                                        " (intended for the given node or not) that pass the"
                                        " probability of reception check are sent upstream to"
                                        " the transport.");

  configRegistrar.registerNumeric<std::uint8_t>("datarate",
                                                 ConfigurationProperties::DEFAULT |
                                                 ConfigurationProperties::MODIFIABLE,
                                                 {1},
                                                 "Defines the transmit datarate by index of dataratelist."
                                                 " The datarate is used by the transmitter"
                                                 " to compute the transmit delay (packet size/datarate)"
                                                 " between successive transmissions.",
                                                 1);

  configRegistrar.registerNonNumeric<std::string>("dataratelist",
                                                 ConfigurationProperties::REQUIRED,
                                                 {},
                                                 "Defines the transmit datarate list in bps."
                                                 " The datarate is used by the transmitter"
                                                 " to compute the transmit delay (packet size/datarate)"
                                                 " between successive transmissions.");


  configRegistrar.registerNumeric<float>("jitter",
                                         ConfigurationProperties::DEFAULT |
                                         ConfigurationProperties::MODIFIABLE,
                                         {0},
                                         "Defines delay jitter in seconds applied to each transmitted packet."
                                         " The jitter is added to the configured delay based on a uniform"
                                         " random distribution between +/- the configured jitter value.",
                                         0.0f);

  configRegistrar.registerNumeric<float>("delay",
                                         ConfigurationProperties::DEFAULT |
                                         ConfigurationProperties::MODIFIABLE,
                                         {0},
                                         "Defines an additional fixed delay in seconds applied to each"
                                         " transmitted packet.",
                                         0.0f);

  configRegistrar.registerNumeric<bool>("flowcontrolenable",
                                        ConfigurationProperties::DEFAULT,
                                        {false},
                                        "Defines whether flow control is enabled. Flow control only works"
                                        " with the virtual transport and the setting must match the setting"
                                        " within the virtual transport configuration.");

  configRegistrar.registerNumeric<std::uint16_t>("flowcontroltokens",
                                                 ConfigurationProperties::DEFAULT,
                                                 {10},
                                                 "Defines the maximum number of flow control tokens"
                                                 " (packet transmission units) that can be processed from the"
                                                 " virtual transport without being refreshed. The number of"
                                                 " available tokens at any given time is coordinated with the"
                                                 " virtual transport and when the token count reaches zero, no"
                                                 " further packets are transmitted causing application socket"
                                                 " queues to backup.");

  configRegistrar.registerNonNumeric<std::string>("pcrcurveuri",
                                                  ConfigurationProperties::REQUIRED,
                                                  {},
                                                  "Defines the absolute URI of the Packet Completion Rate (PCR) curve"
                                                  " file. The PCR curve file contains probability of reception curves"
                                                  " as a function of Signal to Interference plus Noise Ratio (SINR).");

  configRegistrar.registerNumeric<bool>("radiometricenable",
                                        ConfigurationProperties::DEFAULT,
                                        {false},
                                        "Defines if radio metrics will be reported up via the Radio to Router Interface"
                                        " (R2RI).");

           
  configRegistrar.registerNumeric<float>("radiometricreportinterval",
                                         ConfigurationProperties::DEFAULT,
                                         {1.0f},
                                         "Defines the metric report interval in seconds in support of the R2RI feature.",
                                         0.1f,
                                         60.0f);

  configRegistrar.registerNumeric<float>("neighbormetricdeletetime",
                                         ConfigurationProperties::DEFAULT |
                                         ConfigurationProperties::MODIFIABLE,
                                         {60.0f},
                                         "Defines the time in seconds of no RF receptions from a given neighbor"
                                         " before it is removed from the neighbor table.",
                                         1.0f,
                                         3660.0f);

 
  configRegistrar.registerNumeric<bool>("aggregationenable",
                                        ConfigurationProperties::DEFAULT |
                                         ConfigurationProperties::MODIFIABLE,
                                        {false},
                                        "Defines aggregation feature."
                                        );


  configRegistrar.registerNumeric<bool>("fragmentationenable",
                                        ConfigurationProperties::DEFAULT |
                                         ConfigurationProperties::MODIFIABLE,
                                        {false},
                                        "Defines fragmentation feature."
                                        );

  configRegistrar.registerNumeric<bool>("sendonlyatbegin",
                                        ConfigurationProperties::DEFAULT |
                                         ConfigurationProperties::MODIFIABLE,
                                        {false},
                                        "Defines when can send."
                                        );

   configRegistrar.registerNumeric<bool>("priorityqos",
                                        ConfigurationProperties::DEFAULT |
                                         ConfigurationProperties::MODIFIABLE,
                                        {false},
                                        "Defines QoS feature."
                                        );

  configRegistrar.registerNonNumeric<std::string>("qossetting",
                                                  ConfigurationProperties::DEFAULT |
                                         	  ConfigurationProperties::MODIFIABLE,
                                                  {""},
                                                  "Defines the absolute values for high priority.");

  configRegistrar.registerNumeric<std::uint16_t>("macsubid",
                                                 ConfigurationProperties::REQUIRED,
                                                 {1},
                                                 "Defines id of wireless network.");

  configRegistrar.registerNumeric<std::uint32_t>("timeslotlength",
                                                 ConfigurationProperties::REQUIRED,
                                                 {50000},
                                                 "Defines length of a time slot (microseconds).",
						 5000,
						 100000000);

  configRegistrar.registerNumeric<std::uint32_t>("guardtime",
                                                 ConfigurationProperties::REQUIRED,
                                                 {500},
                                                 "Defines length of guard time (microseconds).",
						 100,
						 1000000);

  configRegistrar.registerNumeric<std::uint16_t>("timeslotnum",
                                                 ConfigurationProperties::REQUIRED,
                                                 {5},
                                                 "Defines how many time slots in a cycle.",
						 2,20000);

  configRegistrar.registerNonNumeric<std::string>("slotmapping",
                                                  ConfigurationProperties::DEFAULT,
                                                  {""},
                                                  "Defines the slot mapping for a node or a network.");

  configRegistrar.registerNumeric<std::uint16_t>("macheaderlen",
                                         ConfigurationProperties::DEFAULT,
                                         {3},
                                         "Defines emulated MAC protocol header length (bytes).",
                                         3,
                                         200);

  configRegistrar.registerNumeric<std::uint16_t>("payloadlenadj",
                                         ConfigurationProperties::DEFAULT,
                                         {0},
                                         "Defines size of payload length adjustment (bytes)."
					 "  set 20 to simulated no IP.",
                                         0,
                                         100);

  configRegistrar.registerNumeric<std::uint32_t>("dynamiclength",
                                                 ConfigurationProperties::DEFAULT,
                                                 {0},
                                                 "Defines length of dynamic slot overhead (microseconds per cycle).",
						 0,
						 1000000);


  auto & statisticRegistrar = registrar.statisticRegistrar();

  commonLayerStatistics_.registerStatistics(statisticRegistrar);

  downstreamQueue_.registerStatistics(statisticRegistrar);

  pNumDownstreamQueueDelay_ =
      statisticRegistrar.registerNumeric<std::uint64_t>("numDownstreamQueueDelay",
                                                  StatisticProperties::CLEARABLE);

  avgDownstreamQueueDelay_.registerStatistic(
      statisticRegistrar.registerNumeric<float>("avgDownstreamQueueDelay",
                                                  StatisticProperties::CLEARABLE));

  neighborMetricManager_.registerStatistics(statisticRegistrar);

   auto & eventRegistrar = registrar.eventRegistrar();
   eventRegistrar.registerEvent(EMANE::Models::TDMA::TdmaBEvent::IDENTIFIER);

}

void 
EMANE::Models::TDMA::MACLayer::configure(const ConfigurationUpdate & update)
{
  LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                          DEBUG_LEVEL,
                          "MACI %03hu %s::%s", 
                          id_, 
                          pzLayerName, 
                          __func__);

  for(const auto & item : update)
    {
      if(item.first == "enablepromiscuousmode")
        {
          /** [logservice-infolog-snippet] */  
          bPromiscuousMode_ = item.second[0].asBool();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL, 
                                  "MACI %03hu %s::%s %s = %s", 
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  bPromiscuousMode_ ? "on" : "off");
          /** [logservice-infolog-snippet] */  
        }
      else if(item.first == "datarate")
        {
          datarate_ = item.second[0].asUINT8();
             
          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %u",
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  datarate_);
        }
      else if(item.first == "dataratelist")
        {
	  std::string drlist = item.second[0].asString();
 		  try {
    		        std::vector<std::string> vecstr = splitstring(drlist,',');
			for (size_t k=0;k<vecstr.size();k++) {
			   if (vecstr[k].back()=='M') {
				vecstr[k].back() = ' ';
				std::uint64_t dr = atoi(vecstr[k].c_str());
				dataratebps_.push_back(dr*1000000);
			   }
			   else if (vecstr[k].back()=='K') {
				vecstr[k].back() = ' ';
				std::uint64_t dr = atoi(vecstr[k].c_str());
				dataratebps_.push_back(dr*1000);
			   }
			   else if (vecstr[k].back()>='0' && vecstr[k].back()<='9') {
				std::uint64_t dr = atoi(vecstr[k].c_str());
				dataratebps_.push_back(dr);
			   }
			}
		  } catch (std::exception& e)
		     {
		     }
            
          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %s     rate# %d",
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  drlist.c_str(),dataratebps_.size());
        }
      else if(item.first == "jitter")
        {
          fJitterSeconds_ = item.second[0].asFloat();

          if(fJitterSeconds_ > 0.0f)
            {
              // create a random mumber distrubtion +- the jitter range
              pRNDJitter_.reset(new Utils::RandomNumberDistribution<std::mt19937, 
                                std::uniform_real_distribution<float>>
                                (-fJitterSeconds_ / 2.0f, fJitterSeconds_ / 2.0));
            }

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %f", 
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  fJitterSeconds_);
        }
      else if(item.first == "delay")
        {
          delayMicroseconds_ = 
            std::chrono::duration_cast<Microseconds>(DoubleSeconds{item.second[0].asFloat()});

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %lf", 
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  std::chrono::duration_cast<DoubleSeconds>(delayMicroseconds_).count());
          
        }
      else if(item.first == "flowcontrolenable")
        {
          bFlowControlEnable_ = item.second[0].asBool();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %s", 
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  bFlowControlEnable_ ? "on" : "off");
        }
      else if(item.first == "flowcontroltokens")
        {
          u16FlowControlTokens_ = item.second[0].asUINT16();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %hu",
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  u16FlowControlTokens_);
        }
      else if(item.first == "pcrcurveuri")
        {
          sPCRCurveURI_ = item.second[0].asString();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %s",
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(),
                                  sPCRCurveURI_.c_str());
        }
      else if(item.first == "radiometricenable")
        {
          bRadioMetricEnable_ = item.second[0].asBool();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %s",
                                  id_,
                                  pzLayerName,
                                  __func__,
                                  item.first.c_str(),
                                  bRadioMetricEnable_ ? "on" : "off");
        }
      else if(item.first == "radiometricreportinterval")
        {
          radioMetricReportIntervalMicroseconds_ =
            std::chrono::duration_cast<Microseconds>(DoubleSeconds{item.second[0].asFloat()});

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %lf",
                                  id_,
                                  pzLayerName,
                                  __func__,
                                  item.first.c_str(),
                                  std::chrono::duration_cast<DoubleSeconds>(radioMetricReportIntervalMicroseconds_).count());
        }
      else if(item.first == "neighbormetricdeletetime")
        {
          neighborMetricDeleteTimeMicroseconds_ =
            std::chrono::duration_cast<Microseconds>(DoubleSeconds{item.second[0].asFloat()});

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %lf",
                                  id_,
                                  pzLayerName,
                                  __func__,
                                  item.first.c_str(),
                                  std::chrono::duration_cast<DoubleSeconds>(neighborMetricDeleteTimeMicroseconds_).count());
        }
      else if(item.first == "aggregationenable")
        {
          aggregationEnable_ = item.second[0].asBool();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %s", 
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  aggregationEnable_ ? "on" : "off");
        }
      else if(item.first == "fragmentationenable")
        {
          fragmentationEnable_ = item.second[0].asBool();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %s", 
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  fragmentationEnable_ ? "on" : "off");
        }
      else if(item.first == "sendonlyatbegin")
        {
          sendatbeginning_ = item.second[0].asBool();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %s", 
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  sendatbeginning_ ? "on" : "off");
        }
      else if(item.first == "priorityqos")
        {
          bQosEnable_ = item.second[0].asBool();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %s", 
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  bQosEnable_ ? "on" : "off");
        }
      else if(item.first == "qossetting")
        {
          qos_map_str_ = item.second[0].asString();
	  setQoS();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %s",
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(),
                                  qos_map_str_.c_str());
        }
      else if(item.first == "macsubid")
        {
          macsubid_ = item.second[0].asUINT16();
             
          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %hu",
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  macsubid_);
        }
      else if(item.first == "timeslotlength")
        {
          timeSlotLength_ =
            Microseconds(item.second[0].asUINT32());
	  timeSlotLen_ = std::chrono::duration_cast<Microseconds>(timeSlotLength_).count();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %ju usec",
                                  id_,
                                  pzLayerName,
                                  __func__,
                                  item.first.c_str(),
                                  std::chrono::duration_cast<Microseconds>(timeSlotLength_).count());
        }
      else if(item.first == "guardtime")
        {
          guardTime_ =
            Microseconds(item.second[0].asUINT32());

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %ju usec",
                                  id_,
                                  pzLayerName,
                                  __func__,
                                  item.first.c_str(),
                                  std::chrono::duration_cast<Microseconds>(guardTime_).count());
        }
      else if(item.first == "dynamiclength")
        {
          dynamicLen_ = item.second[0].asUINT32();
	  if (dynamicLen_>0 && dynamicLen_<1000) dynamicLen_ = 1000;
	  dynamicLength_ = Microseconds(dynamicLen_);
	  dynamic_ = dynamicLen_>0;

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %ju usec",
                                  id_,
                                  pzLayerName,
                                  __func__,
                                  item.first.c_str(),
                                  std::chrono::duration_cast<Microseconds>(dynamicLength_).count());
        }
      else if(item.first == "timeslotnum")
        {
          slotNumInCycle_ = item.second[0].asUINT16();
             
          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %hu",
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  slotNumInCycle_);
        }
      else if(item.first == "slotmapping")
        {
          slot_map_str_ = item.second[0].asString();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %s",
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(),
                                  slot_map_str_.c_str());
        }
      else if(item.first == "macheaderlen")
        {
          macheaderlen_ = item.second[0].asUINT16();
             
          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %hu bytes",
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  macheaderlen_);
        }
      else if(item.first == "payloadlenadj")
        {
          payloadadjustlen_ = item.second[0].asUINT16()+14;	// remove 14 bytes ethernet header
             
          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %hu bytes",
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  payloadadjustlen_);
        }
      else
        {
          throw makeException<ConfigureException>("Tdma::MACLayer: "
                                                  "Unexpected configuration item %s",
                                                  item.first.c_str());
        }
    }
}

void 
EMANE::Models::TDMA::MACLayer::setQoS()
{
  for (int k=0;k<64;k++) priority_[k] = 0;
  try {
     if (qos_map_str_.find_first_of(';')!=std::string::npos) {
        std::vector<std::string> vecstrpri = splitstring(qos_map_str_,';');
	for (size_t j=0;j<vecstrpri.size();j++) {
            if (vecstrpri[j].find_first_of(',')!=std::string::npos) {
   	    	std::vector<std::string> vecstr = splitstring(vecstrpri[j],',');
	    	for (size_t k=0;k<vecstr.size();k++) {
		    if (vecstr[k].find_first_of('-')!=std::string::npos) {
	                 std::vector<std::string> vecstr2 = splitstring(vecstr[k],'-');
	    		 int s = atoi(vecstr2[0].c_str());
	    		 int e = atoi(vecstr2[1].c_str());
 		 	 for (int i=s;i<=e;i++) priority_[i] = vecstrpri.size()-j;    		        
		    }
		    else priority_[atoi(vecstr[k].c_str())] = vecstrpri.size()-j;
	    	}
     	    }
            else {
		        if (vecstrpri[j].find_first_of('-')!=std::string::npos) {
    		           std::vector<std::string> vecstr = splitstring(vecstrpri[j],'-');
	    		   int s = atoi(vecstr[0].c_str());
	    		   int e = atoi(vecstr[1].c_str());
 		 	   for (int i=s;i<=e;i++) priority_[i] = vecstrpri.size()-j;    		        
			}
			else priority_[atoi(qos_map_str_.c_str())] = vecstrpri.size()-j;
 	    }
	}
     }
     else {
         if (qos_map_str_.find_first_of(',')!=std::string::npos) {
    		        std::vector<std::string> vecstr = splitstring(qos_map_str_,',');
			for (size_t k=0;k<vecstr.size();k++) {
		           if (vecstr[k].find_first_of('-')!=std::string::npos) {
    		              std::vector<std::string> vecstr2 = splitstring(vecstr[k],'-');
	    		      int s = atoi(vecstr2[0].c_str());
	    		      int e = atoi(vecstr2[1].c_str());
 		 	      for (int i=s;i<=e;i++) priority_[i] = 1;    		        
			   }
			   else priority_[atoi(vecstr[k].c_str())] = 1;
			}
          }
	  else {
		        if (qos_map_str_.find_first_of('-')!=std::string::npos) {
    		           std::vector<std::string> vecstr = splitstring(qos_map_str_,'-');
	    		   int s = atoi(vecstr[0].c_str());
	    		   int e = atoi(vecstr[1].c_str());
 		 	   for (int i=s;i<=e;i++) priority_[i] = 1;    		        
			}
			else priority_[atoi(qos_map_str_.c_str())] = 1;
 	   }
     }
  } catch (std::exception& e)
		     {
		     }
}

void 
EMANE::Models::TDMA::MACLayer::start()
{
  LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                          DEBUG_LEVEL,
                          "MACI %03hu %s::%s", 
                          id_, 
                          pzLayerName, 
                          __func__);
  // load pcr curve
  pcrManager_.load(sPCRCurveURI_);
  
  // set the neighbor delete time
  neighborMetricManager_.setNeighborDeleteTimeMicroseconds(neighborMetricDeleteTimeMicroseconds_);

  // add downstream queue to be tracked
  queueMetricManager_.addQueueMetric(0, downstreamQueue_.getMaxCapacity());

}


void 
EMANE::Models::TDMA::MACLayer::postStart()
{
  LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                          DEBUG_LEVEL,
                          "MACI %03hu %s::%s", 
                          id_,
                          pzLayerName,
                          __func__);

  // check flow control enabled 
  if(bFlowControlEnable_)
    {
      // start flow control 
      flowControlManager_.start(u16FlowControlTokens_);

      LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                              DEBUG_LEVEL,
                              "MACI %03hu %s::%s sent a flow control token update,"
                              " a handshake response is required to process packets",
                              id_, 
                              pzLayerName, 
                              __func__);
    }

  // set the timer timeout (absolute time), arg, interval
  radioMetricTimedEventId_ = 
    pPlatformService_->timerService().
      scheduleTimedEvent(Clock::now() + radioMetricReportIntervalMicroseconds_,
                         new std::function<bool()>{[this]()
                             {
                               if(!bRadioMetricEnable_)
                                 {
                                   neighborMetricManager_.updateNeighborStatus();
                                 }
                               else
                                 {
                                   ControlMessages msgs{
                                       Controls::R2RISelfMetricControlMessage::create(getDataRate(datarate_),
                                                                                      getDataRate(datarate_),
                                                                                      radioMetricReportIntervalMicroseconds_),
                                       Controls::R2RINeighborMetricControlMessage::create(neighborMetricManager_.getNeighborMetrics()),
                                       Controls::R2RIQueueMetricControlMessage::create(queueMetricManager_.getQueueMetrics())};

                                    sendUpstreamControl(msgs);
                                 }

                                return false;
                             }},
                         radioMetricReportIntervalMicroseconds_);

  LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                          DEBUG_LEVEL,
                          "MACI %03hu %s::%s: added radio metric timed eventId %zu", 
                          id_, 
                          pzLayerName,
                          __func__,
                          radioMetricTimedEventId_);

  timeslotByte_ = getDataRate(datarate_)*(timeSlotLength_-guardTime_).count()/1000000/8;

  // send request event to get TDMA info
   auto timeNow = Clock::now();
   auto time1S = Microseconds(1300000);
   nemREventId_ = pPlatformService_->timerService().
        scheduleTimedEvent(timeNow+time1S,
                           new std::function<bool()>{std::bind(&MACLayer::sendInitRevent,
                                                                   this)});
}

bool 
EMANE::Models::TDMA::MACLayer::sendInitRevent()
{
  std::string smap = std::string{""};
  if (slot_map_str_.length()>0) {
      if (slot_map_str_.front()=='+') {
	std::vector<std::string> slots = splitstring(slot_map_str_.substr(1),';');
	for (size_t k=0;k<slots.size();k++) {
	   std::vector<std::string> ss = splitstring(slots[k],'=');
	   if (id_ == atoi(ss[0].c_str())) {
		smap = ss[1];
		break;
	   }
	}
      }
      else {
	smap = slot_map_str_;
      }
  }
  EMANE::Models::TDMA::TdmaREvent event(id_,EMANE::Models::TDMA::TDMA_TYPE_NEMINIT,smap,macsubid_,slotNumInCycle_,dynamic_?timeSlotLen_:0);
  pPlatformService_->eventService().sendEvent(0,event);
  nemREventId_ = 0;
  LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                          DEBUG_LEVEL,
                          "MACI %03hu %s::%s: TDMA INIT event sent", 
                          id_, 
                          pzLayerName,
                          __func__
                          );
  return true;
}

void 
EMANE::Models::TDMA::MACLayer::processConfiguration(const ConfigurationUpdate & update)
{
  LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                          DEBUG_LEVEL,
                          "MACI %03hu %s::%s", 
                          id_, 
                          pzLayerName, 
                          __func__);
  
  for(const auto & item : update)
    {
      if(item.first == "enablepromiscuousmode")
        {
          bPromiscuousMode_ = item.second[0].asBool();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  DEBUG_LEVEL, 
                                  "MACI %03hu %s::%s %s = %s", 
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  bPromiscuousMode_ ? "on" : "off");
          
        }
      else if(item.first == "datarate")
        {
          datarate_ = item.second[0].asUINT8();
	  timeslotByte_ = getDataRate(datarate_)*(timeSlotLength_-guardTime_).count()/1000000/8;
             
          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  DEBUG_LEVEL,
                                  "MACI %03hu %s::%s %s = %u",
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  datarate_);
        }
      else if(item.first == "jitter")
        {
          fJitterSeconds_ = item.second[0].asFloat();

          if(fJitterSeconds_ > 0.0f)
            {
              // create a random mumber distrubtion +- the jitter range
              pRNDJitter_.reset(new Utils::RandomNumberDistribution<std::mt19937, 
                                std::uniform_real_distribution<float>>
                                (-fJitterSeconds_ / 2.0f, fJitterSeconds_ / 2.0));
            }

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  DEBUG_LEVEL,
                                  "MACI %03hu %s::%s %s = %f", 
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  fJitterSeconds_);
        }
      else if(item.first == "delay")
        {
          delayMicroseconds_ = 
            std::chrono::duration_cast<Microseconds>(DoubleSeconds{item.second[0].asFloat()});

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  DEBUG_LEVEL,
                                  "MACI %03hu %s::%s %s = %lf", 
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  std::chrono::duration_cast<DoubleSeconds>(delayMicroseconds_).count());
          
        }
      else if(item.first == "neighbormetricdeletetime")
        {
          neighborMetricDeleteTimeMicroseconds_ =
            std::chrono::duration_cast<Microseconds>(DoubleSeconds{item.second[0].asFloat()});

          // set the neighbor delete time
          neighborMetricManager_.setNeighborDeleteTimeMicroseconds(neighborMetricDeleteTimeMicroseconds_);

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                                  DEBUG_LEVEL,
                                  "MACI %03hu %s::%s %s = %lf",
                                  id_,
                                  pzLayerName,
                                  __func__,
                                  item.first.c_str(),
                                  std::chrono::duration_cast<DoubleSeconds>(neighborMetricDeleteTimeMicroseconds_).count());
        }
      else if(item.first == "aggregationenable")
        {
          aggregationEnable_ = item.second[0].asBool();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %s", 
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  aggregationEnable_ ? "on" : "off");
        }
      else if(item.first == "fragmentationenable")
        {
          fragmentationEnable_ = item.second[0].asBool();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %s", 
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  fragmentationEnable_ ? "on" : "off");
        }
      else if(item.first == "sendonlyatbegin")
        {
          sendatbeginning_ = item.second[0].asBool();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %s", 
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  sendatbeginning_ ? "on" : "off");
        }
      else if(item.first == "priorityqos")
        {
          bQosEnable_ = item.second[0].asBool();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %s", 
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(), 
                                  bQosEnable_ ? "on" : "off");
        }
      else if(item.first == "qossetting")
        {
          qos_map_str_ = item.second[0].asString();
	  setQoS();

          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                  INFO_LEVEL,
                                  "MACI %03hu %s::%s %s = %s",
                                  id_, 
                                  pzLayerName, 
                                  __func__, 
                                  item.first.c_str(),
                                  qos_map_str_.c_str());
        }
      else
        {
          throw makeException<ConfigureException>("Tdma::MACLayer: "
                                                  "Unexpected configuration item %s",
                                                  item.first.c_str());
        }
    }
}



void 
EMANE::Models::TDMA::MACLayer::stop()
{
  LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                          DEBUG_LEVEL,
                          "MACI %03hu %s::%s", 
                          id_,
                          pzLayerName,
                          __func__);

  pPlatformService_->timerService().cancelTimedEvent(downstreamQueueTimedEventId_);

  downstreamQueueTimedEventId_ = 0;

  // check flow control enabled
  if(bFlowControlEnable_)
    {
      // stop the flow control manager
      flowControlManager_.stop();
    }
}



void 
EMANE::Models::TDMA::MACLayer::destroy()
  throw()
{
  LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                          DEBUG_LEVEL,
                          "MACI %03hu %s::%s", 
                          id_,
                          pzLayerName,
                          __func__);
}


void 
EMANE::Models::TDMA::MACLayer::processUpstreamControl(const ControlMessages &){}


void 
EMANE::Models::TDMA::MACLayer::processDownstreamControl(const ControlMessages & msgs)
{
  for(const auto & pMessage : msgs)
    {
      LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                              DEBUG_LEVEL,
                              "MACI %03hu %s::%s downstream control message id %hu", 
                              id_, 
                              pzLayerName,
                              __func__,
                              pMessage->getId());

      switch(pMessage->getId())
        {
        case Controls::FlowControlControlMessage::IDENTIFIER:
          {
            const auto pFlowControlControlMessage =
              static_cast<const Controls::FlowControlControlMessage *>(pMessage);

            if(bFlowControlEnable_)
              {
                LOGGER_STANDARD_LOGGING(pPlatformService_->logService(), 
                                        DEBUG_LEVEL,
                                        "MACI %03hu %s::%s received a flow control token request/response",
                                        id_, 
                                        pzLayerName, 
                                        __func__);

                flowControlManager_.processFlowControlMessage(pFlowControlControlMessage);
              }
            else
              {
                LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                                        ERROR_LEVEL,
                                        "MACI %03hu %s::%s received a flow control token request but"
                                        " flow control is not enabled", 
                                        id_,
                                        pzLayerName,
                                        __func__);
              }
          }
          break;
          
          // [serializedcontrolmessage-flowcontrol-snibbet] /
        case Controls::SerializedControlMessage::IDENTIFIER:
          {
            const auto pSerializedControlMessage =
              static_cast<const Controls::SerializedControlMessage *>(pMessage); 
        
            switch(pSerializedControlMessage->getSerializedId())
              {
              case Controls::FlowControlControlMessage::IDENTIFIER:
                {
                  std::unique_ptr<Controls::FlowControlControlMessage> 
                    pFlowControlControlMessage{
                    Controls::FlowControlControlMessage::create(pSerializedControlMessage->getSerialization())};
                  
                  if(bFlowControlEnable_)
                    {
                      flowControlManager_.processFlowControlMessage(pFlowControlControlMessage.get());
                    }
                  else
                    {
                      LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                                              ERROR_LEVEL,
                                              "MACI %03hu %s::%s received a flow control token request but"
                                              " flow control is not enabled", 
                                              id_,
                                              pzLayerName,
                                              __func__);
                    }
                }
                break;
              }
          }
          // [serializedcontrolmessage-flowcontrol-snibbet] /
        }
    }
}


void 
EMANE::Models::TDMA::MACLayer::processUpstreamPacket(const CommonMACHeader & commonMACHeader,
                                                       UpstreamPacket & pkt,
                                                       const ControlMessages & msgs)
{
  // get current time
  TimePoint beginTime{Clock::now()};

  commonLayerStatistics_.processInbound(pkt);

  if(commonMACHeader.getRegistrationId() != type_)
    {
      LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                              ERROR_LEVEL, 
                              "MACI %03hu %s::%s: MAC Registration Id %hu does not match our Id %hu, drop.",
                              id_, 
                              pzLayerName, 
                              __func__, 
                              commonMACHeader.getRegistrationId(), 
                              type_);

      commonLayerStatistics_.processOutbound(pkt, 
                                             std::chrono::duration_cast<Microseconds>(Clock::now() - beginTime), 
                                             DROP_CODE_REGISTRATION_ID);
      
      // drop
      return;
    }

  size_t len{pkt.stripLengthPrefixFraming()};

  const PacketInfo & pktInfo{pkt.getPacketInfo()};

  if(len && pkt.length() >= len)
    {
      MACHeaderMessage tdmaMACHeader{pkt.get(), len};

      pkt.strip(len);

      const Controls::ReceivePropertiesControlMessage * pReceivePropertiesControlMessage{};

      const Controls::FrequencyControlMessage * pFrequencyControlMessage{};
      
      for(auto & pControlMessage : msgs)
        {
          switch(pControlMessage->getId())
            {
            case EMANE::Controls::ReceivePropertiesControlMessage::IDENTIFIER:
              {
                // [logservice-loggerfnvargs-snippet] /  
                pReceivePropertiesControlMessage =
                  static_cast<const Controls::ReceivePropertiesControlMessage *>(pControlMessage); 

                LOGGER_VERBOSE_LOGGING_FN_VARGS(pPlatformService_->logService(),
                                                DEBUG_LEVEL,
                                                Controls::ReceivePropertiesControlMessageFormatter(pReceivePropertiesControlMessage),
                                                "MACI %03hu Tdma::%s Receiver Properties Control Message",
                                                id_,
                                                __func__);
                // [logservice-loggerfnvargs-snippet] /  
              }
              break;
              
            case Controls::FrequencyControlMessage::IDENTIFIER:
              {
                pFrequencyControlMessage =
                  static_cast<const Controls::FrequencyControlMessage *>(pControlMessage); 

                LOGGER_VERBOSE_LOGGING_FN_VARGS(pPlatformService_->logService(),
                                                DEBUG_LEVEL,
                                                Controls::FrequencyControlMessageFormatter(pFrequencyControlMessage),
                                                "MACI %03hu Tdma::%s Frequency Control Message",
                                                id_,
                                                __func__);
                  
              }
                
              break;
            }
        }

     
      if(!pReceivePropertiesControlMessage || !pFrequencyControlMessage || 
         pFrequencyControlMessage->getFrequencySegments().empty())
        {
          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                                  ERROR_LEVEL,
                                  "MACI %03hu %s::%s: phy control "
                                  "message not provided from src %hu, drop",
                                  id_,
                                  pzLayerName,
                                  __func__,
                                  pktInfo.getSource());
      
          commonLayerStatistics_.processOutbound(pkt, 
                                                 std::chrono::duration_cast<Microseconds>(Clock::now() - beginTime), 
                                                 DROP_CODE_BAD_CONTROL_INFO);

          // drop
          return;
        }
      else
        {
          const auto & frequencySegments = pFrequencyControlMessage->getFrequencySegments();

          // [startofreception-calculation-snibbet] /
          TimePoint startOfReception{pReceivePropertiesControlMessage->getTxTime() +
              pReceivePropertiesControlMessage->getPropagationDelay() +
              frequencySegments.begin()->getOffset()};
          // [startofreception-calculation-snibbet] /

          Microseconds span{pReceivePropertiesControlMessage->getSpan()};
            
          auto pCallback =
            new std::function<bool()>(std::bind([this,
                                                 startOfReception,
                                                 frequencySegments,
                                                 span,
                                                 beginTime](UpstreamPacket & pkt,
                                                            std::uint64_t u64SequenceNumber,
                                                            std::uint64_t u64DataRate,
							    std::uint8_t sequence, std::uint8_t fragment, std::uint8_t datarate, std::uint8_t len)
            {
              const PacketInfo & pktInfo{pkt.getPacketInfo()};
              
              const FrequencySegment & frequencySegment{*frequencySegments.begin()};
              
              LOGGER_VERBOSE_LOGGING(pPlatformService_->logService(),
                                     DEBUG_LEVEL,
                                     "MACI %03hu %s upstream EOR processing: src %hu, dst %hu,"
                                     " len %zu, freq %ju, offset %ju, duration %ju, mac sequence %ju",
                                     id_,
                                     pzLayerName,
                                     pktInfo.getSource(),
                                     pktInfo.getDestination(),
                                     pkt.length(),
                                     frequencySegment.getFrequencyHz(),
                                     frequencySegment.getOffset().count(),
                                     frequencySegment.getDuration().count(),
                                     u64SequenceNumber);
              
              
              double dSINR{};
             
              double dNoiseFloordB{};


              try
                {
                  // [spectrumservice-request-snibbet] /
                  // get the spectrum info for the entire span, where a span
                  // is the total time between the start of the signal of the
                  // earliest segment and the end of the signal of the latest
                  // segment. This is not necessarily the signal duration.
                  auto window = pRadioService_->spectrumService().request(frequencySegment.getFrequencyHz(),
                                                                          span,
                                                                          startOfReception);

                  // since we only have a single segment the span will equal the segment duration.
                  // For simple noise processing we will just pull out the max noise segment, we can
                  // use the maxBinNoiseFloor utility function for this. More elaborate noise window analysis
                  // will require a more complex algorithm, although you should get a lot of mileage out of 
                  // this utility function.
                  bool bSignalInNoise{};

                  std::tie(dNoiseFloordB,bSignalInNoise) =
                    Utils::maxBinNoiseFloor(window,frequencySegment.getRxPowerdBm());
                  
                  dSINR = frequencySegment.getRxPowerdBm() - dNoiseFloordB;
                  // [spectrumservice-request-snibbet] /

                  LOGGER_VERBOSE_LOGGING(pPlatformService_->logService(),
                                         DEBUG_LEVEL,
                                         "MACI %03hu %s upstream EOR processing: src %hu, dst %hu, max noise %f, signal in noise %s, SINR %f",
                                         id_,
                                         pzLayerName,
                                         pktInfo.getSource(),
                                         pktInfo.getDestination(),
                                         dNoiseFloordB,
                                         bSignalInNoise ? "yes" : "no",
                                         dSINR);
                }
              catch(SpectrumServiceException & exp)
                {
                  LOGGER_VERBOSE_LOGGING(pPlatformService_->logService(),
                                         ERROR_LEVEL,
                                         "MACI %03hu %s upstream EOR processing: src %hu, dst %hu, sor %ju, span %ju spectrum service request error: %s",
                                         id_,
                                         pzLayerName,
                                         pktInfo.getSource(),
                                         pktInfo.getDestination(),
                                         std::chrono::duration_cast<Microseconds>(startOfReception.time_since_epoch()).count(),
                                         span.count(),
                                         exp.what());
                  
                  commonLayerStatistics_.processOutbound(pkt, 
                                                         std::chrono::duration_cast<Microseconds>(Clock::now() - beginTime), 
                                                         DROP_CODE_BAD_SPECTRUM_QUERY);
                  // drop
                  return true;
                }

              const Microseconds & durationMicroseconds{frequencySegment.getDuration()};
              
              // check sinr
              if(!checkPOR(dSINR, pkt.length(),1))
                {
                  LOGGER_VERBOSE_LOGGING(pPlatformService_->logService(),
                                         DEBUG_LEVEL,
                                         "MACI %03hu %s upstream EOR processing: src %hu, dst %hu, "
                                         "rxpwr %3.2f dBm, drop",
                                         id_,
                                         pzLayerName,
                                         pktInfo.getSource(),
                                         pktInfo.getDestination(),
                                         frequencySegment.getRxPowerdBm());
                  
                  commonLayerStatistics_.processOutbound(pkt, 
                                                         std::chrono::duration_cast<Microseconds>(Clock::now() - beginTime), 
                                                         DROP_CODE_SINR);
                  
                  // drop
                  return true;
                }
              
              // update neighbor metrics 
              neighborMetricManager_.updateNeighborRxMetric(pktInfo.getSource(),    // nbr (src)
                                                            u64SequenceNumber,      // sequence number
                                                            pktInfo.getUUID(),
                                                            dSINR,                  // sinr in dBm
                                                            dNoiseFloordB,          // noise floor in dB
                                                            startOfReception,       // rx time
                                                            durationMicroseconds,   // duration
                                                            u64DataRate);           // data rate bps
             
              // check promiscuous mode, destination is this nem or to all nem's
              if(bPromiscuousMode_ ||
                 (pktInfo.getDestination() == id_) ||
                 (pktInfo.getDestination() == NEM_BROADCAST_MAC_ADDRESS))
                {
                  LOGGER_VERBOSE_LOGGING(pPlatformService_->logService(),
                                         DEBUG_LEVEL,
                                         "MACI %03hu %s upstream EOR processing: src %hu, dst %hu, forward upstream   len %d",
                                         id_,
                                         pzLayerName,
                                         pktInfo.getSource(),
                                         pktInfo.getDestination(),
					 pkt.length());
                  
                  commonLayerStatistics_.processOutbound(pkt,
                                                         std::chrono::duration_cast<Microseconds>(Clock::now() - beginTime));
                  
		  MACHeaderMessage tdmaMACHeader(sequence,fragment,datarate,len);
                  if (tdmaMACHeader.isFragment()) {
                  	LOGGER_VERBOSE_LOGGING(pPlatformService_->logService(),
                                         DEBUG_LEVEL,
                                         "MACI FRAG %03hu %s origin %hu, dst %hu, len %zu fseq: %d",
                                         id_,
                                         pzLayerName,
                                         pktInfo.getSource(),
                                         pktInfo.getDestination(),
					 pkt.length(),
					 tdmaMACHeader.getFlag());
			struct MacHeader mh;
			mh.sequence = tdmaMACHeader.getSequence(); mh.fragflag = tdmaMACHeader.getFlag(); 
			mh.datarate = tdmaMACHeader.getDataRate(); mh.len = tdmaMACHeader.getLen();
			EMANE::UpstreamPacket fpkt = fragmentManager_.process(pkt,pkt.getPacketInfo(),&mh);
			if (fpkt.length()>0) {
	   			sendUpstreamPacket(fpkt);
			}
		  }
		  else {
                  	sendUpstreamPacket(pkt);
                  }
                  
                  // done
                  return true;
                }
              else
                {
                  LOGGER_VERBOSE_LOGGING(pPlatformService_->logService(),
                                         DEBUG_LEVEL,
                                         "MACI %03hu %s upstream EOR processing: not for this nem, "
                                         "ignore pkt src %hu, dst %hu, drop",
                                         id_,
                                         pzLayerName,
                                         pktInfo.getSource(),
                                         pktInfo.getDestination());
                  
                  commonLayerStatistics_.processOutbound(pkt, 
                                                         std::chrono::duration_cast<Microseconds>(Clock::now() - beginTime), 
                                                         DROP_CODE_DST_MAC);
                  
                  // drop 
                  return true;
                }
            },pkt,commonMACHeader.getSequenceNumber(),getDataRate(tdmaMACHeader.getDataRate()),tdmaMACHeader.getSequence(),tdmaMACHeader.getFlag(),tdmaMACHeader.getDataRate(),tdmaMACHeader.getLen()));


          auto eor = startOfReception + frequencySegments.begin()->getDuration();

          if(eor > beginTime)
            {
              // wait for end of reception to complete processing
              pPlatformService_->timerService().scheduleTimedEvent(eor,pCallback);
            }
          else
            {
              // we can process now, end of reception has past
              (*pCallback)();
              
              delete pCallback;
            }
        }
    }

}
                                    



void 
EMANE::Models::TDMA::MACLayer::processDownstreamPacket(DownstreamPacket & pkt,
                                                         const ControlMessages &)
{
  TimePoint beginTime{Clock::now()};
  commonLayerStatistics_.processInbound(pkt);

  if (!tdmaReady_) 
        {
          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                                  ERROR_LEVEL,
                                  "MACI %03hu %s::%s: tdma not ready, drop packet",
                                  id_,
                                  pzLayerName,
                                  __func__);
          
          commonLayerStatistics_.processOutbound(pkt, 
                                                 std::chrono::duration_cast<Microseconds>(Clock::now() - beginTime), 
                                                 DROP_CODE_NOT_READY);

          // drop
          return;
        }

  // check flow control
  if(bFlowControlEnable_)
    {
      if(flowControlManager_.removeToken() == false)
        {
          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                                  ERROR_LEVEL,
                                  "MACI %03hu %s::%s: failed to remove token, drop packet",
                                  id_,
                                  pzLayerName,
                                  __func__);
          
          commonLayerStatistics_.processOutbound(pkt, 
                                                 std::chrono::duration_cast<Microseconds>(Clock::now() - beginTime), 
                                                 DROP_CODE_FLOW_CONTROL_ERROR);

          // drop
          return;
        }
    }

  std::uint8_t priority = 0;
  if (bQosEnable_) {
         struct iphdr		ipHeader;
	 unsigned char* buffer = new unsigned char[pkt.length()];
	 getPktBuf(pkt.getVectorIO(),buffer);
	 memcpy((void *)&ipHeader,(void *)(buffer+14),sizeof(ipHeader));
	priority = priority_[ipHeader.tos&IPTOS_TOS_MASK]>0?0:1;
	delete buffer;
  }


  // get duration
  Microseconds durationMicroseconds{getDurationMicroseconds(pkt.length(),getDataRate(datarate_))};
 
  DownstreamQueueEntry entry{pkt,                   // pkt
      u64TxSequenceNumber_,  // sequence number
      beginTime,             // acquire time
      durationMicroseconds,  // duration
      getDataRate(datarate_),       // data rate
      sequence_,0,datarate_,(std::uint8_t)macheaderlen_,
      priority
      };
  
  sequence_++;
  ++u64TxSequenceNumber_;
  
  if(bHasPendingDownstreamQueueEntry_)
    {
      std::vector<DownstreamQueueEntry> result{downstreamQueue_.enqueue(entry)};

      // check for discarded, update stats
      for(auto & iter : result)
        {
          commonLayerStatistics_.processOutbound(iter.pkt_, 
                                                 std::chrono::duration_cast<Microseconds>(Clock::now() - iter.acquireTime_), 
                                                 DROP_CODE_QUEUE_OVERFLOW);
          
          // drop, replace token
          if(bFlowControlEnable_)
            {
              flowControlManager_.addToken();
            }
        }
    }
  else
    {
      bHasPendingDownstreamQueueEntry_ = true;

      pendingDownstreamQueueEntry_ = std::move(entry);

      // delay and jitter is applied on the transmit side prior to
      // sending the packet to the PHY for transmission. This is a change
      // from previous version of the mac
      Microseconds txDelay{delayMicroseconds_ + getJitter()};

      TimePoint sot{Clock::now() + txDelay};

      if(txDelay > Microseconds::zero())
        {

          downstreamQueueTimedEventId_ = 
            pPlatformService_->timerService().
            scheduleTimedEvent(sot,
                               new std::function<bool()>{std::bind(&MACLayer::handleDownstreamQueueEntry,
                                                                   this,
                                                                   sot)});

        }
      else
        {
          handleDownstreamQueueEntry(sot);
        }
    }
}

int 
EMANE::Models::TDMA::MACLayer::getSynSlotNum()
{
   size_t queuelen = downstreamQueue_.getCurrentDepth();
   int ret = (bHasPendingDownstreamQueueEntry_||(queuelen>0))?1:0;
   if (ret>0) {
      ret = queuelen/5;
      if (aggregationEnable_) ret = ret/2;
      if (ret>(slotNumInCycle_/2+1)) ret = slotNumInCycle_/2+1;
      if (ret==0) ret = 1;
   }
   if (ret == lastReqSlotNum_) return -1;	// no req required
   lastReqSlotNum_ = ret;
   return ret;
}

bool 
EMANE::Models::TDMA::MACLayer::dynamicSlot(TimePoint)
{
    int needslotn = getSynSlotNum();
    if (needslotn>=0) {
      if (needslotn>0) {
	EMANE::Models::TDMA::TdmaREvent event(id_,EMANE::Models::TDMA::TDMA_TYPE_REQ_SLOT,
		"",macsubid_,needslotn,dynamicLen_);
	pPlatformService_->eventService().sendEvent(0,event);
      } else {
	EMANE::Models::TDMA::TdmaREvent event(id_,EMANE::Models::TDMA::TDMA_TYPE_FREE_SLOT,
		"",macsubid_,slotNumInCycle_,dynamicLen_);
	pPlatformService_->eventService().sendEvent(0,event);
      }
    }
    return true;
}

bool 
EMANE::Models::TDMA::MACLayer::handleDownstreamQueueEntry(TimePoint sot)
{
  // previous end-of-transmission time
  TimePoint now = Clock::now();

  // if not the owner of current timeslot, wait to next timeslot
  std::uint64_t nowus = (now).time_since_epoch().count()+50;
  std::uint64_t cycleid = (nowus - tdmaBaseTime_)/(dynamicLen_+timeSlotLen_*slotNumInCycle_);
  std::uint64_t timeincycle = (nowus - tdmaBaseTime_)%(dynamicLen_+timeSlotLen_*slotNumInCycle_);

  if (dynamic_ && timeincycle < dynamicLen_) {
    dynamicSlot(now);
    std::chrono::microseconds tobegin(dynamicLen_-timeincycle+10);
    TimePoint nextry{now + tobegin};
          downstreamQueueTimedEventId_ = 
            pPlatformService_->timerService().
            scheduleTimedEvent(nextry,
                               new std::function<bool()>{std::bind(&MACLayer::handleDownstreamQueueEntry,
                                                                   this,
                                                                   nextry)});
    return true;
  }

  int currSlotId = (timeincycle-dynamicLen_)%slotNumInCycle_;
  std::uint64_t slotid = cycleid*slotNumInCycle_+currSlotId;	


  std::uint64_t tonextus = cycleid*(dynamicLen_+timeSlotLen_*slotNumInCycle_)+timeSlotLen_*(currSlotId+1) - (nowus-tdmaBaseTime_);

  std::chrono::microseconds tonext(tonextus);
  if (id_ != slot_map_[currSlotId] || (sendatbeginning_ && begin_send_ == slotid) || (slot_send_ == slotid)) {
    TimePoint nextry{now + tonext};

          downstreamQueueTimedEventId_ = 
            pPlatformService_->timerService().
            scheduleTimedEvent(nextry,
                               new std::function<bool()>{std::bind(&MACLayer::handleDownstreamQueueEntry,
                                                                   this,
                                                                   nextry)});
    return true;
  }
  bool first_in_slot = begin_send_ != slotid;
  begin_send_ = slotid;

  if(bHasPendingDownstreamQueueEntry_)
    {
      if(bFlowControlEnable_)
        {
          flowControlManager_.addToken();
        }

      // check higher priority packet
      if (downstreamQueue_.getCurrentDepth() > 0) {
	DownstreamQueueEntry pke = downstreamQueue_.peek();
	if (pke.u8Priority_<pendingDownstreamQueueEntry_.u8Priority_) {
	    std::swap(pke,pendingDownstreamQueueEntry_);
	    std::tie(pendingDownstreamQueueEntry_,
                        bHasPendingDownstreamQueueEntry_) =
                        downstreamQueue_.dequeue();
	    downstreamQueue_.enqueue_front(pke);
	}
      }

      MACHeaderMessage mac(pendingDownstreamQueueEntry_.sequence_,pendingDownstreamQueueEntry_.fragflag_,
			pendingDownstreamQueueEntry_.datarate_,pendingDownstreamQueueEntry_.len_);

      size_t pktsize = getPktSize(pendingDownstreamQueueEntry_.pkt_,pendingDownstreamQueueEntry_.fragflag_);

      // fragmentation check
      if (fragmentationEnable_) {
	// ready to send with fragmentation
	Microseconds tvAva {first_in_slot?(timeSlotLen_):(tonextus)};
	Microseconds duration = getDurationMicroseconds(1+macheaderlen_,getDataRate(pendingDownstreamQueueEntry_.datarate_));
	if (tvAva < guardTime_ || duration > (tvAva - guardTime_)) {
	    // not enough time
	    slot_send_ = slotid;
    	    TimePoint nextry{now + tonext};
            downstreamQueueTimedEventId_ = 
            	pPlatformService_->timerService().
            		scheduleTimedEvent(nextry,
                               new std::function<bool()>{std::bind(&MACLayer::handleDownstreamQueueEntry,
                                                                   this,
                                                                   nextry)});
	    return true;
	}
	else {
	   size_t maxavabyte = getTimeByte(getDataRate(datarate_),(tvAva - guardTime_));
	   if (maxavabyte>timeslotByte_) maxavabyte = timeslotByte_;
	   if (mac.isFragment() || maxavabyte < pktsize+macheaderlen_) {
		// do fragmentation
		
		bool firstTime = mac.isFragment()==false;
		size_t x = (pendingDownstreamQueueEntry_.pkt_.length()>payloadadjustlen_)?(pendingDownstreamQueueEntry_.pkt_.length()-payloadadjustlen_):0;
		size_t realsize = (size_t) (firstTime?x:pendingDownstreamQueueEntry_.pkt_.length());
		if (realsize<=(maxavabyte-macheaderlen_)) {
	   	    // no more fragmentation
	   	    if (mac.isFragment()) {
			mac.setLast();
			pendingDownstreamQueueEntry_.fragflag_ = mac.getFlag();
	   	    }
		}
		else {
		    // split packet, put rest in the front for queue

          	    EMANE::PacketInfo info = pendingDownstreamQueueEntry_.pkt_.getPacketInfo();

	  	    size_t newpktsize = firstTime?maxavabyte-macheaderlen_+payloadadjustlen_:maxavabyte-macheaderlen_;
		    size_t restsize = pendingDownstreamQueueEntry_.pkt_.length() - newpktsize;
	  	    // for new packet
          	    unsigned char * buffer1 = new unsigned char [newpktsize];
          	    unsigned char * buffer2 = new unsigned char [restsize];
		    splitPkt(newpktsize+restsize,pendingDownstreamQueueEntry_.pkt_.getVectorIO() , buffer1, newpktsize, buffer2);
		    EMANE::DownstreamPacket newPkt1(info, buffer1, newpktsize);
		    EMANE::DownstreamPacket newPkt2(info, buffer2, restsize);
		    mac.incFrag();
  		    DownstreamQueueEntry entry1{newPkt1,                   // pkt
      			pendingDownstreamQueueEntry_.u64SequenceNumber_,  // sequence number
      			pendingDownstreamQueueEntry_.acquireTime_,             // acquire time
      			getDurationMicroseconds(firstTime?newpktsize-payloadadjustlen_:newpktsize,getDataRate(datarate_)),  // duration
      			pendingDownstreamQueueEntry_.u64DataRatebps_,       // data rate
      			pendingDownstreamQueueEntry_.sequence_,mac.getFlag(),pendingDownstreamQueueEntry_.datarate_,(std::uint8_t)macheaderlen_,
      			pendingDownstreamQueueEntry_.u8Priority_
      			};
  		    DownstreamQueueEntry entry2{newPkt2,                   // pkt
      			pendingDownstreamQueueEntry_.u64SequenceNumber_,  // sequence number
      			pendingDownstreamQueueEntry_.acquireTime_,             // acquire time
      			getDurationMicroseconds(restsize+macheaderlen_,getDataRate(datarate_)),  // duration
      			pendingDownstreamQueueEntry_.u64DataRatebps_,       // data rate
      			pendingDownstreamQueueEntry_.sequence_,mac.getFlag(),pendingDownstreamQueueEntry_.datarate_,(std::uint8_t)macheaderlen_,
      			pendingDownstreamQueueEntry_.u8Priority_
      			};
		    downstreamQueue_.enqueue_front(entry2);
		    pendingDownstreamQueueEntry_ = std::move(entry1);
          	    delete [] buffer1;
          	    delete [] buffer2;
		}
	    // go to send pendingDownstreamQueueEntry_

	   }
	   // else send the fragment
	}
      }
      // size check
      else if (pktsize+macheaderlen_ > timeslotByte_) {
          LOGGER_STANDARD_LOGGING(pPlatformService_->logService(),
                                  ERROR_LEVEL,
                                  "MACI %03hu %s::%s: packet too big! pkt size %d slot size %d",
                                  id_,
                                  pzLayerName,
                                  __func__,
				  pktsize+macheaderlen_,timeslotByte_);
          auto & pkt = pendingDownstreamQueueEntry_.pkt_;
          commonLayerStatistics_.processOutbound(pkt, 
                                                 std::chrono::duration_cast<Microseconds>(Clock::now() - pendingDownstreamQueueEntry_.acquireTime_), 
                                                 DROP_CODE_TOO_BIG);

          std::tie(pendingDownstreamQueueEntry_,   bHasPendingDownstreamQueueEntry_) = downstreamQueue_.dequeue();
          TimePoint nextry{now + std::chrono::microseconds{10}};
          downstreamQueueTimedEventId_ = 
            pPlatformService_->timerService().
            scheduleTimedEvent(nextry,
                               new std::function<bool()>{std::bind(&MACLayer::handleDownstreamQueueEntry,
                                                                   this,
                                                                   nextry)});

	  if (first_in_slot) begin_send_--;  // try next as first in slot
          // drop
          return true;
      }
      // check time 
      else {
	Microseconds tvAva {first_in_slot?(timeSlotLen_):(tonextus)};
	Microseconds duration = getDurationMicroseconds(pktsize+macheaderlen_,getDataRate(pendingDownstreamQueueEntry_.datarate_));
        pendingDownstreamQueueEntry_.durationMicroseconds_ = duration;

	if (tvAva < guardTime_ || duration > (tvAva - guardTime_)) {
	    // not enough time
	    slot_send_ = slotid;
    	    TimePoint nextry{now + tonext};
            downstreamQueueTimedEventId_ = 
            	pPlatformService_->timerService().
            		scheduleTimedEvent(nextry,
                               new std::function<bool()>{std::bind(&MACLayer::handleDownstreamQueueEntry,
                                                                   this,
                                                                   nextry)});
	    return true;
	}
      }

      Serialization serialization{mac.serialize()};

      auto & pkt = pendingDownstreamQueueEntry_.pkt_;
      
      // prepend mac header to outgoing packet
       pkt.prepend(serialization.c_str(), serialization.size());

       // next prepend the serialization length
       pkt.prependLengthPrefixFraming(serialization.size());
       
       commonLayerStatistics_.processOutbound(pkt, 
                                              std::chrono::duration_cast<Microseconds>(now - pendingDownstreamQueueEntry_.acquireTime_));

       sendDownstreamPacket(CommonMACHeader(type_, pendingDownstreamQueueEntry_.u64SequenceNumber_), 
                            pkt,
                            {Controls::FrequencyControlMessage::create(0,                                   // bandwidth (0 means use phy default)
                                                                       {{0, pendingDownstreamQueueEntry_.durationMicroseconds_}}), // freq (0 means use phy default)
                                Controls::TimeStampControlMessage::create(sot)});
       
      if ( ! aggregationEnable_ )
	    slot_send_ = slotid;

      // queue delay
      Microseconds queueDelayMicroseconds{std::chrono::duration_cast<Microseconds>(now - sot)}; 
      
      *pNumDownstreamQueueDelay_ += queueDelayMicroseconds.count();
      
      avgDownstreamQueueDelay_.update(queueDelayMicroseconds.count());
      
      queueMetricManager_.updateQueueMetric(0,                                      // queue id, (we only have 1 queue)
                                            downstreamQueue_.getMaxCapacity(),      // queue size
                                            downstreamQueue_.getCurrentDepth(),     // queue depth
                                            downstreamQueue_.getNumDiscards(true),  // get queue discards and clear counter
                                            queueDelayMicroseconds);                // queue delay 
          
      neighborMetricManager_.updateNeighborTxMetric(pendingDownstreamQueueEntry_.pkt_.getPacketInfo().getDestination(),
                                                    pendingDownstreamQueueEntry_.u64DataRatebps_, 
                                                    now);
      

      auto eor = sot + pendingDownstreamQueueEntry_.durationMicroseconds_;

      auto pCallback = new std::function<bool()>{[this]()
                                                 {
                                                   std::tie(pendingDownstreamQueueEntry_,
                                                            bHasPendingDownstreamQueueEntry_) =
                                                   downstreamQueue_.dequeue();

                                                   if(bHasPendingDownstreamQueueEntry_)
                                                     {
                                                       Microseconds txDelay{delayMicroseconds_ + getJitter()};

                                                       TimePoint sot{Clock::now() + txDelay};
                                                       
                                                       if(txDelay > Microseconds::zero())
                                                         {
                                                           downstreamQueueTimedEventId_ = 
                                                             pPlatformService_->timerService().
                                                             scheduleTimedEvent(sot,
                                                                                new std::function<bool()>{std::bind(&MACLayer::handleDownstreamQueueEntry,
                                                                                                                    this,
                                                                                                                    sot)});
                                                         }
                                                       else
                                                         {
                                                           handleDownstreamQueueEntry(sot);
                                                         }
                                                     }
						     else {
							// no more pkt to send
							if (dynamic_) {
  							    TimePoint nowx = Clock::now();
							    std::uint64_t nowus = (nowx).time_since_epoch().count();
							    std::uint64_t cycleid = (nowus - tdmaBaseTime_)/(dynamicLen_+timeSlotLen_*slotNumInCycle_);
							    if (last_dyn_cycid_ != cycleid) {
								last_dyn_cycid_ = cycleid;
								std::uint64_t nextcycle = tdmaBaseTime_+(cycleid+1)*(dynamicLen_+timeSlotLen_*slotNumInCycle_);
								auto timenext = Microseconds(nextcycle-nowus);
								pPlatformService_->timerService().
                                                        	     scheduleTimedEvent(nowx+timenext,
                                                                                new std::function<bool()>{std::bind(&MACLayer::dynamicSlot,
                                                                                                                    this, nowx)});
							    }
							}
						     }

                                                   return true;
                                                 }};
                                                   


      if(eor > now)
        {
          // wait for end of reception before processing next packet
          pPlatformService_->timerService().scheduleTimedEvent(eor,pCallback);
        }
      else
        {
          // we can process now, end of reception has past
          (*pCallback)();
              
          delete pCallback;
        }
    }

  return true;
}

void 
EMANE::Models::TDMA::MACLayer::splitPkt(int totallen, Utils::VectorIO vio, void *part1, int part1len, void *part2)
{
   size_t m=0;
   unsigned char * buffer = new unsigned char [totallen];
   for (size_t k=0;k<vio.size();k++) {
	memcpy(buffer+m,(unsigned char *)(vio[k].iov_base),vio[k].iov_len);
	m = m+vio[k].iov_len;
   }
   memcpy((unsigned char *)part1,buffer,part1len);
   memcpy((unsigned char *)part2,buffer+part1len,totallen-part1len);
   delete buffer;
}

size_t
EMANE::Models::TDMA::MACLayer::getPktSize(EMANE::DownstreamPacket & pkt,std::uint8_t fragflag)
{
   size_t x = (pkt.length()>payloadadjustlen_)?(pkt.length()-payloadadjustlen_):0;
   return (fragflag<1?x:pkt.length());
}

size_t 
EMANE::Models::TDMA::MACLayer::getTimeByte(std::uint64_t sendRatebps, EMANE::Microseconds tvLeftTime)
{
   std::uint64_t  leftTime = std::chrono::duration_cast<Microseconds>(tvLeftTime).count(); 
   size_t length = 0;  
   length = size_t(leftTime * sendRatebps /  1000000 / 8); 
   return length;  
}

EMANE::Microseconds 
EMANE::Models::TDMA::MACLayer::getDurationMicroseconds(size_t lengthInBytes, std::uint64_t sendRatebps)
{
  if(sendRatebps > 0)
    {
      std::uint64_t us = lengthInBytes;
      us = ((us * 8000000) / sendRatebps);
      Microseconds ret(us);
      return ret;
    }
  else
    {
      return Microseconds{};
    }
}


EMANE::Microseconds 
EMANE::Models::TDMA::MACLayer::getJitter()
{
  if(fJitterSeconds_ > 0.0f)
    {
      return std::chrono::duration_cast<Microseconds>(DoubleSeconds{(*pRNDJitter_)()});
    }
  else
    {
      return Microseconds{};
    }
}

std::uint16_t 
EMANE::Models::TDMA::MACLayer::getDataRateIndex(std::uint64_t recvRatebps)
{
  for (size_t i=0;i<dataratebps_.size();i++) 
	if (recvRatebps == dataratebps_[i]) return i+1;
  return 0;
}

std::uint64_t 
EMANE::Models::TDMA::MACLayer::getDataRate(std::uint8_t rateIdx)
{
  if (rateIdx > dataratebps_.size()) return 0;
  return dataratebps_[rateIdx-1];	
}

bool 
EMANE::Models::TDMA::MACLayer::checkPOR(float fSINR, size_t packetSize, std::uint16_t dataRateIndex)
{
  // find por
  float fPCR{pcrManager_.getPCR(fSINR, packetSize, dataRateIndex)};

  // get random value [0.0, 1.0]
  float fRandomValue{RNDZeroToOne_()};

  // pcr >= random value
  bool bResult{fPCR >= fRandomValue};

  LOGGER_VERBOSE_LOGGING(pPlatformService_->logService(), 
                         DEBUG_LEVEL, 
                         "MACI %03hu %s::%s: sinr %3.2f, pcr %3.2f %s rand %3.3f",
                         id_, 
                         pzLayerName,
                         __func__, 
                         fSINR,
                         fPCR,
                         bResult ? ">=" : "<", 
                         fRandomValue);

  return bResult;       
}

/** [timerservice-processtimedevent-snippet] */ 
void 
EMANE::Models::TDMA::MACLayer::processTimedEvent(TimerEventId,
                                                   const TimePoint &,
                                                   const TimePoint &,
                                                   const TimePoint &,
                                                   const void * arg)
{
/*
  LOGGER_VERBOSE_LOGGING(pPlatformService_->logService(), 
                         DEBUG_LEVEL, 
                         "MACI %03hu %s::%s: tevent it: %u",
                         id_, 
                         pzLayerName,
                         __func__, 
                         eventid);
*/
      auto pCallBack = reinterpret_cast<const std::function<bool()> *>(arg);
      if((*pCallBack)())
        {
          delete pCallBack;
        }
}
/** [timerservice-processtimedevent-snippet] */ 
void
EMANE::Models::TDMA::MACLayer::processEvent(const EventId & eventId,
                                                    const Serialization & serialization)
{
  LOGGER_VERBOSE_LOGGING(pPlatformService_->logService(), 
                         DEBUG_LEVEL, 
                         "MACI %03hu %s::%s: event it: %hu",
                         id_, 
                         pzLayerName,
                         __func__, 
                         eventId);

  // check event id
  switch(eventId)
    {
    case EMANE::Models::TDMA::TdmaBEvent::IDENTIFIER:
      {
	eventLock_.lock();
	EMANE::Models::TDMA::TdmaBEvent bevent(serialization);
	if (bevent.getSubId() == macsubid_) {
	    const SlotMap & slotmap = bevent.getSlotmap();
	    std::uint64_t slotbt = bevent.getSlot0time();

	    usedSlotNum_ = 0;
	    for (size_t i=0;i<slotmap.size();i++) {
		slot_map_[i] = slotmap[i];
		if (slot_map_[i] == id_) usedSlotNum_++;
	    }
	    tdmaBaseTime_ = slotbt;
	    tdmaReady_ = true;
	}
	eventLock_.unlock();
      }
      break;
    }
  // no other events to be handled
}

DECLARE_MAC_LAYER(EMANE::Models::TDMA::MACLayer);
