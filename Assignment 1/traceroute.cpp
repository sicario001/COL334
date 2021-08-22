#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cstdio>
#include <stdexcept>
#include <array>
#include <sstream>
#include <iomanip>
#include <fstream>

#include "gnuplot_i.hpp"

using namespace std;

std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
class IPv4{
    public :
    std::string ip1;
    std::string ip2;
    std::string ip3;
    std::string ip4;
    IPv4(){};
    IPv4(string ip){
        vector<string> ip_vec = IPv4::parse_IPv4(ip);
        ip1 = ip_vec[0];
        ip2 = ip_vec[1];
        ip3 = ip_vec[2];
        ip4 = ip_vec[3];
    }
    std::string get_IPv4(){
        return (ip1+"."+ip2+"."+ip3+"."+ip4);
    }
    static std::vector<std::string> parse_IPv4(std::string ip){
        int len = ip.length();
        std::vector<int> pos;
        pos.push_back(-1);
        for (int i=0; i<len; i++){
            if (ip[i]=='.'){
                pos.push_back(i);
            }
        }
        pos.push_back(len);
        std::vector<std::string> res;
        for (int i=0; i<4; i++){
            res.push_back(ip.substr(pos[i]+1, pos[i+1]-pos[i]-1));
        }
        return res;
    }


};
enum PING_STATUS{
    OK,
    TTL_EXPIRED_IN_TRANSIT,
    REQUEST_TIMED_OUT,
};
class PingResult{
    public:
    PING_STATUS status;
    string ip;
    vector<int> rtt;
    PingResult(){};
    PingResult(PING_STATUS status_, string ip_, vector<int> rtt_){
        status = status_;
        ip = ip_;
        rtt = rtt_;
    }
    void print(){
        cout<<"Status : ";
        switch (status){
            case OK:
                cout<<"OK\n";
                cout<<"IP : "<<ip<<"\n";
                cout<<"RTT : ";
                for (int x:rtt){
                    cout<<x<<" ";
                }
                cout<<"\n";
                break;
            case TTL_EXPIRED_IN_TRANSIT:
                cout<<"TTL_EXPIRED_IN_TRANSIT\n";
                cout<<"IP : "<<ip<<"\n";
                break;
            case REQUEST_TIMED_OUT:
                cout<<"REQUEST_TIMED_OUT\n";
        }
    }

};
class Ping{
    public:
    static string PingCmd(int ttl, string domain, int packet_size = 32, int num_packets = 3){
        string exec_cmd = "ping -l "+to_string(packet_size)+ " " + "-i "+to_string(ttl)+" "+"-n "+to_string(num_packets)+" "+domain;
        return exec(exec_cmd.c_str());
    }

