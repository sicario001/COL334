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
#include<ios>
#include<limits>
#include<map>

#define PORT 8080
using namespace std;
enum STATE{
    NOT_CONNECTED,   
    CONNECTED,
    REGISTERED
};
std::string username;
std::string SERVER_IP;
int sock_send = 0;
int sock_rcv = 0;

pthread_t send_thread, rcv_thread;
STATE send_state = NOT_CONNECTED;
STATE rcv_state = NOT_CONNECTED;
// buffers maintained for receiving messages
const int max_msg_size = 1024;

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

std::string receive_msg(int fd, vector<char>& buffer_msg, vector<char>& buffer_rcv, int * buffer_msg_ind){
   // at every call of receive_msg we try to return a message
    bool msg_found = false;
    // ind of last character of message
    int msg_end_ind = -1;
    // run while loop until msg in found in the buffer_msg
    while(!msg_found){
        // check if message is already present using delimeter \n\n
        for (int i=0; i<(max_msg_size-1); i++){
            if (buffer_msg[i]=='\n' && buffer_msg[i+1]=='\n'){
                msg_found = true;
                msg_end_ind = i-1;
                break;
            }
        }
        
        if (!msg_found){
            
            std::fill(buffer_rcv.begin(), buffer_rcv.end(), 0);
            ssize_t r = recv(fd, &buffer_rcv[0], buffer_rcv.size(), 0);
            if (r>0){
                for (int i= (*buffer_msg_ind); i<(*buffer_msg_ind)+r; i++){
                    buffer_msg[i] = buffer_rcv[i-(*buffer_msg_ind)];
                }
                // update the first free index of buffer_msg i.e buffer_msg_ind, as it moves to the right
                (*buffer_msg_ind) = (*buffer_msg_ind) +r;
            }
        }
    }
    std::string message;
    // if message is found update the buffer_msg by deleting the message and shifting the remaining characters to the left
    if (msg_found){
        message = std::string(buffer_msg.begin(),buffer_msg.begin() +msg_end_ind+1);
        for (int i= msg_end_ind+3; i<max_msg_size; i++){
            buffer_msg[i-(msg_end_ind+3)] = buffer_msg[i];
        }
        for (int i= max_msg_size-(msg_end_ind+3); i<max_msg_size; i++){
            buffer_msg[i] = 0;
        }
        // update the first free index of buffer_msg i.e buffer_msg_ind, as it moves to the left
        (*buffer_msg_ind) = (*buffer_msg_ind)-(msg_end_ind+3);
    }
    return message;
    
}

