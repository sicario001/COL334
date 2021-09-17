#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <fstream>
#include <vector>
#include <pthread.h>
#include <arpa/inet.h>
#include <map>

#define PORT 8080
using namespace std;
// to do
// 1) broadcast message
// 2) valid username

ssize_t recv_all(int fd, void* buf, size_t buf_len) {
    for(size_t len = buf_len; len;) {
        ssize_t r = ::recv(fd, buf, len, 0);
        if(r <= 0)
            return r;
        buf = static_cast<char*>(buf) + r;
        len -= r;
    }
    return buf_len;
}

ssize_t send_all(int fd, void const* buf, size_t buf_len) {
    for(size_t len = buf_len; len;) {
        ssize_t r = ::send(fd, buf, len, 0);
        if(r <= 0)
            return r;
        buf = static_cast<char const*>(buf) + r;
        len -= r;
    }
    return buf_len;
}

void send_string(int fd, std::string const& msg) {
    ssize_t r;
    // Send message length.
    uint32_t len = msg.size();
    len = htonl(len); // In network byte order.
    if((r = send_all(fd, &len, sizeof len)) < 0)
        throw std::runtime_error("send_all 1");
    // Send the message.
    if((r = send_all(fd, msg.data(), msg.size())) < 0)
        throw std::runtime_error("send_all 2");
}

std::string recv_string(int fd) {
    ssize_t r;
    // Receive message length in network byte order.
    uint32_t len;
    if((r = recv_all(fd, &len, sizeof len)) <= 0)
        throw std::runtime_error("recv_all 1");
    len = ntohl(len);
    // Receive the message.
    std::string msg(len, '\0');
    if(len && (r = recv_all(fd, &msg[0], len)) <= 0)
        throw std::runtime_error("recv_all 2");
    return msg;
}


// maintain for setting up socket accepting new connections at the defined port
int server_fd;
struct sockaddr_in address;
int addrlen = sizeof(address);
// storing different socket fd and thread index for different users send and rcv sockets
std::map<std::string, int> username_to_socket_send;
std::map<std::string, int> username_to_socket_rcv;
std::map<int, std::string> socket_send_to_username;
std::map<int, std::string> socket_rcv_to_username;

const int max_msg_size = 1024;

std::map<int, vector<char>> socket_to_buffer_msg;
std::map<int, vector<char>> socket_to_buffer_rcv;
std::map<int, int> socket_to_buffer_msg_ind;
// buffers maintained for receiving messages

// char buffer_msg[max_msg_size] = {0};
// int buffer_msg_ind = 0;     // first free index of buffer_msg
// char buffer_rcv[max_msg_size] = {0};
void delete_socket_buffers(int sock){
    if (socket_to_buffer_msg.find(sock)!=socket_to_buffer_msg.end()){
        socket_to_buffer_msg.erase(sock);
    }
    if (socket_to_buffer_rcv.find(sock)!=socket_to_buffer_rcv.end()){
        socket_to_buffer_rcv.erase(sock);
    }
    if (socket_to_buffer_msg_ind.find(sock)!=socket_to_buffer_msg_ind.end()){
        socket_to_buffer_msg_ind.erase(sock);
    }

}

std::string receive_msg(int fd){
   // at every call of receive_msg we try to return a message
    bool msg_found = false;
    // ind of last character of message
    int msg_end_ind = -1;
    // run while loop until msg in found in the buffer_msg
    while(!msg_found){
        // check if message is already present using delimeter \n\n
        for (int i=0; i<(max_msg_size-1); i++){
            if (socket_to_buffer_msg[fd][i]=='\n' && socket_to_buffer_msg[fd][i+1]=='\n'){
                msg_found = true;
                msg_end_ind = i-1;
                break;
            }
        }
        // if msg is absent then recv is called
        if (!msg_found){
            std::fill(socket_to_buffer_rcv[fd].begin(), socket_to_buffer_rcv[fd].end(), 0);
            ssize_t r = recv(fd, &socket_to_buffer_rcv[fd][0], socket_to_buffer_rcv[fd].size(), 0);
            if (r>0){
                for (int i= socket_to_buffer_msg_ind[fd]; i<socket_to_buffer_msg_ind[fd]+r; i++){
                    socket_to_buffer_msg[fd][i] = socket_to_buffer_rcv[fd][i-socket_to_buffer_msg_ind[fd]];
                }
                
                // update the first free index of buffer_msg i.e buffer_msg_ind, as it moves to the right
                socket_to_buffer_msg_ind[fd] +=r;
            }
        }
    }
    std::string message;
    // if message is found update the buffer_msg by deleting the message and shifting the remaining characters to the left
    if (msg_found){
        message = std::string(socket_to_buffer_msg[fd].begin(),socket_to_buffer_msg[fd].begin() +msg_end_ind+1);
        for (int i= msg_end_ind+3; i<max_msg_size; i++){
            socket_to_buffer_msg[fd][i-(msg_end_ind+3)] = socket_to_buffer_msg[fd][i];
        }
        for (int i= max_msg_size-(msg_end_ind+3); i<max_msg_size; i++){
            socket_to_buffer_msg[fd][i] = 0;
        }
        // update the first free index of buffer_msg i.e buffer_msg_ind, as it moves to the left
        socket_to_buffer_msg_ind[fd] = socket_to_buffer_msg_ind[fd]-(msg_end_ind+3);
    }
    return message;
    
}

