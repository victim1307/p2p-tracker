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
#include <semaphore.h>

pthread_t tid_ps;
pthread_t tid_pr;

struct ChunkStruct {
    string dpath;
    long totchunks;
    vector<int> fchunks;
};

struct DownlConfig {
    string gid;
    string srcfile;
    string destp;
    int totchunks;
    int dl_sock;
    long file_size;
    pair<string,int> chunks_from;
    vector<int> which_chunks;
};

pair<string,int> THIS_PEER_SOCK;
vector<pair<string,int>> TRACK_SOCK_VEC;
char MSG_BUFF[BUFFER_SIZE];
string CURR_USER="";
map<pair<string,string>,ChunkStruct> FILE_CHUNKS_INFO;  //<gid,filename>
sem_t m;

void* peerServer(void *args) {
//    cout << "in server" << endl;
    struct sockaddr_in serverSock;
    int socketfd = socket(PF_INET, SOCK_STREAM, 0);
    if(socketfd < 0) {
        perror("socket failed\n");
        exit(1);
    }
    cout<<"Socket created\n";
    memset(&serverSock, '\0', sizeof(serverSock));
    serverSock.sin_family = AF_INET;
    serverSock.sin_addr.s_addr = inet_addr(THIS_PEER_SOCK.first.c_str());
    unsigned int portno = (unsigned short) THIS_PEER_SOCK.second;
    serverSock.sin_port = htons(portno);
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
    while(true) {
        if(client_sock = accept(socketfd, (struct sockaddr *) &client_addr, &addr_len)) {
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
            recv(client_sock, MSG_BUFF, BUFFER_SIZE, 0);
            vector<string> rmsg = split_string(MSG_BUFF, '|');
            cout << "Request for gid=" + rmsg[0] + " file=" + rmsg[1] << endl;
            pair<string, string> pgf = make_pair(rmsg[0], rmsg[1]);
            cout << FILE_CHUNKS_INFO[pgf].fchunks.size() << endl;
            string bvec = bitvec_toString(FILE_CHUNKS_INFO[pgf].fchunks);
            send(client_sock, bvec.c_str(), bvec.length(), 0);
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
            recv(client_sock, MSG_BUFF, BUFFER_SIZE, 0);
            cout << MSG_BUFF << endl;
            rmsg = split_string(MSG_BUFF, '|');
            vector<int> chunks_tosend;
            vector<string> chkmsg = split_string(rmsg[1], ';');
            for (int i = 0; i < chkmsg.size(); i++) {
                chunks_tosend.push_back(stoi(chkmsg[i]));
//                cout << chunks_tosend[i] << " ";
            }
//            cout << endl;
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
            string fpath = FILE_CHUNKS_INFO[pgf].dpath + "/" + pgf.second;
            FILE *fp = fopen(fpath.c_str(), "rb");
            if (fp == NULL) {
                perror("\nFile null");
            }
            char CHUNK_BUFF[CHUNK_SIZE];
            long readsize, fs;
            cout << fpath << " " << chunks_tosend.size() << " " << pgf.second << endl;
//            for(long ci=0;ci<chunks_tosend.size();ci++) {
//                memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
//                recv(client_sock,MSG_BUFF,BUFFER_SIZE,0);
//                memset(&CHUNK_BUFF, 0, sizeof(CHUNK_BUFF));
////                cout << "Request for chunk " << MSG_BUFF << " ........ " << "Sending chunk " << chunks_tosend[ci] << " --> ";
//                fs = fseek(fp,chunks_tosend[ci]*CHUNK_SIZE,SEEK_SET);
//                if(fs != 0) {
//                    perror("\nseek nonzero ");
//                }
//                readsize = fread(&CHUNK_BUFF,sizeof(char),CHUNK_SIZE,fp);
////                cout << readsize << endl;
////                cout << CHUNK_BUFF << endl;
//                if(readsize <= 0) {
//                    perror("\nnot read ");
//                }
//                send(client_sock,CHUNK_BUFF,readsize,0);
//            }
            while (true) {
                memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
                memset(&CHUNK_BUFF, 0, sizeof(CHUNK_BUFF));
                recv(client_sock, MSG_BUFF, BUFFER_SIZE, 0);
//                cout << "Request for chunk " << MSG_BUFF << " ........ ";
                if (string(MSG_BUFF) == "bye") {
                    break;
                }
                long cn = stol(MSG_BUFF);
                fs = fseek(fp, cn * CHUNK_SIZE, SEEK_SET);
                if (fs != 0) {
                    perror("\nseek error ");
                }
                readsize = fread(&CHUNK_BUFF, sizeof(char), CHUNK_SIZE, fp);
//                cout << "Sending chunk " << cn << "-->" << readsize << endl;
//                cout << cn << "=" << readsize << "\t";
                if (readsize < 0) {
                    perror("\nnot read ");
                }
                send(client_sock, CHUNK_BUFF, readsize, 0);
            }
            cout << endl << "All chunks sent" << endl;
            memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
            close(client_sock);
        }
    }
    pthread_exit(NULL);
}

