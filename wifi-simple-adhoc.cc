/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 The Boeing Company
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

// 
// This script configures two nodes on an 802.11b physical layer, with
// 802.11b NICs in adhoc mode, and by default, sends one packet of 1000 
// (application) bytes to the other node.  The physical layer is configured
// to receive at a fixed RSS (regardless of the distance and transmit
// power); therefore, changing position of the nodes has no effect. 
//
// There are a number of command-line options available to control
// the default behavior.  The list of available command-line options
// can be listed with the following command:
// ./waf --run "wifi-simple-adhoc --help"
//
// For instance, for this configuration, the physical layer will
// stop successfully receiving packets when rss drops below -97 dBm.
// To see this effect, try running:
//
// ./waf --run "wifi-simple-adhoc --rss=-97 --numPackets=20"
// ./waf --run "wifi-simple-adhoc --rss=-98 --numPackets=20"
// ./waf --run "wifi-simple-adhoc --rss=-99 --numPackets=20"
//
// Note that all ns-3 attributes (not just the ones exposed in the below
// script) can be changed at command line; see the documentation.
//
// This script can also be helpful to put the Wifi layer into verbose
// logging mode; this command will turn on all wifi logging:
// 
// ./waf --run "wifi-simple-adhoc --verbose=1"
//
// When you are done, you will notice two pcap trace files in your directory.
// If you have tcpdump installed, you can try this:
//
// tcpdump -r wifi-simple-adhoc-0-0.pcap -nn -tt
//

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/header.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

NS_LOG_COMPONENT_DEFINE ("WifiSimpleAdhoc");

using namespace ns3;

void ReceivePacket (Ptr<Socket> socket)
{
       
        Ptr<Node> nd = socket->GetNode();
    Ptr<Packet> p = socket->Recv();
    uint8_t *buffer = new uint8_t[p->GetSize ()];
    p->CopyData(buffer,p->GetSize ());
    //cout <<"1 st byte " << (int)buffer[0] <<(int)buffer[1] <<endl;
    //UdpHeader hd;
    //p->PeekHeader(hd);
    //NS_LOG_UNCOND("Read packet header"<<hd);
    NS_LOG_UNCOND ("Node "<<nd->GetId()<<" Received one packet! Uid="<<p->GetUid()<< "from" << (int)buffer[0] );   
}

static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize, 
                             uint32_t pktCount, Time pktInterval )
{
  if (pktCount > 0)
    {
      Ptr<Node> nd = socket->GetNode();/*getting node*/
      int node_no;
      node_no = nd->GetId();
      cout<<"sending packet from " << node_no << endl;
      uint8_t packetdata[4] = {11,20,30,40};
      /*1 byte of the packet will have sourcr node no*/
      packetdata[0] = node_no;
      socket->Send (Create<Packet> (packetdata,4));
      //socket->Send (p);
      Simulator::Schedule (pktInterval, &GenerateTraffic, 
                           socket, pktSize,pktCount-1, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}


int main (int argc, char *argv[])
{
  //ErpOfdmRate24Mbps      
  //std::string phyMode ("DsssRate11Mbps");
  std::string phyMode ("ErpOfdmRate24Mbps"); 
  double rss = -80;  // -dBm
  uint32_t packetSize = 100; // bytes
  uint32_t numPackets = 10;
  double interval = 1.0; // seconds
  bool verbose = false;

  CommandLine cmd;

  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("rss", "received signal strength", rss);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "number of packets generated", numPackets);
  cmd.AddValue ("interval", "interval (seconds) between packets", interval);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);

  cmd.Parse (argc, argv);
  // Convert to time object
  Time interPacketInterval = Seconds (interval);

  // disable fragmentation for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  // turn off RTS/CTS for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", 
                      StringValue (phyMode));

  NodeContainer c;
  c.Create (5);

  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;
  if (verbose)
    {
      wifi.EnableLogComponents ();  // Turn on all Wifi logging
    }
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  // This is one parameter that matters when using FixedRssLossModel
  // set it to zero; otherwise, gain will be added
  wifiPhy.Set ("RxGain", DoubleValue (0) ); 
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO); 

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  // The below FixedRssLossModel will cause the rss to be fixed regardless
  // of the distance between the two stations, and the transmit power
  //wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue (rss));
  //uint8_t nRays = 3;
  //uint8_t nOscillators = 8;
        //JakesFadingLossModel
  //wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel"); 
  wifiChannel.AddPropagationLoss ("ns3::RandomPropagationLossModel","Variable", RandomVariableValue (ExponentialVariable (100.0))); 
  //wifiChannel.AddPropagationLoss ("ns3::JakesFadingLossModel"); 
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add a non-QoS upper mac, and disable rate control
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
  // Set it to adhoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, c);

  // Note that with FixedRssLossModel, the positions below are not 
  // used for received signal strength. 
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();  
  positionAlloc->Add (Vector (0.0, 250.0, 0.0));//0
  positionAlloc->Add (Vector (250.0, 0.0, 0.0));//1
  positionAlloc->Add (Vector (250.0, 250.0, 0.0));//2
  positionAlloc->Add (Vector (250.0, 500.0, 0.0));//3
  positionAlloc->Add (Vector (500.0, 250.0, 0.0));//4
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (c);

  InternetStackHelper internet;
  internet.Install (c);

  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (c.Get (2), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));

//add one more reciever 
  Ptr<Socket> recvSink1 = Socket::CreateSocket (c.Get (1), tid);
  InetSocketAddress local1 = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink1->Bind (local1);
  recvSink1->SetRecvCallback (MakeCallback (&ReceivePacket));

  //add one more reciever 
  Ptr<Socket> recvSink2 = Socket::CreateSocket (c.Get (3), tid);
  InetSocketAddress local2 = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink2->Bind (local2);
  recvSink2->SetRecvCallback (MakeCallback (&ReceivePacket));
    
  //add one more reciever 
  Ptr<Socket> recvSink3 = Socket::CreateSocket (c.Get (4), tid);
  InetSocketAddress local3 = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink3->Bind (local3);
  recvSink3->SetRecvCallback (MakeCallback (&ReceivePacket)); 
    
  Ptr<Socket> source = Socket::CreateSocket (c.Get (0), tid);
  InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  source->SetAllowBroadcast (true);
  source->Connect (remote);

  //create another source 

  Ptr<Socket> source1 = Socket::CreateSocket (c.Get (1), tid);
  InetSocketAddress remote1 = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  source1->SetAllowBroadcast (true);
  source1->Connect (remote1);  

  // Tracing
  wifiPhy.EnablePcap ("wifi-simple-adhoc", devices);

  // Output what we are doing
  //NS_LOG_UNCOND ("Testing " << numPackets  << " packets sent with receiver rss " << rss );

  Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
                                  Seconds (1.0), &GenerateTraffic, 
                                  source, packetSize, numPackets, interPacketInterval);

  Simulator::ScheduleWithContext (source1->GetNode ()->GetId (),
                                  Seconds (1.2), &GenerateTraffic, 
                                  source1, packetSize, numPackets, interPacketInterval);    

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