std::string get_message_by_length(int content_length, int sock){
    std::string msg;
    int req_length = content_length-socket_to_buffer_msg_ind[sock];
    if (req_length<0){
        msg = std::string(socket_to_buffer_msg[sock].begin(), socket_to_buffer_msg[sock].begin()+content_length);
        for (int i=content_length; i<socket_to_buffer_msg_ind[sock]; i++){
            socket_to_buffer_msg[sock][i-content_length] = socket_to_buffer_msg[sock][i];
        }
        for (int i = socket_to_buffer_msg_ind[sock]-content_length; i<socket_to_buffer_msg_ind[sock]; i++){
            socket_to_buffer_msg[sock][i] = 0;
        }
        socket_to_buffer_msg_ind[sock] = socket_to_buffer_msg_ind[sock]-content_length;
    }
    else{
        char buffer[req_length+10] = {0};
        recv_all(sock, &buffer, req_length);
        
        for (int i=socket_to_buffer_msg_ind[sock]; i<socket_to_buffer_msg_ind[sock]+req_length; i++){
            socket_to_buffer_msg[sock][i] = buffer[i-socket_to_buffer_msg_ind[sock]];
        }
        msg = std::string(socket_to_buffer_msg[sock].begin(), socket_to_buffer_msg[sock].begin()+content_length);
        std::fill(socket_to_buffer_msg[sock].begin(), socket_to_buffer_msg[sock].end(), 0);
        socket_to_buffer_msg_ind[sock] = 0;
    }
    return msg;
}
enum HEADER_RECV{
    REGISTER_TOSEND,
    REGISTER_TORECV,
    SEND,
    ERROR_103_HEADER_INCOMPLETE_RECV,
    RECEIVED,
    INVALID_RECV
};
enum HEADER_SEND{
    REGISTERED_TOSEND,
    REGISTERED_TORECV,
    ERROR_100_MALFORMED_USERNAME,
    ERROR_101_NO_USER_REGISTERED,
    ERROR_103_HEADER_INCOMPLETE_SEND,
    SENT,
    ERROR_102_UNABLE_TO_SEND,
    FORWARD,
    INVALID_SEND
};

void send_msg(HEADER_SEND type, std::vector<std::string>args, int sock){
    switch(type){
        case REGISTERED_TOSEND:
        {
            std::string msg = "REGISTERED TOSEND ";
            msg+=args[0];
            msg+="\n\n";
            send_all(sock, msg.c_str(), msg.length());
            break;
        }
        case REGISTERED_TORECV:
        {
            std::string msg = "REGISTERED TORECV ";
            msg+=args[0];
            msg+="\n\n";
            send_all(sock, msg.c_str(), msg.length());

            break;
        }
        case FORWARD:
        {
            std::string msg = "FORWARD ";
            msg+=args[0];
            msg+="\n";
            msg+="Content-length: ";
            msg+=args[1];
            msg+="\n\n";
            msg+=args[2];
            send_all(sock, msg.c_str(), msg.length());
            break;
        }
        case ERROR_100_MALFORMED_USERNAME:
        {
            std::string msg = "ERROR 100 Malformed username\n\n";
            send_all(sock, msg.c_str(), msg.length());
            break;
        }
        case ERROR_101_NO_USER_REGISTERED:
        {
            std::string msg = "ERROR 101 No user registered\n\n";
            send_all(sock, msg.c_str(), msg.length());
            break;
        }
        case ERROR_102_UNABLE_TO_SEND:
        {
            std::string msg = "ERROR 102 Unable to send\n\n";
            send_all(sock, msg.c_str(), msg.length());
            break;
        }
        case ERROR_103_HEADER_INCOMPLETE_SEND:
        {
            std::string msg = "ERROR 103 Header Incomplete\n\n";
            send_all(sock, msg.c_str(), msg.length());
            break;
        }
        case SENT:
        {
            std::string msg = "SENT ";
            msg+=args[0];
            msg+="\n\n";
            send_all(sock, msg.c_str(), msg.length());

            break;
        }
        default:
        {
            
        }

    }
}

