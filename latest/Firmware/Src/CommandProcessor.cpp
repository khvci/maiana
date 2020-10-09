/*
  Copyright (c) 2016-2020 Peter Antypas

  This file is part of the MAIANA™ transponder firmware.

  The firmware is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>
*/


#include "CommandProcessor.hpp"
#include "EventQueue.hpp"
#include "Utils.hpp"
#include "TXScheduler.hpp"
#include "config.h"
#include "StationData.h"
#include "TXScheduler.hpp"
#include "bsp.hpp"
#include "GPS.hpp"
#include "RadioManager.hpp"


CommandProcessor &CommandProcessor::instance()
{
  static CommandProcessor __instance;
  return __instance;
}

void CommandProcessor::init()
{

}

CommandProcessor::CommandProcessor()
{
  EventQueue::instance().addObserver(this, COMMAND_EVENT);
}

void CommandProcessor::processEvent(const Event &e)
{
  switch(e.type)
  {
  case COMMAND_EVENT:
    processCommand(e.command.buffer);
    break;
  default:
    break;
  }
}

void fireTestPacket()
{
  VHFChannel channel = CH_87;

  if ( rand() % 2 == 0 )
    channel = CH_88;

  TXPacket *p = TXPacketPool::instance().newTXPacket(channel);
  if ( !p ) {
      //DBG("Ooops! Out of TX packets :(\r\n");
      return;
  }

  /**
   * Define a dummy packet of 9600 random bits, so it will take 1 second to transmit.
   * This is long enough for most spectrum analyzers to capture details using "max hold",
   * even at very low resolution bandwidths. Great way to measure power and look for
   * spurious emissions as well as harmonics.
   */
  p->configureForTesting(channel, 9600);

  RadioManager::instance().sendTestPacketNow(p);
}

void CommandProcessor::processCommand(const char *buff)
{
  string s(buff);
  Utils::trim(s);

  if ( s.find("station ") == 0 )
    {
      /*
       * The station command format is:
       * station mmsi,name,callsign,type,len,beam,portoffset,bowoffset
       */

      StationData station;

      string params = s.substr(8);
      if (params.empty())
        return;

      vector<string> tokens;
      Utils::tokenize(params, ',', tokens);
      if ( tokens.size() < 8 )
        return;

      memset(&station, 0, sizeof station);
      station.mmsi = Utils::toInt(tokens[0]);
      strncpy(station.name, tokens[1].c_str(), sizeof station.name);
      strncpy(station.callsign, tokens[2].c_str(), sizeof station.callsign);
      int type = (VesselType)Utils::toInt(tokens[3]);
      if ( type == 30 || type == 34 || type == 36 || type == 37 )
        station.type = (VesselType)type;
      station.len = Utils::toInt(tokens[4]);
      station.beam = Utils::toInt(tokens[5]);
      station.portOffset = Utils::toInt(tokens[6]);
      station.bowOffset = Utils::toInt(tokens[7]);
      station.magic = STATION_DATA_MAGIC;

      Configuration::instance().writeStationData(station);
    }
  else if ( s.find("dfu") == 0 )
    {
      jumpToBootloader();
    }
  else if ( s.find("factory reset") == 0 )
    {
      // Clear station data
      Configuration::instance().resetToDefaults();
    }
  else if ( s.find("tx test") == 0 )
    {
      fireTestPacket();
    }
  else if (s.find("reboot") == 0 )
    {
      bsp_reboot();
    }
}

void CommandProcessor::jumpToBootloader()
{
  GPS::instance().disable();
  bsp_enter_dfu();
}