std::string get_message_by_length(int content_length, int sock, vector<char>& buffer_msg, int * buffer_msg_ind){
    int req_length = content_length-(*buffer_msg_ind);
    std::string msg;
    if (req_length<0){
        msg = std::string(buffer_msg.begin(), buffer_msg.begin()+content_length);
        for (int i=content_length; i<(* buffer_msg_ind); i++){
            buffer_msg[i-content_length] = buffer_msg[i];
        }
        for (int i = (* buffer_msg_ind)-content_length; i<(* buffer_msg_ind); i++){
            buffer_msg[i] = 0;
        }
        (* buffer_msg_ind) = (* buffer_msg_ind)-content_length;
    }
    else{
        char buffer[req_length+10] = {0};
        recv_all(sock, &buffer, req_length);
        
        for (int i=(*buffer_msg_ind); i<(*buffer_msg_ind)+req_length; i++){
            buffer_msg[i] = buffer[i-(*buffer_msg_ind)];
        }
        msg= std::string(buffer_msg.begin(), buffer_msg.begin()+content_length);
        std::fill(buffer_msg.begin(), buffer_msg.end(), 0);
        (* buffer_msg_ind) = 0;
    }
    return msg;
}
enum HEADER_RECV{
    REGISTERED_TOSEND,
    REGISTERED_TORECV,
    ERROR_100_MALFORMED_USERNAME,
    ERROR_101_NO_USER_REGISTERED,
    ERROR_103_HEADER_INCOMPLETE_RECV,
    SENT,
    ERROR_102_UNABLE_TO_SEND,
    FORWARD,
    INVALID_RECV
};
enum HEADER_SEND{
    REGISTER_TOSEND,
    REGISTER_TORECV,
    SEND,
    ERROR_103_HEADER_INCOMPLETE_SEND,
    RECEIVED,
    INVALID_SEND
};
void send_msg(HEADER_SEND type, std::vector<std::string>args, int sock){
    switch(type){
        case REGISTER_TOSEND:
        {
            std::string msg = "REGISTER TOSEND ";
            msg+=args[0];
            msg+="\n\n";
            send_all(sock, msg.c_str(), msg.length());
            break;
        }
        case REGISTER_TORECV:
        {
            std::string msg = "REGISTER TORECV ";
            msg+=args[0];
            msg+="\n\n";
            send_all(sock, msg.c_str(), msg.length());

            break;
        }
        case SEND:
        {
            std::string msg = "SEND ";
            msg+=args[0];
            msg+="\n";
            msg+="Content-length: ";
            msg+=args[1];
            msg+="\n\n";
            msg+=args[2];
            send_all(sock, msg.c_str(), msg.length());
            break;
        }
        case RECEIVED:
        {
            std::string msg = "RECEIVED ";
            msg+=args[0];
            msg+="\n\n";
            send_all(sock, msg.c_str(), msg.length());
            break;
        }
        case ERROR_103_HEADER_INCOMPLETE_SEND:
        {
            std::string msg = "ERROR 103 Header Incomplete\n\n";
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
        case REGISTERED_TOSEND:
        {
            int len = header.length();
            std::string username = header.substr(18, len-18);
            return {username};
            break;
        }
        case REGISTERED_TORECV:
        {
            int len = header.length();
            std::string username = header.substr(18, len-18);
            return {username};
            break;
        }
        case SENT:
        {
            int len = header.length();
            std::string username = header.substr(5, len-5);
            return {username};
            break;
        }
        case FORWARD:
        {
            std::string username, length;
            int len = header.length();
            std::string sub_msg = header.substr(8, len-8);
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
        default:
        {
            return {};
        }

    }
}

HEADER_RECV get_header_recv_type(std::string header){
    int len = header.length();
    if (len>=18 && header.substr(0, 18)=="REGISTERED TOSEND "){
        return REGISTERED_TOSEND;
    }
    else if (len>=18 && header.substr(0, 18)=="REGISTERED TORECV "){
        return REGISTERED_TORECV;
    }
    else if (len>=5 && header.substr(0, 5)=="SENT "){
        return SENT;
    }
    else if (len>=8 && header.substr(0, 8)=="FORWARD "){
        std::string sub_msg = header.substr(8, len-8);
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
                return FORWARD;
            }
        }
    }
    else if (len==28 && header == "ERROR 100 Malformed username"){
        return ERROR_100_MALFORMED_USERNAME;
    }
    else if (len==28 && header == "ERROR 101 No user registered"){
        return ERROR_101_NO_USER_REGISTERED;
    }
    else if (len==24 && header == "ERROR 102 Unable to send"){
        return ERROR_102_UNABLE_TO_SEND;
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
bool valid_user_inp(std::string user_inp){
    int len = user_inp.length();
    std::string username, message;
    if (user_inp[0]=='@'){
        for (int i=0; i<len; i++){
            if (user_inp[i]==' '){
                username = user_inp.substr(1, i-1);
                message = user_inp.substr(i+1, len-i-1);
                if (valid_username(username)){
                    return true;
                }
            }
        }
    }
    return false;
}
std::vector<std::string> parse_user_inp(std::string user_inp){
    int len = user_inp.length();
    std::string username, message;
    for (int i=0; i<len; i++){
        if (user_inp[i]==' '){
            username = user_inp.substr(1, i-1);
            message = user_inp.substr(i+1, len-i-1);
            break;
        }
    }
    return {username, message};
}
int setup_sock_send(){
    struct sockaddr_in serv_addr;
    if ((sock_send = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP.c_str());

    if (connect(sock_send, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }
    send_state = CONNECTED;
    return 0;
}
int setup_sock_rcv(){
    struct sockaddr_in serv_addr;
    if ((sock_rcv= socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP.c_str());

    if (connect(sock_rcv, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }
    
    rcv_state = CONNECTED;
    return 0;
}
void* receive_func(void* arg){
    vector<char> buffer_msg(max_msg_size);
    vector<char> buffer_rcv(max_msg_size);
    int buffer_msg_ind = 0;
    

    while (true){
        if (rcv_state==NOT_CONNECTED){

            setup_sock_rcv();
        }
        else if (rcv_state==CONNECTED){
            // connected state, registration next
            send_msg(REGISTER_TORECV, {username}, sock_rcv);
            while(true){
                std::string msg = receive_msg(sock_rcv, buffer_msg, buffer_rcv, &buffer_msg_ind);
                HEADER_RECV type = get_header_recv_type(msg);
                if (type==REGISTERED_TORECV){
                    rcv_state = REGISTERED;
                    break;
                }
                else if (type == ERROR_100_MALFORMED_USERNAME){
                    break;
                    // since state of rcv_state not changed we again send the register message
                }
                else if (type == FORWARD){
                    std::vector<std::string> parsed_header = parse_header(FORWARD, msg);
                    int content_length = std::stoi(parsed_header[1]);
                    std::string msg_content = get_message_by_length(content_length, sock_rcv, buffer_msg, &buffer_msg_ind);
                    // read and ignore, continue waiting for expected response
                }
                else if (type == INVALID_RECV){
                    send_msg(ERROR_103_HEADER_INCOMPLETE_SEND, {}, sock_rcv);
                    close(sock_rcv);
                    cout<<"\nClosing connection!\n";
                    rcv_state = NOT_CONNECTED;
                    break;
                }
                else if (type == ERROR_103_HEADER_INCOMPLETE_RECV){
                    close(sock_rcv);
                    cout<<"\nClosing connection!\n";
                    rcv_state = NOT_CONNECTED;
                    break;
                }
            }
        }
        else{
            // registered state
            while(true){
                std::string msg = receive_msg(sock_rcv, buffer_msg, buffer_rcv, &buffer_msg_ind);
                HEADER_RECV type = get_header_recv_type(msg);
                if (type==FORWARD){
                    // receive message, send its ack and display it to the user
                    // read message
                    std::vector<std::string> parsed_msg = parse_header(FORWARD, msg);
                    int content_length = std::stoi(parsed_msg[1]);
                   
                    std::string msg_content = get_message_by_length(content_length, sock_rcv, buffer_msg, &buffer_msg_ind);
                    // sending ack
                    // cout<<msg<<"\n"<<msg_content<<"\n";
                    send_msg(RECEIVED, {parsed_msg[0]}, sock_rcv);
                    cout<<"#"<<parsed_msg[0]<<" "<<msg_content<<"\n";
                }
                else if (type == ERROR_103_HEADER_INCOMPLETE_RECV){
                    close(sock_rcv);
                    cout<<"\nClosing connection!\n";
                    rcv_state = NOT_CONNECTED;
                    break;
                }
                else if (type == INVALID_RECV){
                    send_msg(ERROR_103_HEADER_INCOMPLETE_SEND, {}, sock_rcv);
                    close(sock_rcv);
                    cout<<"\nClosing connection!\n";
                    rcv_state = NOT_CONNECTED;
                    break;
                }
            }
        }
    }
    pthread_exit(NULL);
}
void* send_func(void* arg){
    vector<char> buffer_msg(max_msg_size);
    vector<char> buffer_rcv(max_msg_size);
    int buffer_msg_ind = 0;

    while(true){
        if (send_state==NOT_CONNECTED){
            setup_sock_send();
        }
        else if (send_state==CONNECTED){
            // connected state, registration next
            send_msg(REGISTER_TOSEND, {username}, sock_send);
            while(true){
                // issue here and at other receive methods at client

                std::string msg = receive_msg(sock_send, buffer_msg, buffer_rcv, &buffer_msg_ind);

                HEADER_RECV type = get_header_recv_type(msg);
                if (type==REGISTERED_TOSEND){
                    send_state = REGISTERED;
                    break;
                }
                else if (type == ERROR_100_MALFORMED_USERNAME){
                    break;
                    // since state of rcv_state not changed we again send the register message
                }
                else if (type == FORWARD){
                    std::vector<std::string> parsed_header = parse_header(FORWARD, msg);
                    int content_length = std::stoi(parsed_header[1]);
                    std::string msg_content = get_message_by_length(content_length, sock_send, buffer_msg, &buffer_msg_ind);
                    // read and ignore, continue waiting for expected response
                }
                else if (type == INVALID_RECV){
                    send_msg(ERROR_103_HEADER_INCOMPLETE_SEND, {}, sock_send);
                    close(sock_send);
                    cout<<"\nClosing connection!\n";
                    send_state = NOT_CONNECTED;
                    break;
                }
                else if (type == ERROR_103_HEADER_INCOMPLETE_RECV){
                    close(sock_send);
                    cout<<"\nClosing connection!\n";
                    send_state = NOT_CONNECTED;
                    break;
                }
            }
        }
        else{
            std::cout<<"\nREGISTERED\n";
            // registered state
            while(true){
                std::string user_inp;
                getline(std::cin, user_inp, '\n');
                if (!valid_user_inp(user_inp)){
                    std::cout<<"\nInvalid command!";
                }
                else{
                    std::vector<std::string> parsed_inp = parse_user_inp(user_inp);
                    // cout<<parsed_inp[0]<<" "<<parsed_inp[1]<<"\n";
                    bool sent = false;
                    bool close_socket = false;;
                    send_msg(SEND, {parsed_inp[0], std::to_string(parsed_inp[1].length()), parsed_inp[1]}, sock_send);
                    while (true){
                        HEADER_RECV ack_type;
                        std::string ack_msg;
                        ack_msg = receive_msg(sock_send, buffer_msg, buffer_rcv, &buffer_msg_ind);
                        ack_type = get_header_recv_type(ack_msg);
                        if (ack_type==SENT){
                            std::vector<std::string> parsed_ack = parse_header(SENT, ack_msg);
                            if (parsed_ack[0]==parsed_inp[0]){
                                sent = true;
                                break;
                            }
                        }
                        else if (ack_type==ERROR_102_UNABLE_TO_SEND){
                            break;
                        }
                        else if (ack_type==ERROR_103_HEADER_INCOMPLETE_RECV){  
                            close(sock_send);
                            cout<<"\nClosing connection!\n";
                            send_state = NOT_CONNECTED;
                            close_socket = true;
                            break;
                        }
                        else if (ack_type == INVALID_RECV){
                            send_msg(ERROR_103_HEADER_INCOMPLETE_SEND, {}, sock_send);
                            close(sock_send);
                            cout<<"\nClosing connection!\n";
                            send_state = NOT_CONNECTED;
                            close_socket = true;
                            break;
                        }
                    }
                    if (!sent){
                        std::cout<<"\nMessage to "<<parsed_inp[0]<<" could not be sent!\n";
                        // ack message not sent
                    }
                    if (close_socket){
                        break;
                    }
                }
            }
        }
    }
    pthread_exit(NULL);
}


void setup(){
    // std::cout<<"\nin";
    pthread_create(&send_thread, NULL, &send_func, NULL);
    pthread_create(&rcv_thread, NULL, &receive_func, NULL);
    pthread_join(send_thread, NULL);
    pthread_join(rcv_thread, NULL);
}


int main(int argc, char const *argv[])
{
    
//-----------------------------------------------------------------------------------

    

    std::cout<<"\nEnter server's IP : ";
    // std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    getline(std::cin, SERVER_IP, '\n');

    std::cout<<"\nEnter username : ";
    // std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    getline(std::cin, username, '\n');
    while (!valid_username(username)){
        std::cout<<"\nInvalid Username!\nPlease enter a valid username : ";
        getline(std::cin, username, '\n');
    }
    
    setup();
    return 0;
}