void* fileDownloader(void *args) {
//    cout << "in fileDownloader" << endl;
    DownlConfig down_config = *((DownlConfig *)args);
    cout << down_config.chunks_from.first << ":" << down_config.chunks_from.second << endl;
    cout << endl;
    string chunks = bitvec_toString(down_config.which_chunks);
//    struct sockaddr_in cpeerSock;
//    memset(&cpeerSock, '\0', sizeof(cpeerSock));
//    cpeerSock.sin_addr.s_addr = inet_addr(down_config.chunks_from.first.c_str());
//    cpeerSock.sin_port = htons(down_config.chunks_from.second);
//    cpeerSock.sin_family = AF_INET;
//    sendto(down_config.dl_sock,chnkmsg.c_str(),chnkmsg.length()+1,0,(struct sockaddr*) &cpeerSock,sizeof(cpeerSock));
    string chnkmsg = to_string(down_config.totchunks)+"|"+chunks;
    cout << chnkmsg << endl;
    send(down_config.dl_sock,chnkmsg.c_str(),chnkmsg.length()+1,0);
    memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
    sem_wait(&m);
    int rcvlen;
    string dfpath = down_config.destp+"/"+down_config.srcfile;
    cout << dfpath << endl;
    pair<string,string> gf = make_pair(down_config.gid,down_config.srcfile);
    if (FILE *file = fopen(dfpath.c_str(), "r")) {
        fclose(file);
//        cout << "file exists" << endl;
    }
    else {
//        cout << "file does not exist" << endl;
        FILE *fp = fopen(dfpath.c_str(),"w");
        if (fallocate(fileno(fp),0,0,down_config.file_size)!=0) {
            perror("\n fallocate : ");
        }
        fclose(fp);
    }
    FILE *fin = fopen(dfpath.c_str(),"rb+");
//    for(int ci=0;ci<down_config.which_chunks.size();ci++) {
//        char CHUNK_BUFF[CHUNK_SIZE];
//        memset(&CHUNK_BUFF, '\0', CHUNK_SIZE);
//        string chnkno = to_string(down_config.which_chunks[ci]);
//        send(down_config.dl_sock,chnkno.c_str(),chnkno.length()+1,0);
////        cout << "Downloading chunk " << down_config.which_chunks[ci] << " ........ ";
//        rcvlen = recv(down_config.dl_sock, CHUNK_BUFF, CHUNK_SIZE, 0);
////        cout << "Recieved chunk " << down_config.which_chunks[ci] << " " << rcvlen << endl;
////        cout << CHUNK_BUFF << endl;
//        string to_write = string(CHUNK_BUFF);
//        fseek(fin,down_config.which_chunks[ci]*CHUNK_SIZE,SEEK_SET);
//        fwrite(CHUNK_BUFF,sizeof(char),rcvlen,fin);
//        FILE_CHUNKS_INFO[gf].fchunks.push_back(down_config.which_chunks[ci]);
//    }
    long ci=0;
    while(true) {
        if(ci==down_config.which_chunks.size()) {
            break;
        }
        char CHUNK_BUFF[CHUNK_SIZE];
        memset(&CHUNK_BUFF, '\0', CHUNK_SIZE);
        string chnkno = to_string(down_config.which_chunks[ci]);
//        cout << "Downloading chunk " << chnkno << " ........ ";
        send(down_config.dl_sock,chnkno.c_str(),chnkno.length()+1,0);
        rcvlen = recv(down_config.dl_sock, CHUNK_BUFF, CHUNK_SIZE, 0);
//        cout << ci << "=" << rcvlen << "  ";
//        cout << "Recieved chunk " << down_config.which_chunks[ci] << " " << rcvlen << endl;
        if(ci<down_config.which_chunks.size()-1 && rcvlen<down_config.file_size &&  rcvlen<CHUNK_SIZE) {
            continue;
//            string errmsg = "bad|"+to_string(down_config.which_chunks[ci]);
//            send(down_config.dl_sock,errmsg.c_str(),errmsg.length()+1,0);
        }
        fseek(fin,down_config.which_chunks[ci]*CHUNK_SIZE,SEEK_SET);
        fwrite(CHUNK_BUFF,sizeof(char),rcvlen,fin);
        FILE_CHUNKS_INFO[gf].fchunks.push_back(down_config.which_chunks[ci]);
        ci++;
    }
    string bye = "bye";
    send(down_config.dl_sock,bye.c_str(),bye.length()+1,0);
    memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
    fclose(fin);
    sem_post(&m);
//    cout << "exiting fileDownloader" << endl;
    close(down_config.dl_sock);
    pthread_exit(NULL);
}

