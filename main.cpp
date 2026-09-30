#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "main.hpp"
#include "diagnose.hpp"
#include <iostream>

int main(){ 
    int id; 
    cout << "Digite o id desse processo: " << endl; 
    cin >> id; 

    Client c(id); 
    
    thread recv_thread([&c]() {
        while (true) { 
            c.receive_message(); 
        } 
    }); 

    thread timer_thread([&c]() {
        c.start_timer(2000); 
    }); 

    thread info_thread([&c]() {
        while (true) {
            this_thread::sleep_for(chrono::seconds(5));
            c.show_infos();
        }
    });

    recv_thread.join(); 
    timer_thread.join(); 
    info_thread.join();

    return 0;
}