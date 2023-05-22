#include <iostream>
#include <bits/stdc++.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <cmath>
#include <unistd.h>
#include <sys/file.h>
//#define _GNU_SOURCE
#include <fcntl.h>

#define BUFFER_SIZE 1024*64
#define QLIMIT 32
//#define CHUNK_SIZE 1024*512
#define CHUNK_SIZE 1024*64

using namespace std;

pair<string,int> split_address(string addr) {
    int pos = addr.find(":");
    pair<string,int> sock;
    sock.first = addr.substr(0,pos);
    sock.second = stoi(addr.substr(pos+1,addr.length()));
    return sock;
}

vector<string> split_string(string s,char d) {
    vector<string> v;
    if(s=="") {
        return v;
    }
    stringstream ss(s);
    string temp;
    while(getline(ss,temp,d)) {
        v.push_back(temp);
    }
    return v;
}

vector<int> split_bitvector(string s,char d, int totchunks) {
    vector<int> v;
    v.resize(totchunks,0);
    stringstream ss(s);
    string temp;
    while(getline(ss,temp,d)) {
        v[stoi(temp)] = 1;
    }
    return v;
}

string bitvec_toString(vector<int> v) {
    string bitstr = to_string(*v.begin());
    for(auto it=v.begin();it!=v.end();it++) {
        if(it == v.begin())
            continue;
        bitstr += ";"+to_string(*it);
    }
    return bitstr;
}

// It seems that you have provided a code snippet for the file transfer system with the tracker and peer components. However, without the complete implementation, it is difficult to provide specific guidance or address any issues you may be facing.

// Based on the provided code snippet, it appears that you have defined several data structures such as UserStruct, GroupStruct, and FileStruct to store information about peers, groups, and files. You are also using map containers to store and manage this information.

// Additionally, you have defined a constant PEERLIMIT to specify the maximum number of peers allowed, and you have created an array peer_TID to store the thread IDs of the peer threads.

#define PEERLIMIT 10

pthread_t tid_fd;
pair<string,int> THIS_TRACK_SOCK;
pthread_t peer_TID[PEERLIMIT];
char MSG_BUFF[BUFFER_SIZE];
struct UserStruct {
    string uid;
    string password;
    string ip;
    int port;
};
map<string,UserStruct> ALL_PEERS;
map<string,UserStruct> ACTIVE_PEERS;
struct GroupStruct {
    string gid;
    string owner;
    vector<string> members;
//    vector<string> files;
};
struct FileStruct {
    string sha;
    int totalchunks;
    long filesize;
    map<pair<string,int>,string> seeders;  //<<ip,port>,path>
    map<pair<string,int>,string> leechers; //<<ip,port>,path>
};
map<string,GroupStruct> GROUP_INFO;
map<pair<string,string>,FileStruct> FILE_INFO;  //<gid,filename>


void setSocket(string trkfile) {
    fstream fs(trkfile,ios::in); // The function opens the trkfile in input mode (ios::in) using an fstream object named fs.
    string tadr; // It declares a string variable tadr to store the read address.
    getline(fs,tadr); // It reads a line from the file using getline(fs, tadr), which reads the first line of the file into tadr.
    THIS_TRACK_SOCK = split_address(tadr); // The split_address function is called to split the address into an IP and port pair. The result is stored in the THIS_TRACK_SOCK variable, which is of type pair<string, int>.
    cout << THIS_TRACK_SOCK.first << " : " << THIS_TRACK_SOCK.second << endl; // Finally, the function prints the IP address and port using cout.
// It seems that this function is responsible for setting the tracker's socket address by reading it from the specified file.
}

string handle_create_user(string user, string pswd, string ipadr, string portno) {
    auto itr = ALL_PEERS.find(user);
    string status;
    if(itr == ALL_PEERS.end()) {
        UserStruct us;
        us.uid = user;
        us.password = pswd;
        us.ip = ipadr;
        us.port = stoi(string(portno));
        ALL_PEERS[user] = us;
        status = "New user created with --> NAME:"+user+" PASSWORD:"+pswd+"\n";
    }
    else {
        status = "User "+user+" already exists\n";
    }
    return status;
}

string handle_logout(string user) {
    auto itr = ACTIVE_PEERS.find(user);
    string status;
    if(itr == ACTIVE_PEERS.end()) {
        status = "You are not logged in\n";
    }
    else {
        ACTIVE_PEERS.erase((user));
        status = user+" logged out\n";
    }
    return status;
}