void downloadConfigure(string gid, string filename, string destp, int totalchunks, long fsize, string sha, vector<pair<string,int>> active_peer_chunks) {
    cout << "in downloadConfigure" << endl;
    map<pair<string,int>,vector<int>> chunks_peers_have;
    map<pair<string,int>,int> peer_DlSocks;
    for(auto apcit=active_peer_chunks.begin();apcit!=active_peer_chunks.end();apcit++) {
        int DlSock = socket(PF_INET,SOCK_STREAM, 0);
        if(DlSock < 0) {
            perror("\nFailed to create downloader socket ");
            exit(1);
        }
        // connect & request for chunk info
        cout << "connecting " << apcit->first << ":" << apcit->second << endl;
        struct sockaddr_in peerSock;
        memset(&peerSock, '\0', sizeof(peerSock));
        peerSock.sin_addr.s_addr = inet_addr(apcit->first.c_str());
        peerSock.sin_port = htons(apcit->second);
        peerSock.sin_family = AF_INET;
        if(connect(DlSock,(struct sockaddr*) &peerSock,sizeof(peerSock)) < 0) {
            perror("\nFailed to connect to peer");
        }
//        cout << "connected to " << apcit->first << ":" << apcit->second << " --> ";
        string send_msg = gid+"|"+filename;
        memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
        send(DlSock,send_msg.c_str(),send_msg.length()+1,0);
        recv(DlSock,MSG_BUFF,BUFFER_SIZE,0);
//        cout << MSG_BUFF << endl;
        chunks_peers_have[*apcit] = split_bitvector(MSG_BUFF,';',totalchunks);
        peer_DlSocks[*apcit] = DlSock;
        memset(&MSG_BUFF, 0, sizeof(MSG_BUFF));
    }
    // loop to choose chunks from each peer
    map<pair<string,int>,vector<int>> pick_chunks_from;
    int ci=0;
    int pcnt=0;
    while(ci<totalchunks) {
        for(auto apcit=chunks_peers_have.begin();apcit!=chunks_peers_have.end();apcit++) {
            if(pcnt==chunks_peers_have.size() && apcit->second[ci]==0) {
                cout << "Could not find chunk " << ci << endl;
                return;
            }
            else if(apcit->second[ci]==0) {
                pcnt++;
                continue;
            }
            else {
                pick_chunks_from[apcit->first].push_back(ci);
                ci++;
                pcnt = 0;
            }
        }
    }
    // vector of DownConfig
    vector<DownlConfig> downcfg_vec;
    for(auto cfit=pick_chunks_from.begin();cfit!=pick_chunks_from.end();cfit++) {
        DownlConfig down_config;
        down_config.totchunks = totalchunks;
        down_config.srcfile = filename;
        down_config.gid = gid;
        down_config.destp = destp;
        down_config.file_size = fsize;
        down_config.dl_sock = peer_DlSocks[cfit->first];
        down_config.chunks_from = cfit->first;
        down_config.which_chunks = cfit->second;
        downcfg_vec.push_back(down_config);
    }
    // create thread for each peer
    int d = 0;
    pthread_t downl_TID[downcfg_vec.size()];
    for(auto vsix=0;vsix<downcfg_vec.size();vsix++) {
        if (pthread_create(&downl_TID[vsix], NULL, fileDownloader, &downcfg_vec[vsix]) != 0) {
            perror("\nFailed to create downloader thread ");
        }
        pthread_join(downl_TID[vsix],NULL);
    }
//    cout << "exiting downloadConfigure" << endl;
}

