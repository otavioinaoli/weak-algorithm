#ifndef MAIN_HPP
#define MAIN_HPP

#include <bits/stdc++.h>
#include <string>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "diagnose.hpp"
#include <array>
#include <thread>
#include <chrono>
#include <iostream>

using namespace std;

#define HEARTBEAT_REQUEST 1
#define HEARTBEAT_REPLY 2
#define ALL 3

struct Message {
    int procId;
    int procDest;
    int type; 
    std::string msg;


    Message(){
        procId = 0;
        msg = "";
    }

    Message(int id, int dest, string m, int t) {
        procId = id;
        procDest = dest;
        msg = m;
        type = t;
    }


    string message_to_string(){
        return to_string(procId)  + "-" + msg;
    }

    std::array<char, 1024> to_datagram() {
        std::array<char, 1024> dtg;
        char *ptr = (char *) dtg.data();
        memcpy((void *) ptr, (void *) &procId, sizeof(procId));
        ptr += sizeof(procId);
        memcpy((void *) ptr, (void *) &procDest, sizeof(procDest));
        ptr += sizeof(procDest);
        memcpy((void *) ptr, (void *) &type, sizeof(type));
        ptr += sizeof(type);
        memcpy((void *) ptr, (void *) msg.c_str(), msg.size()+1);

        return dtg;
    }

    void from_datagram(std::array<char, 1024> dtg) {
        char *ptr = (char *) dtg.data();
        memcpy((void *) &procId, (void *) ptr, sizeof(procId));
        ptr += sizeof(procId);
        memcpy((void *) &procDest, (void *) ptr, sizeof(procDest));
        ptr += sizeof(procDest);

        memcpy((void *) &type, (void *) ptr, sizeof(type));
        ptr += sizeof(type);
        //strcpy(msg.c_str(), (void *) ptr);
        msg = msg.assign(ptr);
    }

};

struct Client {
    int procId;
    int serverSocketRcv;
    int serverSocketSend;
    int lider;
    unordered_set<int> suspected;
    unordered_set<int> alive;
    unordered_set<int> process;

    Client(int id) {
        // Inicializando estrutras
        procId = id;
        lider = 1;

        for(int i = 1; i <= 5; i++){
            process.insert(i);
        }
        for(int i = 1; i <= 5; i++){
            alive.insert(i);
        }

         // returns a file descriptor for an IPv4 UDP socket, or a negative value on failure
        serverSocketRcv = socket(AF_INET, SOCK_DGRAM, 0);
        diagnose(serverSocketRcv >= 0, "Opening datagram socket for receive");

        {
            // enable SO_REUSEADDR to allow multiple instances of this application to
            //    receive copies of the multicast datagrams.
            int reuse = 1;
            diagnose(setsockopt(serverSocketRcv, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse,
                sizeof(reuse)) >= 0, "Setting SO_REUSEADDR");
        }

        // Bind to the proper port number with the IP address specified as INADDR_ANY
        sockaddr_in serverAddress = {};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(8080);
        serverAddress.sin_addr.s_addr = INADDR_ANY;

        // bind() attaches the socket to a local address and port. INADDR_ANY binds
        // every network interface on the machine, which is what a server usually wants
        diagnose(!bind(serverSocketRcv, (struct sockaddr*)&serverAddress, sizeof(serverAddress)),
            "Binding datagram socket");

        ip_mreq group = {};    // initialize to all zeroes
        group.imr_multiaddr.s_addr = inet_addr("226.1.1.1");
        group.imr_interface.s_addr = inet_addr("127.0.0.1");
        diagnose(setsockopt(serverSocketRcv, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&group,
            sizeof(group)) >= 0, "Adding multicast group");

        serverSocketSend = socket(AF_INET, SOCK_DGRAM, 0);
        diagnose(serverSocketSend >= 0, "Opening datagram socket for send");

        in_addr localIface = {};   // init to all zeroes
        localIface.s_addr = inet_addr("127.0.0.1");
        diagnose(setsockopt(serverSocketSend, IPPROTO_IP, IP_MULTICAST_IF, (char*)&localIface,
                            sizeof(localIface)) >= 0, "Setting local interface");
    }

    void send_message(string text, int type, int procDest){
        Message m (procId, procDest, text, type);

        sockaddr_in groupSock = {};  
        groupSock.sin_family = AF_INET;
        groupSock.sin_addr.s_addr = inet_addr("226.1.1.1");
        groupSock.sin_port = htons(8080);

        diagnose(sendto(serverSocketSend, m.to_datagram().data(), m.to_datagram().size(), 0,
                        (sockaddr*)&groupSock, sizeof(groupSock)) >= 0,
                "Sending datagram message");
    }

    Message receive_message(){
        // Read from the socket
        std::array<char, 1024> arr;
        arr.fill(0);

        diagnose(read(serverSocketRcv, arr.data(), arr.size()) >= 0, "Reading datagram message");

        Message m;
        m.from_datagram(arr);
        if(m.procDest != ALL && m.procDest != procId) return m;

        if(m.type == HEARTBEAT_REQUEST){
            send_message("I AM LIVE",HEARTBEAT_REPLY, m.procId);
        }

        if(m.type == HEARTBEAT_REPLY){
            alive.insert(m.procId);
        }
    
        return m;
    }

    void start_timer(int interval_ms = 2000) {
        while (true) {
            this_thread::sleep_for(chrono::milliseconds(interval_ms));
            perfect_failure_detector();
        }
    }

    void perfect_failure_detector(){
        for(auto p : process){
            if(!alive.count(p) && !suspected.count(p)){
                suspected.insert(p);
            }else if(alive.count(p) && suspected.count(p)){
                suspected.erase(p); 
            }
            send_message("Send heatbeat", HEARTBEAT_REQUEST, ALL);
        }
        leader_election();
        alive.clear();
    }

    int select_candidates(){
        int min_id = INT_MAX;
        for(auto a: alive){
            if(a < min_id){
                min_id = a;
            }
        }
        return min_id;
    }

    void leader_election(){
        int winner = select_candidates();
        if(winner != INT_MAX)
            lider = select_candidates();
    }

    void show_infos(){ 
        cout << "id: " << procId << endl; 
        cout << "líder: " << lider << endl; 

        cout << "processos: " << endl; 
        for(auto s : process){ 
            cout << s << ", "; 
        } 

        cout << "\nalive: " << endl; 
        for(auto s : alive){ 
            cout << s << ", "; 
        } 

        cout << "\nsuspected: " << endl; 
        for(auto s : suspected){ 
            cout << s << ", "; 
        }

        cout << endl;
    }

};

#endif