struct args{
    int socket;
};

std::vector<std::string>parse_header(HEADER_RECV type, std::string header){
    switch(type){
        case REGISTER_TOSEND:
        {
            int len = header.length();
            std::string username = header.substr(16, len-16);
            return {username};
            break;
        }
        case REGISTER_TORECV:
        {
            int len = header.length();
            std::string username = header.substr(16, len-16);
            return {username};
            break;
        }
        case SEND:
        {
            std::string username, length;
            int len = header.length();
            std::string sub_msg = header.substr(5, len-5);
            int len_sub = sub_msg.length();
            int ind = -1;
            for (int i=0; i<len_sub; i++){
                if (sub_msg[i]=='\n'){
                    ind = i;
                    break;
                }
            }
            username = sub_msg.substr(0, ind);
            sub_msg = sub_msg.substr(ind+1, len_sub-ind-1);
            len_sub = sub_msg.length();
            length = sub_msg.substr(16, len_sub-16);
            return {username, length};
            break;
        }
        case RECEIVED:
        {
            int len = header.length();
            std::string username = header.substr(9, len-9);
            return {username};
            break;
        }
        default:
        {
            return {};
        }

    }
}
HEADER_RECV get_header_recv_type(std::string header){
    int len = header.length();
    if (len>=16 && header.substr(0, 16)=="REGISTER TOSEND "){
        return REGISTER_TOSEND;
    }
    else if (len>=16 && header.substr(0, 16)=="REGISTER TORECV "){
        return REGISTER_TORECV;
    }
    else if (len>=9 && header.substr(0, 9)=="RECEIVED "){
        return RECEIVED;
    }
    else if (len>=5 && header.substr(0, 5)=="SEND "){
        std::string sub_msg = header.substr(5, len-5);
        int len_sub = sub_msg.length();
        int ind = -1;
        for (int i=0; i<len_sub; i++){
            if (sub_msg[i]=='\n'){
                ind = i;
                break;
            }
        }
        if (ind>=0){
            sub_msg = sub_msg.substr(ind+1, len_sub-ind-1);
            len_sub = sub_msg.length();
            if (len_sub>=16 && sub_msg.substr(0, 16)=="Content-length: "){
                return SEND;
            }
        }
    }
    else if (len==27 && header == "ERROR 103 Header Incomplete"){
        return ERROR_103_HEADER_INCOMPLETE_RECV;
    }
    return INVALID_RECV;
}
bool valid_username(std::string username){
    for (char x: username){
        if ((x<'0' || x>'9')&&(x<'a' || x>'z')&&(x<'A' || x>'Z')){
            return false;
        }
    }
    return true;
}
void rcv_and_forward(int sock){
    while (true){
        HEADER_RECV type;
        std::string msg;

        msg = receive_msg(sock);

        type = get_header_recv_type(msg);
        
        if (type==SEND){
            std::vector<std::string> parsed_header = parse_header(SEND, msg);
            int content_length = std::stoi(parsed_header[1]);
            std::string msg_content = get_message_by_length(content_length, sock);
            if (parsed_header[0]=="ALL"){
                // broadcast message
                bool broadcast_sent = true;
                for (auto x: username_to_socket_rcv){
                    int socket_to_forward = x.second;
                    bool received = false;
                    send_msg(FORWARD, {socket_send_to_username[sock], std::to_string(content_length), msg_content}, socket_to_forward);
                    while (true){
                        HEADER_RECV ack_type;
                        std::string ack_msg;
                        ack_msg = receive_msg(socket_to_forward);
                        ack_type = get_header_recv_type(ack_msg);
                        // cout<<ack_msg<<"\n";
                        if (ack_type==RECEIVED){
                            std::vector<std::string> parsed_ack = parse_header(RECEIVED, ack_msg);
                            if (parsed_ack[0]==socket_send_to_username[sock]){
                                received = true;
                                break;
                            }
                        }
                        else if (ack_type==ERROR_103_HEADER_INCOMPLETE_RECV){  
                            std::string username_to_forward = socket_rcv_to_username[socket_to_forward];
                            socket_rcv_to_username.erase(socket_to_forward);
                            username_to_socket_rcv.erase(username_to_forward);
                            delete_socket_buffers(socket_to_forward);
                            close(socket_to_forward);
                            break;
                        }
                        else if (ack_type == INVALID_RECV){
                            send_msg(ERROR_103_HEADER_INCOMPLETE_SEND, {}, socket_to_forward);
                            std::string username_to_forward = socket_rcv_to_username[socket_to_forward];
                            socket_rcv_to_username.erase(socket_to_forward);
                            username_to_socket_rcv.erase(username_to_forward);
                            delete_socket_buffers(socket_to_forward);
                            close(socket_to_forward);
                            break;
                        }
                    }
                    if (!received){
                        broadcast_sent = false;
                        break;
                    }
                }
                if (broadcast_sent){
                    send_msg(SENT, {"ALL"}, sock);
                }
                else{
                    send_msg(ERROR_102_UNABLE_TO_SEND, {}, sock);
                }
            }
            else if ((username_to_socket_rcv.find(parsed_header[0])==username_to_socket_rcv.end())){
                send_msg(ERROR_102_UNABLE_TO_SEND, {}, sock);
            }
            else{
                int socket_to_forward = username_to_socket_rcv[parsed_header[0]];
                send_msg(FORWARD, {socket_send_to_username[sock], std::to_string(content_length), msg_content}, socket_to_forward);
                bool received = false;
                while (true){
                    HEADER_RECV ack_type;
                    std::string ack_msg;
                    ack_msg = receive_msg(socket_to_forward);
                    ack_type = get_header_recv_type(ack_msg);
                    // cout<<ack_msg<<"\n";
                    if (ack_type==RECEIVED){
                        std::vector<std::string> parsed_ack = parse_header(RECEIVED, ack_msg);
                        if (parsed_ack[0]==socket_send_to_username[sock]){
                            received = true;
                            break;
                        }
                    }
                    else if (ack_type==ERROR_103_HEADER_INCOMPLETE_RECV){  
                        std::string username_to_forward = socket_rcv_to_username[socket_to_forward];
                        socket_rcv_to_username.erase(socket_to_forward);
                        username_to_socket_rcv.erase(username_to_forward);
                        delete_socket_buffers(socket_to_forward);
                        close(socket_to_forward);
                        break;
                    }
                    else if (ack_type == INVALID_RECV){
                        send_msg(ERROR_103_HEADER_INCOMPLETE_SEND, {}, socket_to_forward);
                        std::string username_to_forward = socket_rcv_to_username[socket_to_forward];
                        socket_rcv_to_username.erase(socket_to_forward);
                        username_to_socket_rcv.erase(username_to_forward);
                        delete_socket_buffers(socket_to_forward);
                        close(socket_to_forward);
                        break;
                    }
                }
                if (received){
                    send_msg(SENT, {socket_rcv_to_username[socket_to_forward]}, sock);
                }
                else{
                    send_msg(ERROR_102_UNABLE_TO_SEND, {}, sock);
                }
            }
        }
        else if (type== INVALID_RECV){
            send_msg(ERROR_103_HEADER_INCOMPLETE_SEND, {}, sock);   
            std::string username = socket_send_to_username[sock];
            socket_send_to_username.erase(sock);
            username_to_socket_send.erase(username);
            delete_socket_buffers(sock);
            close(sock);
            break;
        }
        else if (type = ERROR_103_HEADER_INCOMPLETE_RECV){
            std::string username = socket_send_to_username[sock];
            socket_send_to_username.erase(sock);
            username_to_socket_send.erase(username);
            delete_socket_buffers(sock);
            close(sock);
            break;
        }
    }
}
void* registration_func(void* arg){
    pthread_detach(pthread_self());
    int sock = ((struct args*)arg)->socket;
    HEADER_RECV type;
    std::string msg;
    bool flag_torecv = false;
    bool flag_tosend = false;
    std::string username;
    while (!(flag_torecv || flag_tosend)){
        msg = receive_msg(sock);
        type = get_header_recv_type(msg);

        if (type==REGISTER_TOSEND){

            std::vector<std::string> parsed_header = parse_header(REGISTER_TOSEND, msg);
            if (valid_username(parsed_header[0])){
                username = parsed_header[0];
                flag_tosend = true;
                
            }
            else{
                send_msg(ERROR_100_MALFORMED_USERNAME, {}, sock);
            }
        }
        else if (type==REGISTER_TORECV){
            std::vector<std::string> parsed_header = parse_header(REGISTER_TORECV, msg);
            if (valid_username(parsed_header[0])){
                username = parsed_header[0];
                flag_torecv = true;
            }
            else{
                send_msg(ERROR_100_MALFORMED_USERNAME, {}, sock);
            }
        }
        else if (type==SEND){
            std::vector<std::string> parsed_header = parse_header(SEND, msg);
            int content_length = std::stoi(parsed_header[1]);
            std::string msg_content = get_message_by_length(content_length, sock);
            //wait to receive the message length equal to content length using receive_all(assuming content length field was correct else it falls in invalid type)
        }
        else if (type == ERROR_103_HEADER_INCOMPLETE_RECV){
            delete_socket_buffers(sock);
            close(sock);
            break;
        }
        else if (type==INVALID_RECV){
            //close the connection by sending the header incomplete message
            send_msg(ERROR_103_HEADER_INCOMPLETE_SEND, {}, sock);
            delete_socket_buffers(sock);
            close(sock);
            break;
        }
    }
    // registration complete or socket closed
    if (flag_tosend){
        // cout<<"IN1\n";
        send_msg(REGISTERED_TOSEND, {username}, sock);
        // cout<<"IN2\n";
        username_to_socket_send[username] = sock;
        socket_send_to_username[sock] = username;
        // continue receiving messages from the connected client from this socket
        
        rcv_and_forward(sock);
    }
    else if (flag_torecv){
        // cout<<"IN1\n";
        send_msg(REGISTERED_TORECV, {username}, sock);
        // cout<<"IN2\n";
        username_to_socket_rcv[username] = sock;
        socket_rcv_to_username[sock] = username;
        
    }
    pthread_exit(NULL);
    
}
void setup(){
    // char buffer[1024] = {0};
    // const char *hello = "Hello from server\n\nHello there\n\nHi there\n\nasa";
       
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
       
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );
       
    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address, 
                                 sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 10 is backlog here
    if (listen(server_fd, 50) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
}
void recv_connections(){
    
    int new_socket;
    while (true){
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, 
                            (socklen_t*)&addrlen))<0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        pthread_t new_thread;
        // socket_to_thread[new_socket] = new_thread; // check if needed
        struct args *arg = (struct args *)malloc(sizeof(struct args));
        arg->socket = new_socket;

        socket_to_buffer_msg[new_socket] = vector<char>(max_msg_size);
        socket_to_buffer_rcv[new_socket] = vector<char>(max_msg_size);
        socket_to_buffer_msg_ind[new_socket] = 0;

        pthread_create(&new_thread, NULL, &registration_func, (void *)arg);

    }
}
void test(){
    int server_fd_test, new_socket, valread;
    struct sockaddr_in address_test;
    int opt = 1;
    int addrlen = sizeof(address_test);
    char buffer[1024] = {0};
       
    // Creating socket file descriptor
    if ((server_fd_test = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
       
    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd_test, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                                                  &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address_test.sin_family = AF_INET;
    address_test.sin_addr.s_addr = INADDR_ANY;
    address_test.sin_port = htons( PORT );
       
    // Forcefully attaching socket to the port 8080
    if (bind(server_fd_test, (struct sockaddr *)&address_test, 
                                 sizeof(address_test))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd_test, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    if ((new_socket = accept(server_fd_test, (struct sockaddr *)&address_test, 
                       (socklen_t*)&addrlen))<0)
    {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    socket_to_buffer_msg[new_socket] = vector<char>(max_msg_size);
    socket_to_buffer_rcv[new_socket] = vector<char>(max_msg_size);


    socket_to_buffer_msg_ind[new_socket] = 0;

    // ssize_t r = recv(server_fd_test, buffer, 100, 0);
    // cout<<r<<"\n";
    string msg = "Hello\n\n";
    send_all(new_socket, msg.c_str(), msg.length());

    string rcv_msg = receive_msg(new_socket);
    std::cout<<rcv_msg<<"\n";
}
int main(int argc, char *argv[]){
	
    // new_socket is the identifier for the connected socket
    // test();
    setup();
    recv_connections();
    return 0;
}