int main(int argc,char ** argv) {
    sem_init(&m,0,1);
    if(argc<3) {
        cout << "Usage : peer.cpp <peer IP:Port> <tracker_file>" << endl;
        return 0;
    }
    string tracker_file = argv[2];
    THIS_PEER_SOCK = split_address(argv[1]);
    fstream fs(tracker_file,ios::in);
    string tadr;
    while(getline(fs,tadr)) {
        TRACK_SOCK_VEC.push_back(split_address(tadr));
    }
    if(pthread_create(&tid_ps, NULL, peerServer, NULL)!= 0) {
        perror("Failed to create server thread\n");
    }

    int clientSock = socket(PF_INET,SOCK_STREAM, 0);
    if(clientSock < 0) {
        perror("\nFailed to create client socket ");
        exit(1);
    }

    struct sockaddr_in trackerSock;
    memset(&trackerSock, '\0', sizeof(trackerSock));
    trackerSock.sin_addr.s_addr = inet_addr(TRACK_SOCK_VEC[0].first.c_str());
    trackerSock.sin_port = htons(TRACK_SOCK_VEC[0].second);
    trackerSock.sin_family = AF_INET;

    if(connect(clientSock,(struct sockaddr*) &trackerSock,sizeof(trackerSock)) < 0) {
        perror("\nFailed to connect to tracker");
    }

    string rqst;
    while(true) {
        getline(cin,rqst);
        vector<string> rqst_vec = split_string(rqst,' ');
        string cmd = rqst_vec[0];
        if(cmd == "create_user") {
            if(rqst_vec.size()<3) {
                cout << "Usage : create_user <username> <password>" << endl;
                continue;
            }
            string cmd_params = rqst_vec[0]+"|"+rqst_vec[1]+"|"+rqst_vec[2]+"|"+THIS_PEER_SOCK.first+"|"+to_string(THIS_PEER_SOCK.second);
            send(clientSock,cmd_params.c_str(),cmd_params.length()+1,0);
            recv(clientSock,MSG_BUFF,BUFFER_SIZE,0);
            string rcvd_msg = string(MSG_BUFF);
            cout << rcvd_msg << endl;
            memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(cmd == "login") {
            if(rqst_vec.size()<3) {
                cout << "Usage : login <username> <password>" << endl;
                continue;
            }
            if(CURR_USER!="") {
                cout << "You are already Logged in" << endl;
                continue;
            }
            string cmd_params = rqst_vec[0]+"|"+rqst_vec[1]+"|"+rqst_vec[2];
            send(clientSock,cmd_params.c_str(),cmd_params.length()+1,0);
            recv(clientSock,MSG_BUFF,BUFFER_SIZE,0);
            string rcvdmsg = string(MSG_BUFF);
            cout << rcvdmsg << endl;
            if(rcvdmsg == "Login success\n")
                CURR_USER = rqst_vec[1];
            memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(cmd == "logout") {
            if(CURR_USER=="") {
                cout << "You are not logged in" << endl;
                continue;
            }
            string cmd_params = rqst_vec[0]+"|"+CURR_USER;
            send(clientSock,cmd_params.c_str(),cmd_params.length()+1,0);
            recv(clientSock,MSG_BUFF,BUFFER_SIZE,0);
            cout << MSG_BUFF << endl;
            CURR_USER = "";
            memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(cmd == "create_group") {
            if(rqst_vec.size()<2) {
                cout << "Usage : create_group <groupname>" << endl;
                continue;
            }
            if(CURR_USER=="") {
                cout << "You are not logged in" << endl;
                continue;
            }
            string cmd_params = rqst_vec[0]+"|"+rqst_vec[1]+"|"+CURR_USER;
            send(clientSock,cmd_params.c_str(),cmd_params.length()+1,0);
            recv(clientSock,MSG_BUFF,BUFFER_SIZE,0);
            cout << MSG_BUFF << endl;
            memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(cmd == "join_group") {
            if(rqst_vec.size()<2) {
                cout << "Usage : join_group <groupname>" << endl;
                continue;
            }
            if(CURR_USER=="") {
                cout << "You are not logged in" << endl;
                continue;
            }
            string cmd_params = rqst_vec[0]+"|"+rqst_vec[1]+"|"+CURR_USER;
            send(clientSock,cmd_params.c_str(),cmd_params.length()+1,0);
            recv(clientSock,MSG_BUFF,BUFFER_SIZE,0);
            cout << MSG_BUFF << endl;
            memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(cmd == "upload_file") {
            if(rqst_vec.size()<3) {
                cout << "Usage : upload_file <filepath> <group>" << endl;
                continue;
            }
            if(CURR_USER=="") {
                cout << "You are not logged in" << endl;
                continue;
            }
            struct stat filestatus;
            stat(rqst_vec[1].c_str(), &filestatus);
            long fsz = filestatus.st_size;
            int totchunks = ceil((float)fsz/(CHUNK_SIZE));
            string cmd_params = rqst_vec[0]+"|"+rqst_vec[1]+"|"+rqst_vec[2]+"|"+THIS_PEER_SOCK.first+"|"+to_string(THIS_PEER_SOCK.second)+"|"+CURR_USER+"|"+to_string(totchunks)+"|"+to_string(fsz);
            send(clientSock,cmd_params.c_str(),cmd_params.length()+1,0);
            recv(clientSock,MSG_BUFF,BUFFER_SIZE,0);
            cout << MSG_BUFF << endl;
            string rspmsg = string(MSG_BUFF);
            int idx = rqst_vec[1].find_last_of('/');
            string filename = rqst_vec[1].substr(idx+1,rqst_vec[1].length());
            pair<string,string> gf = make_pair(rqst_vec[2],filename);
            if(rspmsg.find("is now uploaded to group") != string::npos) {
                ChunkStruct chst;
                chst.dpath = rqst_vec[1].substr(0,idx);
                chst.totchunks = totchunks;
                for(int i=0;i<totchunks;i++) {
                    chst.fchunks.push_back(i);
                }
                FILE_CHUNKS_INFO[gf] = chst;
            }
            cout << "File-size: " << fsz << "\tTotal-chunks: " << totchunks << endl;
//            for(auto itr=FILE_CHUNKS_INFO[gf].fchunks.begin();itr!=FILE_CHUNKS_INFO[gf].fchunks.end();itr++) {
//                cout << *itr << " ";
//            }
//            cout << endl;
            memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(cmd == "list_groups") {
            if(CURR_USER=="") {
                cout << "You are not logged in" << endl;
                continue;
            }
            string cmd_params = rqst_vec[0]+"|"+CURR_USER;
            send(clientSock,cmd_params.c_str(),cmd_params.length()+1,0);
            recv(clientSock,MSG_BUFF,BUFFER_SIZE,0);
            cout << MSG_BUFF << endl;
            memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(cmd == "list_files") {
            if(rqst_vec.size()<2) {
                cout << "Usage : list_files <group>" << endl;
                continue;
            }
            if(CURR_USER=="") {
                cout << "You are not logged in" << endl;
                continue;
            }
            string cmd_params = rqst_vec[0]+"|"+rqst_vec[1]+"|"+CURR_USER;
            send(clientSock,cmd_params.c_str(),cmd_params.length()+1,0);
            recv(clientSock,MSG_BUFF,BUFFER_SIZE,0);
            cout << MSG_BUFF << endl;
            memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(cmd == "stop_share") {
            if(rqst_vec.size()<3) {
                cout << "Usage : stop_share <group> <filename>" << endl;
                continue;
            }
            if(CURR_USER=="") {
                cout << "You are not logged in" << endl;
                continue;
            }
            string cmd_params = rqst_vec[0]+"|"+rqst_vec[1]+"|"+rqst_vec[2]+"|"+THIS_PEER_SOCK.first+"|"+to_string(THIS_PEER_SOCK.second)+"|"+CURR_USER;
            send(clientSock,cmd_params.c_str(),cmd_params.length()+1,0);
            recv(clientSock,MSG_BUFF,BUFFER_SIZE,0);
            cout << MSG_BUFF << endl;
            memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(cmd == "download_file") {
            if(rqst_vec.size()<4) {
                cout << "Usage : download_file <group> <filename> <destination_path>" << endl;
                continue;
            }
            if(CURR_USER=="") {
                cout << "You are not logged in" << endl;
                continue;
            }
            pair<string,string> chkgf = make_pair(rqst_vec[1],rqst_vec[2]);
            if(FILE_CHUNKS_INFO.find(chkgf)!=FILE_CHUNKS_INFO.end()) {
                cout << "You already have the file" << endl;
                continue;
            }
            string fDestn = rqst_vec[3];
            string cmd_params = rqst_vec[0]+"|"+rqst_vec[1]+"|"+rqst_vec[2]+"|"+THIS_PEER_SOCK.first+"|"+to_string(THIS_PEER_SOCK.second)+"|"+CURR_USER;
            send(clientSock,cmd_params.c_str(),cmd_params.length()+1,0);
            // total_chunks | file_size | SHA
            recv(clientSock,MSG_BUFF,BUFFER_SIZE,0);
            if(string(MSG_BUFF).find("is not shared in group") != string::npos) {
                cout << MSG_BUFF << endl;
                memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
                continue;
            }
            cout << MSG_BUFF << endl;
            vector<string> vrmsg = split_string(MSG_BUFF,'|');
            int totchunks = stoi(vrmsg[0]);
            long fsize = stol(vrmsg[1]);
            string sha = vrmsg[1];
            memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
            string ack = "send peers";
            send(clientSock,ack.c_str(),ack.length()+1,0);
            // peer addresses
            recv(clientSock,MSG_BUFF,BUFFER_SIZE,0);
            cout << "peers ";
            cout << MSG_BUFF << endl;
            vrmsg.clear();
            vrmsg = split_string(MSG_BUFF,'|');
            if(vrmsg.empty()) {
                cout << "No peers available" << endl;
                continue;
            }
            vector<pair<string,int>> afpeers;
            for(auto it=vrmsg.begin();it!=vrmsg.end();it++) {
                afpeers.push_back(split_address(*it));
            }
            memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
            pair<string,string> gf = make_pair(rqst_vec[1],rqst_vec[2]);
            FILE_CHUNKS_INFO[gf].dpath = rqst_vec[3];
            FILE_CHUNKS_INFO[gf].totchunks = totchunks;
            string lchrq = "add_leecher|"+rqst_vec[1]+"|"+rqst_vec[2]+"|"+THIS_PEER_SOCK.first+"|"+to_string(THIS_PEER_SOCK.second)+"|"+rqst_vec[3];
            send(clientSock,lchrq.c_str(),lchrq.length()+1,0);
            recv(clientSock,MSG_BUFF,BUFFER_SIZE,0);
            cout << MSG_BUFF << endl;
            memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
            downloadConfigure(rqst_vec[1],rqst_vec[2],rqst_vec[3],totchunks,fsize,sha,afpeers);
            if(FILE_CHUNKS_INFO[gf].fchunks.size() == totchunks) {
                cout << "Download complete" << endl;
                string sedq = "add_seeder|"+rqst_vec[1]+"|"+rqst_vec[2]+"|"+THIS_PEER_SOCK.first+"|"+to_string(THIS_PEER_SOCK.second)+"|"+rqst_vec[3];
                send(clientSock,sedq.c_str(),sedq.length()+1,0);
                recv(clientSock,MSG_BUFF,BUFFER_SIZE,0);
                cout << MSG_BUFF << endl;
            }
            else {
                string rlmsg = "remove_leecher|"+gf.first+"|"+gf.second+"|"+THIS_PEER_SOCK.first+"|"+to_string(THIS_PEER_SOCK.second);
                send(clientSock,rlmsg.c_str(),rlmsg.length()+1,0);
                recv(clientSock,MSG_BUFF,BUFFER_SIZE,0);
                cout << MSG_BUFF << endl;

            }
            memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(cmd == "show_downloads") {
            if(FILE_CHUNKS_INFO.empty()) {
                cout << "No files downloaded" << endl;
                continue;
            }
            for(auto itr=FILE_CHUNKS_INFO.begin();itr!=FILE_CHUNKS_INFO.end();itr++) {
                if(itr->second.totchunks == itr->second.fchunks.size()) {
                    cout << "C  ";
                }
                else {
                    cout << "D  ";
                }
                cout << itr->first.first << "  " << itr->first.second << endl;
            }
        }
        else if(cmd == "leave_group") {
            if(rqst_vec.size()<2) {
                cout << "Usage : leave_group <group>" << endl;
                continue;
            }
            if(CURR_USER=="") {
                cout << "You are not logged in" << endl;
                continue;
            }
            string rqmsg = rqst_vec[0]+"|"+rqst_vec[1]+"|"+CURR_USER+"|"+THIS_PEER_SOCK.first+"|"+to_string(THIS_PEER_SOCK.second);
//            cout << "msg=" << rqmsg << endl;
            send(clientSock,rqmsg.c_str(),rqmsg.length()+1,0);
            recv(clientSock,MSG_BUFF,BUFFER_SIZE,0);
            cout << MSG_BUFF << endl;
            memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
        }
        else if(cmd == "exit") {
            if(CURR_USER=="") {
                break;
            }
            string cmd_params = rqst_vec[0]+"|"+CURR_USER;
            send(clientSock,cmd_params.c_str(),cmd_params.length()+1,0);
            recv(clientSock,MSG_BUFF,BUFFER_SIZE,0);
            cout << MSG_BUFF << endl;
            CURR_USER = "";
            memset(MSG_BUFF, 0, sizeof(MSG_BUFF));
            break;
        }
        else {
            cout << "Wrong command" << endl;
        }
    }
    return 0;
}