    static PingResult parsePingResult(string result){
        PING_STATUS status;
        string ip = "";
        vector<int> rtt;

        std::istringstream f(result);
        std::string line;    
        while (std::getline(f, line)) {
            int len = line.length();
            if (len>=11 && line.substr(0, 11)=="Reply from "){
                for (int i=11; i<len; i++){
                    if (line[i]==':'){
                        ip = line.substr(11, i-11);
                        if (line.substr(i+1, len-i-1)==" TTL expired in transit."){
                            status = TTL_EXPIRED_IN_TRANSIT;
                            rtt.push_back(-1);
                        }
                        else{
                            for (int j=i+1; (j+4)<len; j++){
                                if (line.substr(j, 4)=="time"){
                                    int start = j+5;
                                    int end = start;
                                    for (int k=start; (k+1)<len; k++){
                                        if (line.substr(k, 2)=="ms"){
                                            end = k-1;
                                            break;
                                        }
                                    }
                                    rtt.push_back(stoi(line.substr(start, end-start+1)));
                                    break;
                                }
                                
                            }
                        }
                        break;
                    }
                }
            }
            else if (len>=18 && line.substr(0, 18)=="Request timed out."){
                status = REQUEST_TIMED_OUT;
                rtt.push_back(-1);
            }
        }
        return PingResult(status, ip, rtt);
    }
};
class TraceRouteHopResult{
    public:
    int srl_num;
    vector<int> rtt;
    string ip;
    TraceRouteHopResult(){};
    TraceRouteHopResult(int srl_num_, vector<int> rtt_, string ip_){
        srl_num = srl_num_;
        rtt = rtt_;
        ip = ip_;
    }
};
class TraceRoute{
    public:
    static vector<TraceRouteHopResult> runTraceRoute(string domain, int max_hops = 30){
        cout<<"Tracing route to "<<domain<<"\n";
        cout<<"over a maximum of "<<max_hops<<" hops:\n\n";

        vector<TraceRouteHopResult> TraceRouteResult;
        bool flag = false;
        for (int hop = 1; hop<=30; hop++){
            PingResult pingResult = Ping::parsePingResult(Ping::PingCmd(hop, domain));
            if (pingResult.status == REQUEST_TIMED_OUT){
                TraceRouteResult.push_back(TraceRouteHopResult(hop, {-1, -1, -1}, "Request timed out."));
            }
            else if (pingResult.status == TTL_EXPIRED_IN_TRANSIT){
                PingResult pingResult_hop = Ping::parsePingResult(Ping::PingCmd(max_hops, pingResult.ip));
                TraceRouteResult.push_back(TraceRouteHopResult(hop, pingResult_hop.rtt, pingResult.ip));
            }
            else{
                PingResult pingResult_hop = Ping::parsePingResult(Ping::PingCmd(max_hops, pingResult.ip));
                TraceRouteResult.push_back(TraceRouteHopResult(hop, pingResult_hop.rtt, pingResult.ip));
                flag = true;
            }
            print(TraceRouteResult.back());
            if (flag){
                break;
            }
        }
        cout<<"Trace complete.\n";
        return TraceRouteResult;
    }
    static void print(TraceRouteHopResult traceRouteHopResult){
        vector<string> cols(5);
        cols[0] = to_string((traceRouteHopResult.srl_num));
        for (int j=1; j<4; j++){
            if (traceRouteHopResult.rtt[j-1]==-1){
                cols[j] = "*";
            }
            else if (traceRouteHopResult.rtt[j-1]==1){
                cols[j] = "<1 ms";
            }
            else {
                cols[j] = to_string(traceRouteHopResult.rtt[j-1])+" ms";
            }
        }
        cols[4] =  traceRouteHopResult.ip;
        

        cout<<std::left<<std::setw(10)<<cols[0]<<std::setw(15)<<cols[1]<<std::setw(15)<<cols[2]<<std::setw(15)<<cols[3]<<std::setw(25)<<cols[4]<<"\n";
    }
    static void print(vector<TraceRouteHopResult> TraceRouteResult){
        for (int i=0; i<(int)TraceRouteResult.size(); i++){
            print(TraceRouteResult[i]);
        }
    }
};
void saveFile(vector<double> x, vector<double> y){
    ofstream file("data.dat");
    int n = min((int)x.size(), (int)y.size());
    for (int i=0; i<n; i++){
        file<<x[i]<<" "<<y[i]<<"\n";
    }
    file.close();
}
void plotX(string outfile, vector<string> infile, vector<string> legend, int x, int y, string xlabel, string ylabel){
    x++,y++;
    Gnuplot g1;
    g1 << "set terminal png\n";

    stringstream ofile;
    ofile << "set output '"<<outfile<<"'\n";
    g1 << ofile.str();

    stringstream label;
    label<<"set xlabel '"<<xlabel<<"'";
    g1<<label.str();

    label.str(string());
    label<<"set ylabel '"<<ylabel<<"'";
    g1<<label.str();

    stringstream cmdstr;
    cmdstr<<"plot ";
    for(int i=0;i<(int)infile.size();i++){
        cmdstr << "'" << infile[i] << "'" << "using "<<y<<":"<<x<<" title '"<<legend[i]<<"' with lines, ";
    }
    g1<<cmdstr.str();
}
void plotXYL(string outFile, string inFile, int x, int y,string xlabel, string ylabel, string title){
    x++,y++;
    stringstream cmdstr;
    cmdstr<<"plot '"<<inFile<<"' using "<<y<<":"<<x<<" title '"<<title<<"' with lines, ";
    Gnuplot g1;
    // set output to graph.png
    g1 << cmdstr.str();

    g1 << "MAX=GPVAL_Y_MAX\n";
    g1 << "MIN=GPVAL_Y_MIN\n";
    g1 << "set yrange [0:MAX+(MAX-MIN)*0.1]\n";
    g1 << "MAX=GPVAL_X_MAX\n";
    g1 << "MIN=GPVAL_X_MIN\n";
    g1 << "set xrange [MIN-(MAX-MIN)*0.05:MAX+(MAX-MIN)*0.1]\n";
    g1 << "set terminal png\n";

    stringstream ofile;
    ofile<<"set output '"<<outFile<<"'\n";
    g1 << ofile.str();

    stringstream label;
    label<<"set xlabel '"<<xlabel<<"'";
    g1<<label.str();

    label.str(string());
    label<<"set ylabel '"<<ylabel<<"'";
    g1<<label.str();

    g1 << cmdstr.str();
}

void plot(vector<TraceRouteHopResult> TraceRouteResult){

    vector<double> x;
    vector<double> y;
    int n = TraceRouteResult.size();
    for (int i=0; i<n; i++){
        int cnt = 0;
        int val = 0;
        for (int j=0; j<(int)TraceRouteResult[i].rtt.size(); j++){
            if ((TraceRouteResult[i].rtt)[j]!=-1){
                cnt++;
                val += (TraceRouteResult[i].rtt)[j];
            }
        }
        if (cnt>0){
            x.push_back(TraceRouteResult[i].srl_num);
            y.push_back(val/3);
        }
    }
    saveFile(x, y);
    plotXYL("img.png", "data.dat", 1, 0, "Hop number", "Average RTT for 3 packets", "Plot of RTT vs hop number");
    remove("data.dat");

}

int main(int argc, char *argv[]){
    // system("ping -i 10 google.com");
    // std::string ip = "123.132.43.12";
    // IPv4 ip1(ip);
    // cout<<ip1.get_IPv4()<<"\n";
    // PingResult pingResult = Ping::parsePingResult(Ping::PingCmd(50, "iitd.ac.in"));
    // pingResult.print();
    if (argc!=2 && argc!=3){
        cout<<"Invalid Command.\nPlease enter the domain name or the IP address as the argument\n";
    }
    else if (argc==2){
        plot(TraceRoute::runTraceRoute(argv[1]));
    }
    else{
        plot(TraceRoute::runTraceRoute(argv[1], stoi(argv[2])));
    }
    
    return 0;
}