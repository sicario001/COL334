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

NS_LOG_COMPONENT_DEFINE ("A3Part1");


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
    bool useV6 = false;
    int mode;
    std::string modeStr, congestionControlProtocol, resultFilename;
    if (argc!=3){
        std::cout<<"Enter the mode and TCP congestion control protocol/result filename for packet drop as argument <0 Newreno/Highspeed/Veno/Vegas> or <1 filename>";
        return 0;
    }
    else{
        modeStr = argv[1];
        if (modeStr=="0"){
            mode = 0;
        }
        else if (modeStr=="1"){
            mode = 1;
        }
        else{
            std::cout<<"Enter the mode and TCP congestion control protocol/result filename for packet drop as argument <0 Newreno/Highspeed/Veno/Vegas> or <1 filename>";
            return 0;
        }
        if (mode==0){
            StringValue protocol;
            congestionControlProtocol = argv[2];

            if (congestionControlProtocol == "Newreno"){
                protocol = StringValue ("ns3::TcpNewReno");
            }
            else if (congestionControlProtocol == "Highspeed"){
                protocol = StringValue ("ns3::TcpHighSpeed");
            }   
            else if (congestionControlProtocol == "Veno"){
                protocol = StringValue ("ns3::TcpVeno");
            }
            else if (congestionControlProtocol == "Vegas"){
                protocol = StringValue ("ns3::TcpVegas");
            }
            else{
                std::cout<<"Enter the TCP congestion control protocol as argument <Newreno/Highspeed/Veno/Vegas>";
                return 0;
            }
            
            Config::SetDefault ("ns3::TcpL4Protocol::SocketType", protocol);
        }
        else{
            resultFilename = argv[2];
        }

    }
    if (mode==0){

        NodeContainer nodes;
        nodes.Create (2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("8Mbps"));
        pointToPoint.SetChannelAttribute ("Delay", StringValue ("3ms"));

        NetDeviceContainer devices;
        devices = pointToPoint.Install (nodes);

        Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
        em->SetAttribute ("ErrorRate", DoubleValue (0.00001));
        devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

        InternetStackHelper stack;
        stack.Install (nodes);

        uint16_t sinkPort = 8080;
        Address sinkAddress;
        Address anyAddress;
        std::string probeType;
        std::string tracePath;
        if (useV6 == false)
        {
            Ipv4AddressHelper address;
            address.SetBase ("10.1.1.0", "255.255.255.0");
            Ipv4InterfaceContainer interfaces = address.Assign (devices);
            sinkAddress = InetSocketAddress (interfaces.GetAddress (1), sinkPort);
            anyAddress = InetSocketAddress (Ipv4Address::GetAny (), sinkPort);
            probeType = "ns3::Ipv4PacketProbe";
            tracePath = "/NodeList/*/$ns3::Ipv4L3Protocol/Tx";
        }
        else
        {
            Ipv6AddressHelper address;
            address.SetBase ("2001:0000:f00d:cafe::", Ipv6Prefix (64));
            Ipv6InterfaceContainer interfaces = address.Assign (devices);
            sinkAddress = Inet6SocketAddress (interfaces.GetAddress (1,1), sinkPort);
            anyAddress = Inet6SocketAddress (Ipv6Address::GetAny (), sinkPort);
            probeType = "ns3::Ipv6PacketProbe";
            tracePath = "/NodeList/*/$ns3::Ipv6L3Protocol/Tx";
        }

        // PacketLossCounter packetLossCounter;
        PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", anyAddress);
        ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
        sinkApps.Start (Seconds (0.));
        sinkApps.Stop (Seconds (30.));

        Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());

        Ptr<MyApp> app = CreateObject<MyApp> ();
        app->Setup (ns3TcpSocket, sinkAddress, 3000, -1, DataRate ("1Mbps"));
        nodes.Get (0)->AddApplication (app);
        app->SetStartTime (Seconds (1.));
        app->SetStopTime (Seconds (30.));

        AsciiTraceHelper asciiTraceHelper;
        Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("output/CongestionWindow.cwnd");
        ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream));

        PcapHelper pcapHelper;
        Ptr<PcapFileWrapper> file = pcapHelper.CreateFile ("output/PhyRxDrop.pcap", std::ios::out, PcapHelper::DLT_PPP);
        devices.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDrop, file));

            

        Simulator::Stop (Seconds (30));
        Simulator::Run ();
        Simulator::Destroy ();

        generatePlotCwnd("CongestionWindow.cwnd", congestionControlProtocol+"-plot");
    }
    else{
        getDroppedPackets(resultFilename, true);
    }

    return 0;
}