string handle_login(string user, string pswd) {
    string status;
    if(!ACTIVE_PEERS.empty() && ACTIVE_PEERS.find(user) != ACTIVE_PEERS.end()) {
        status = "You are already logged in\n";
    }
    else if(ALL_PEERS.empty() || ALL_PEERS.find(user) == ALL_PEERS.end()) {
        status = user+" does not exist";
    }
    else {
        if(ALL_PEERS[user].password == pswd) {
            ACTIVE_PEERS[user] = ALL_PEERS[user];
            status = "Login success\n";
        }
        else {
            status = "Wrong password\n";
        }
    }
    return status;
}

string handle_create_group(string gid, string user) {
    string status;
    if(GROUP_INFO.find(gid)!=GROUP_INFO.end()) {
        status = "Group "+gid+" already exists\n";
    }
    else {
        GroupStruct grpst;
        grpst.gid = gid;
        grpst.owner = user;
        grpst.members.push_back(user);
        GROUP_INFO[gid] = grpst;
        status = "Group "+gid+" created\n";
    }
    return status;
}

string handle_join_group(string gid, string user) {
    string status;
    if(GROUP_INFO.find(gid)==GROUP_INFO.end()) {
        status = "Group "+gid+" does not exist\n";
    }
    else {
        if(find(GROUP_INFO[gid].members.begin(),GROUP_INFO[gid].members.end(),user)!=GROUP_INFO[gid].members.end()) {
            status = "You are already a member of group "+gid+"\n";
        }
        else {
            GROUP_INFO[gid].members.push_back(user);
            status = "Joined group "+gid+"\n";
        }
    }
    return status;
}

string handle_upload_file(string filep, string gid, string ipadr, string port, string user, string nchunks, string fsize) {
    string status;
    if(ALL_PEERS.empty() || ALL_PEERS.find(user) == ALL_PEERS.end()) {
        status = user+" does not exist";
    }
    else if(GROUP_INFO.find(gid)==GROUP_INFO.end()) {
        status = "Group "+gid+" does not exist\n";
    }
    else if(find(GROUP_INFO[gid].members.begin(),GROUP_INFO[gid].members.end(),user)==GROUP_INFO[gid].members.end()) {
        status = "You are not a member of group "+gid+"\n";
    }
    else {
        int idx = filep.find_last_of('/');
        string filename = filep.substr(idx+1,filep.length());
        pair<string,string> fg = make_pair(gid,filename);
        if(FILE_INFO.find(fg) != FILE_INFO.end()) {
            status = "File "+filename+" is already shared in group "+gid+"\n";
        }
        else {
            int portno = stoi(string(port));
            long nchk = stol(nchunks);
            FileStruct flst;
            pair<string,int> psock = make_pair(ipadr,portno);
            flst.seeders[psock] = filep;
            flst.totalchunks = nchk;
            flst.filesize = stol(fsize);
            // calculate SHA1
            FILE_INFO[fg] = flst;
            status = "File "+filename+" is now uploaded to group "+gid+"\n";
        }
    }
    return status;
}

string handle_list_groups(string user) {
    string status;
    if(GROUP_INFO.empty()) {
        status = "No groups";
    }
    else {
        for(auto grp : GROUP_INFO) {
            status += grp.first+"\t";
        }
        status += "\n";
    }
    return status;
}

string handle_list_files(string gid, string user) {
    string status;
    if(GROUP_INFO.find(gid) == GROUP_INFO.end()) {
        status = "Group "+gid+" does not exist\n";
    }
    else if(find(GROUP_INFO[gid].members.begin(),GROUP_INFO[gid].members.end(),user)==GROUP_INFO[gid].members.end()) {
        status = "You are not a member of group "+gid+"\n";
    }
    else {
        for(auto fg : FILE_INFO) {
            if(fg.first.first == gid) {
                status += fg.first.second + "\t";
            }
        }
        if(status=="") {
            status = "No files shared";
        }
        status += "\n";
    }
    return status;
}

