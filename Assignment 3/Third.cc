/*
Most of the code has been adapted from the seventh.cc file of ns3 examples
*/

#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("A3Part3");


class MyApp : public Application
{
public:
    MyApp ();
    virtual ~MyApp ();

    /**
     * Register this type.
     * \return The TypeId.
     */
    static TypeId GetTypeId (void);
    void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
    virtual void StartApplication (void);
    virtual void StopApplication (void);

    void ScheduleTx (void);
    void SendPacket (void);

    Ptr<Socket>     m_socket;
    Address         m_peer;
    uint32_t        m_packetSize;
    uint32_t        m_nPackets;
    DataRate        m_dataRate;
    EventId         m_sendEvent;
    bool            m_running;
    uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

MyApp::~MyApp ()
{
    m_socket = 0;
}

/* static */
TypeId MyApp::GetTypeId (void)
{
    static TypeId tid = TypeId ("MyApp")
        .SetParent<Application> ()
        .SetGroupName ("Tutorial")
        .AddConstructor<MyApp> ()
        ;
    return tid;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
    m_running = true;
    m_packetsSent = 0;
    if (InetSocketAddress::IsMatchingType (m_peer))
    {
        m_socket->Bind ();
    }
    else
    {
        m_socket->Bind6 ();
    }
    m_socket->Connect (m_peer);
    SendPacket ();
}

void
MyApp::StopApplication (void)
{
    m_running = false;

    if (m_sendEvent.IsRunning ())
    {
        Simulator::Cancel (m_sendEvent);
    }

    if (m_socket)
    {
        m_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    m_socket->Send (packet);

    if (++m_packetsSent < m_nPackets)
    {
        ScheduleTx ();
    }
}

void
MyApp::ScheduleTx (void)
{
    if (m_running)
    {
        Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
        m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
    *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << oldCwnd << "\t" << newCwnd << std::endl;
}

static void
RxDrop (Ptr<PcapFileWrapper> file, Ptr<const Packet> p)
{
    NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
    file->Write (Simulator::Now (), p);
}

void generatePlotCwnd(std::string input, std::string output){
    std::fstream plot;
    plot.open("output/plot.plt", std::fstream::out);
    plot << "set terminal png" <<std::endl;
    plot << "set output \"output/"+output+".png\""<<std::endl;
    plot << "set title \"Congestion Window vs. Time\""<<std::endl;
    plot << "set xlabel \"Time (seconds)\""<<std::endl;
    plot << "set ylabel \"Congestion Window Size (Bytes)\""<<std::endl;
    plot << "plot \"output/"+input+"\" using 1:2 title 'Congestion Window' with linespoints pt 1 ps 0.1"<<std::endl;
    plot.close();
    std::string cmd = "gnuplot output/plot.plt";
    const char *command = cmd.c_str();
    system(command);
}
int getDroppedPackets(std::string input, bool save){
    std::fstream saveDroppedPackets;
    if (save){
        saveDroppedPackets.open("output/droppedPackets.txt", std::fstream::out);
    }
    std::ifstream result(input);
    std::string line;
    int cnt = 0;
    while (getline(result, line)){
        if (line.length()>10 && line.substr(0, 10)=="RxDrop at "){
            cnt++;
            if (save){
                saveDroppedPackets << line << std::endl;
            }
        }
    }
    if (save){
        saveDroppedPackets << "Total dropped packets: " << cnt << std::endl;
        saveDroppedPackets.close();
    }
    std::cout<<"Total dropped packets: " << cnt << std::endl;
    return cnt;
}
int main (int argc, char *argv[])
{

    int mode;
    std::string modeStr, applicationDataRate = "1.5Mbps", channelDataRate1 = "10Mbps", channelDataRate2 = "9Mbps", propDelay = "3ms";
    TypeId protocol1 = TypeId::LookupByName ("ns3::TcpNewReno");
    TypeId protocol2 = TypeId::LookupByName ("ns3::TcpNewReno");
    if (argc!=2){
        std::cout<<"Enter the configuration mode\n";
        return 0;
    }
    else{
        modeStr = argv[1];
        if (modeStr=="1"){
            mode = 1;
        }
        else if (modeStr=="2"){
            mode = 2;
        }
        else if (modeStr=="3"){
            mode = 3;
        }
        else{
            std::cout<<"Enter the configuration mode\n";
            return 0;
        }
        if (mode==1){
            
        }
        else if (mode==2){
            protocol2 = TypeId::LookupByName ("ns3::TcpNewRenoCSE");
        }
        else{
            protocol1 = TypeId::LookupByName ("ns3::TcpNewRenoCSE");
            protocol2 = TypeId::LookupByName ("ns3::TcpNewRenoCSE");
        }
    }
    NodeContainer nodes;
    nodes.Create (3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue (channelDataRate1));
    pointToPoint.SetChannelAttribute ("Delay", StringValue (propDelay));

    NetDeviceContainer devices1;
    devices1 = pointToPoint.Install (nodes.Get(0), nodes.Get(2));


    pointToPoint.SetDeviceAttribute ("DataRate", StringValue (channelDataRate2));
    pointToPoint.SetChannelAttribute ("Delay", StringValue (propDelay));

    NetDeviceContainer devices2;
    devices2 = pointToPoint.Install (nodes.Get(1), nodes.Get(2));

    Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
    em->SetAttribute ("ErrorRate", DoubleValue (0.00001));
    devices1.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
    devices2.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

    InternetStackHelper stack;
    stack.Install (nodes);

    uint16_t sinkPort = 8080;
    Address sinkAddress1, sinkAddress2;
    Address anyAddress;

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign (devices1);
    address.SetBase ("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign (devices2);
    sinkAddress1 = InetSocketAddress (interfaces1.GetAddress (1), sinkPort);
    sinkAddress2 = InetSocketAddress (interfaces2.GetAddress (1), sinkPort);

    anyAddress = InetSocketAddress (Ipv4Address::GetAny (), sinkPort);


    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", anyAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (2));
    sinkApps.Start (Seconds (0.));
    sinkApps.Stop (Seconds (30.));



    std::stringstream nodeId1;
    nodeId1<< nodes.Get (0)->GetId ();
    std::string specificNode1 = "/NodeList/" + nodeId1.str () + "/$ns3::TcpL4Protocol/SocketType";
    Config::Set (specificNode1, TypeIdValue(protocol1));

    std::stringstream nodeId2;
    nodeId2<< nodes.Get (1)->GetId ();
    std::string specificNode2 = "/NodeList/" + nodeId2.str () + "/$ns3::TcpL4Protocol/SocketType";
    Config::Set (specificNode2, TypeIdValue(protocol2));



    Ptr<Socket> ns3TcpSocket1_1 = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());
    Ptr<Socket> ns3TcpSocket1_2 = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());
    Ptr<Socket> ns3TcpSocket2_3 = Socket::CreateSocket (nodes.Get (1), TcpSocketFactory::GetTypeId ());




    Ptr<MyApp> app1_1 = CreateObject<MyApp> ();
    Ptr<MyApp> app1_2 = CreateObject<MyApp> ();
    Ptr<MyApp> app2_3 = CreateObject<MyApp> ();
    app1_1->Setup (ns3TcpSocket1_1, sinkAddress1, 3000, -1, DataRate (applicationDataRate));
    nodes.Get (0)->AddApplication (app1_1);
    app1_1->SetStartTime (Seconds (1.));
    app1_1->SetStopTime (Seconds (20.));

    app1_2->Setup (ns3TcpSocket1_2, sinkAddress1, 3000, -1, DataRate (applicationDataRate));
    nodes.Get (0)->AddApplication (app1_2);
    app1_2->SetStartTime (Seconds (5.));
    app1_2->SetStopTime (Seconds (25.));

    app2_3->Setup (ns3TcpSocket2_3, sinkAddress2, 3000, -1, DataRate (applicationDataRate));
    nodes.Get (1)->AddApplication (app2_3);
    app2_3->SetStartTime (Seconds (15.));
    app2_3->SetStopTime (Seconds (30.));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream1 = asciiTraceHelper.CreateFileStream ("output/CongestionWindow1.cwnd");
    ns3TcpSocket1_1->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream1));

    Ptr<OutputStreamWrapper> stream2 = asciiTraceHelper.CreateFileStream ("output/CongestionWindow2.cwnd");
    ns3TcpSocket1_2->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream2));

    Ptr<OutputStreamWrapper> stream3 = asciiTraceHelper.CreateFileStream ("output/CongestionWindow3.cwnd");
    ns3TcpSocket2_3->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream3));

    PcapHelper pcapHelper;
    Ptr<PcapFileWrapper> file1 = pcapHelper.CreateFile ("output/PhyRxDrop1.pcap", std::ios::out, PcapHelper::DLT_PPP);
    devices1.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDrop, file1));

    Ptr<PcapFileWrapper> file2 = pcapHelper.CreateFile ("output/PhyRxDrop2.pcap", std::ios::out, PcapHelper::DLT_PPP);
    devices2.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDrop, file2));

        

    Simulator::Stop (Seconds (30));
    Simulator::Run ();
    Simulator::Destroy ();

    generatePlotCwnd("CongestionWindow1.cwnd", "Connection1");
    generatePlotCwnd("CongestionWindow2.cwnd", "Connection2");
    generatePlotCwnd("CongestionWindow3.cwnd", "Connection3");


    return 0;
}