string handle_stop_share(string gid, string file, string ipadr, string port, string user) {
    string status;
    if(GROUP_INFO.find(gid)==GROUP_INFO.end()) {
        status = "Group "+gid+" does not exist\n";
    }
    else if(find(GROUP_INFO[gid].members.begin(),GROUP_INFO[gid].members.end(),user)==GROUP_INFO[gid].members.end()) {
        status = "You are not a member of group "+gid+"\n";
    }
    else {
        pair<string,string> fg = make_pair(gid,file);
        auto itr = FILE_INFO.find(fg);
        if(itr == FILE_INFO.end()) {
            status = "File "+file+" is not shared in group "+gid+"\n";
        }
        else {
            int portno = stoi(string(port));
            pair<string,int> psock = make_pair(ipadr,portno);
            bool flag = false;
            if(itr->second.seeders.find(psock) != itr->second.seeders.end()) {
                flag = true;
                itr->second.seeders.erase(psock);
            }
            if(itr->second.leechers.find(psock) != itr->second.leechers.end()) {
                flag = true;
                itr->second.leechers.erase(psock);
            }
            if(itr->second.seeders.empty() && itr->second.leechers.empty()) {
                FILE_INFO.erase(itr);
            }
            if(!flag) {
                status = "You do not have file "+file+"\n";
            }
            else {
                status = "File "+file+" is now unshared\n";
            }
        }
    }
    return status;
}

string handle_leave_group(string gid, string user, string ipadr, string port) {
    string status;
    cout << "in lg" << endl;
    if(GROUP_INFO.find(gid)==GROUP_INFO.end()) {
        status = "Group "+gid+" does not exist\n";
    }
    else if(find(GROUP_INFO[gid].members.begin(),GROUP_INFO[gid].members.end(),user)==GROUP_INFO[gid].members.end()) {
        status = "You are not a member of group "+gid+"\n";
    }
    else {
        pair<string,int> psock = make_pair(ipadr,stoi(port));
        if(GROUP_INFO[gid].owner == user) {
            status = "Admin cannot leave the group\n";
        }
        else {
//            string st;
            auto mitr = find(GROUP_INFO[gid].members.begin(),GROUP_INFO[gid].members.end(),user);
            for(auto it=FILE_INFO.begin();it!=FILE_INFO.end();it++) {
                if(it->first.first == gid) {
                    cout << handle_stop_share(gid,it->first.second,ipadr,port,user);
                }
            }
            GROUP_INFO[gid].members.erase(mitr);
            status = "You are removed from the group\n";
        }
    }
    cout << "exit lg" << endl;
    return status;
}

string handle_download_file(string gid, string fname, string user, int client_sock) {
//    cout << "in handle_download_file" << endl;
    string status;
    if(GROUP_INFO.find(gid)==GROUP_INFO.end()) {
        status = "Group "+gid+" does not exist\n";
    }
    else if(find(GROUP_INFO[gid].members.begin(),GROUP_INFO[gid].members.end(),user)==GROUP_INFO[gid].members.end()) {
        status = "You are not a member of group "+gid+"\n";
    }
    else {
        pair<string, string> fg = make_pair(gid, fname);
        auto itr = FILE_INFO.find(fg);
        if (itr == FILE_INFO.end()) {
            status = "File " + fname + " is not shared in group " + gid + "\n";
        }
        else {
            string sha = "sha";
            string fmsg = to_string(itr->second.totalchunks)+"|"+to_string(itr->second.filesize)+"|"+sha;
            send(client_sock,fmsg.c_str(),fmsg.length(),0);
            fmsg = "";
            vector<string> afpeers;
            for(auto sit=itr->second.seeders.begin();sit!=itr->second.seeders.end();sit++) {
                for (auto apit = ACTIVE_PEERS.begin(); apit != ACTIVE_PEERS.end(); apit++) {
                    if (apit->second.ip == sit->first.first && apit->second.port == sit->first.second) {
                        afpeers.push_back(sit->first.first+":"+to_string(sit->first.second));
                    }
                }
            }
            recv(client_sock,MSG_BUFF,BUFFER_SIZE,0);
            memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
            fmsg = *afpeers.begin();
            for(auto sit=afpeers.begin();sit!=afpeers.end();sit++) {
                if(sit == afpeers.begin())
                    continue;
                fmsg += "|"+(*sit);
            }
            afpeers.clear();
            for(auto lit=itr->second.leechers.begin();lit!=itr->second.leechers.end();lit++) {
                for (auto apit = ACTIVE_PEERS.begin(); apit != ACTIVE_PEERS.end(); apit++) {
                    if (apit->second.ip == lit->first.first && apit->second.port == lit->first.second) {
                        afpeers.push_back(lit->first.first+":"+to_string(lit->first.second));
                    }
                }
            }
            memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
            for(auto lit=afpeers.begin();lit!=afpeers.end();lit++) {
                fmsg += "|"+(*lit);
            }
            // send peer adddresses
            send(client_sock,fmsg.c_str(),fmsg.length(),0);
//            afpeers.clear();
//            fmsg = "";
//            send(client_sock,fmsg.c_str(),fmsg.length(),0);
            status = "File "+fname+" information sent\n";
            cout << status << endl;
            return status;
        }
    }
    send(client_sock,status.c_str(),status.length(),0);
//    cout << "exiting handle_download_file" << endl;
    return status;
}

string handle_add_leecher(string gid, string filename, string ipadr, string port, string destp) {
    pair<string,string> gf = make_pair(gid,filename);
    pair<string,int> lchinf = make_pair(ipadr,stoi(port));
    FILE_INFO[gf].leechers[lchinf] = destp;
    return "You are addded as leecher\n";
}

string handle_add_seeder(string gid, string filename, string ipadr, string port, string destp) {
    pair<string,string> gf = make_pair(gid,filename);
    pair<string,int> sedinf = make_pair(ipadr,stoi(port));
    FILE_INFO[gf].leechers.erase(sedinf);
    FILE_INFO[gf].seeders[sedinf] = destp;
    return "You are now a seeder\n";
}

string handle_remove_leecher(string gid, string filename, string ipadr, string port) {
    pair<string,string> gf = make_pair(gid,filename);
    pair<string,int> sedinf = make_pair(ipadr,stoi(port));
    FILE_INFO[gf].leechers.erase(sedinf);
    return "Removed from leechers\n";
}

void* serveRequest(void *args) {
//    cout << "here" << endl;
    int client_sock = *((int *)args);
    while(true) {
        recv(client_sock, MSG_BUFF, BUFFER_SIZE, 0);
        vector<string> rcvd_cmd = split_string(MSG_BUFF, '|');
        string send_msg;
        if (rcvd_cmd[0] == "create_user") {
            cout << rcvd_cmd[0] << " request" << endl;
            send_msg = handle_create_user(rcvd_cmd[1], rcvd_cmd[2], rcvd_cmd[3], rcvd_cmd[4]);
            send(client_sock,send_msg.c_str(),send_msg.length(),0);
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(rcvd_cmd[0] == "login") {
            cout << rcvd_cmd[0] << " request" << endl;
            send_msg = handle_login(rcvd_cmd[1], rcvd_cmd[2]);
            send(client_sock,send_msg.c_str(),send_msg.length(),0);
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(rcvd_cmd[0] == "logout") {
            cout << rcvd_cmd[0] << " request" << endl;
            send_msg = handle_logout(rcvd_cmd[1]);
            send(client_sock,send_msg.c_str(),send_msg.length(),0);
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(rcvd_cmd[0] == "create_group") {
            cout << rcvd_cmd[0] << " request" << endl;
            send_msg = handle_create_group(rcvd_cmd[1],rcvd_cmd[2]);
            send(client_sock,send_msg.c_str(),send_msg.length(),0);
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(rcvd_cmd[0] == "join_group") {
            cout << rcvd_cmd[0] << " request" << endl;
            send_msg = handle_join_group(rcvd_cmd[1],rcvd_cmd[2]);
            send(client_sock,send_msg.c_str(),send_msg.length(),0);
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(rcvd_cmd[0] == "upload_file") {
            cout << rcvd_cmd[0] << " request" << endl;
            send_msg = handle_upload_file(rcvd_cmd[1],rcvd_cmd[2],rcvd_cmd[3],rcvd_cmd[4],rcvd_cmd[5],rcvd_cmd[6],rcvd_cmd[7]);
            send(client_sock,send_msg.c_str(),send_msg.length(),0);
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(rcvd_cmd[0] == "list_groups") {
            cout << rcvd_cmd[0] << " request" << endl;
            send_msg = handle_list_groups(rcvd_cmd[1]);
            send(client_sock,send_msg.c_str(),send_msg.length(),0);
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(rcvd_cmd[0] == "list_files") {
            cout << rcvd_cmd[0] << " request" << endl;
            send_msg = handle_list_files(rcvd_cmd[1],rcvd_cmd[2]);
            send(client_sock,send_msg.c_str(),send_msg.length(),0);
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(rcvd_cmd[0] == "stop_share") {
            cout << rcvd_cmd[0] << " request" << endl;
            send_msg = handle_stop_share(rcvd_cmd[1],rcvd_cmd[2],rcvd_cmd[3],rcvd_cmd[4],rcvd_cmd[5]);
            send(client_sock,send_msg.c_str(),send_msg.length(),0);
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(rcvd_cmd[0] == "leave_group") {
            cout << rcvd_cmd[0] << " request" << endl;
            send_msg = handle_leave_group(rcvd_cmd[1],rcvd_cmd[2],rcvd_cmd[3],rcvd_cmd[4]);
            send(client_sock,send_msg.c_str(),send_msg.length(),0);
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(rcvd_cmd[0] == "download_file") {
            cout << rcvd_cmd[0] << " request" << endl;
            send_msg = handle_download_file(rcvd_cmd[1],rcvd_cmd[2],rcvd_cmd[5],client_sock);
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(rcvd_cmd[0] == "add_leecher") {
            cout << rcvd_cmd[0] << " request" << endl;
            send_msg = handle_add_leecher(rcvd_cmd[1],rcvd_cmd[2],rcvd_cmd[3],rcvd_cmd[4],rcvd_cmd[5]);
            send(client_sock,send_msg.c_str(),send_msg.length(),0);
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(rcvd_cmd[0] == "add_seeder") {
            cout << rcvd_cmd[0] << " request" << endl;
            send_msg = handle_add_seeder(rcvd_cmd[1],rcvd_cmd[2],rcvd_cmd[3],rcvd_cmd[4],rcvd_cmd[5]);
            send(client_sock,send_msg.c_str(),send_msg.length(),0);
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(rcvd_cmd[0] == "remove_leecher") {
            cout << rcvd_cmd[0] << " request" << endl;
            send_msg = handle_remove_leecher(rcvd_cmd[1],rcvd_cmd[2],rcvd_cmd[3],rcvd_cmd[4]);
            send(client_sock,send_msg.c_str(),send_msg.length(),0);
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(rcvd_cmd[0] == "exit") {
            cout << rcvd_cmd[0] << " request" << endl;
            send_msg = handle_logout(rcvd_cmd[1]);
            send(client_sock,send_msg.c_str(),BUFFER_SIZE,0);
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
            break;
        }
    }
    pthread_exit(NULL);
}

int main(int argc,char ** argv) {
    if(argc<2) {
        cout << "Usage : tracker.cpp <tracker_file>" << endl;
        return 0;
    }
    setSocket(argv[1]);
//    cout << "in server" << endl;
    struct sockaddr_in serverSock;
    int socketfd = socket(PF_INET, SOCK_STREAM, 0);
    if(socketfd < 0)	//tcp -sock_stream  af_inet - ipv4
    {
        perror("\nSocket failed ");
        exit(1);
    }
    cout<<"Socket created\n";
    memset(&serverSock, '\0', sizeof(serverSock));
    serverSock.sin_family = AF_INET;
    serverSock.sin_addr.s_addr = inet_addr(THIS_TRACK_SOCK.first.c_str());
    unsigned int number = (unsigned short) THIS_TRACK_SOCK.second;
    serverSock.sin_port = htons(number);
    if(bind(socketfd,(struct sockaddr*) &serverSock, sizeof(serverSock)) < 0) {
        perror("\nError in binding ");
        exit(1);
    }
    if(listen(socketfd,QLIMIT) < 0) {
        perror("\nError in listen ");
        exit(1);
    }
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_sock;
    int i=0;
    while(true) {
        if (client_sock = accept(socketfd, (struct sockaddr *) &client_addr, &addr_len)) {
            //send(newsock,str,strlen(str),0);
            cout << "Connected to " << client_addr.sin_addr.s_addr << " : " << client_addr.sin_port << endl;
            if (pthread_create(&peer_TID[i++], NULL, serveRequest, &client_sock) != 0) {
                perror("\nFailed to create server request service thread ");
            }

        }
    }
    pthread_exit(NULL);
}

