#include <iostream>
#include <bits/stdc++.h>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <thread>
#define BUF_SIZE 524288
using namespace std;

/* -------------------------------------------------------------------------- */
/*                              // ANCHOR GLOBALS                             */
/* -------------------------------------------------------------------------- */
int PEER_SERVER_PORT;
string PEER_ADDR;
int TRACKER_PORT;
int is_logged = false;
string curent_user_id;
unordered_map<string, string> file_paths; // original to donwloaded paths
unordered_map<string, bool> shared_files;
unordered_map<string, bool> in_downloaded_files;
unordered_map<string, bool> downloaded_files;
unordered_map<string, vector<int>> shared_files_chunk_info; // filename->bitvector

/* -------------------------------------------------------------------------- */
/*                              // ANCHOR HELPERS                             */
/* -------------------------------------------------------------------------- */

// clears screen
void clrscr(void)
{
    cout << "\033[2J\033[1;1H";
}

// vector stringify
string vector_stringify(vector<int> input, bool with_space = false)
{
    string res = "";
    for (auto i : input)
    {
        res += to_string(i);
        if (with_space)
        {
            res += " ";
        }
    }
    return res;
}

// split string with delimiter
vector<string> split_string(string text, string seperator)
{
    vector<string> tokens;
    int position;
    while ((position = text.find(seperator)) != string::npos)
    {
        tokens.push_back(text.substr(0, position));
        text.erase(0, position + seperator.length());
    }
    tokens.push_back(text);
    return tokens;
}

// calculate file size in bytes
long long int calc_file_size(string filename)
{
    long long int size;
    struct stat st;
    stat(filename.c_str(), &st);
    size = st.st_size;
    return size;
}

// calculates SHA and returns string hash
string calcSHA(string file_buffer_string)
{
    string hash;
    unsigned char md[20];
    if (!SHA1(reinterpret_cast<const unsigned char *>(&file_buffer_string[0]), file_buffer_string.length(), md))
    {
        cout << "Error in hashing\n";
    }
    else
    {
        for (int i = 0; i < 20; i++)
        {
            char buffer[3];
            sprintf(buffer, "%02x", md[i] & 0xff);
            hash += string(buffer);
        }
    }
    return hash;
}

// breaks the input command and returns a vector of tokens
vector<string> tokenize(string command)
{
    vector<string> args;
    string token;
    stringstream command_stream(command);
    while (command_stream >> token)
    {
        args.emplace_back(token);
    }
    return args;
}

// sets the shared_files_chunk_info
void set_chunk_vector(string file_name, long long chunk, long long piece_count, bool upload = false)
{
    if (upload)
    {
        vector<int> tmp(piece_count, 1);
        shared_files_chunk_info[file_name] = tmp;
    }
    else
    {
        if (shared_files_chunk_info[file_name].size() == 0)
        {
            vector<int> tmp(piece_count, 0);
            shared_files_chunk_info[file_name] = tmp;
        }
        shared_files_chunk_info[file_name][chunk] = 1;
    }
}

// peer-tracker connection establishment helper
void handshake_with_tracker(int peer_client_socket_fd, sockaddr_in &address)
{
    address.sin_family = AF_INET;
    address.sin_port = htons(TRACKER_PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) <= 0)
    {
        perror("[-]Tracker Connection failed\n");
        exit(EXIT_FAILURE);
    }
    int tracker_connection;
    if (tracker_connection = connect(peer_client_socket_fd, (sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("[-]Connection to tracker failed\n");
        exit(EXIT_FAILURE);
    }
    cout << "[*]Tracker Handshake Completed\n";
}

// actual sending of command request from peer to tracker, Used in handlers
void request_to_tracker(int peer_client_socket_fd, sockaddr_in &address, string message)
{
    address.sin_family = AF_INET;
    address.sin_port = htons(TRACKER_PORT);
    send(peer_client_socket_fd, message.c_str(), message.length(), 0);
    // cout << "[*]Request sent to tracker\n";
}

// capturing the response of tracker into string, Used in handlers
string response_from_tracker(int peer_client_socket_fd)
{
    char res[BUF_SIZE] = {0};
    int res_len = recv(peer_client_socket_fd, res, BUF_SIZE, 0);
    res[res_len] = '\0';
    return res;
}

// writing of a file which transfered to server helper
// void write_file_from_client(int socket_of_request, int port)
// {
//     char read_buffer[BUF_SIZE] = {0};
//     string dest = "dest.txt" + to_string(port);
//     FILE *d = fopen(dest.c_str(), "wb");
//     if (d == NULL)
//     {
//         perror("[-]Error creating the file\n");
//         exit(EXIT_FAILURE);
//     }
//     int size_of_message;
//     while (1)
//     {
//         memset(read_buffer, 0, BUF_SIZE);
//         size_of_message = read(socket_of_request, read_buffer, BUF_SIZE);
//         string read_buffer_string = read_buffer;
//         if (read_buffer_string == "END")
//         {
//             break;
//         }
//         if (size_of_message <= 0)
//         {
//             cout << "[*]Finished writing messaage\n";
//             break;
//         }
//         cout << "[*]Block of " << size_of_message << " recieved from clent\n";
//         fwrite(read_buffer, 1, size_of_message, d);
//     }
//     fclose(d);
//     return;
// }

// writing of a file which transfered to server helper
// void write_file_from_client2(int socket_of_request, int port)
// {
//     int download_file = open(("down" + to_string(port)).c_str(), O_WRONLY | O_CREAT, 0644);
//     if (download_file < 0)
//     {
//         cout << "unable to create torrent file\n";
//         return;
//     }
//     int size_of_message;
//     char read_buffer[BUF_SIZE] = {0};
//     while ((size_of_message = recv(socket_of_request, read_buffer, BUF_SIZE, 0)) > 0)
//     {
//         string read_buffer_string = read_buffer;
//         if (read_buffer_string == "END")
//         {
//             break;
//         }
//         write(download_file, read_buffer, size_of_message);
//         memset(read_buffer, 0, BUF_SIZE);
//     }
//     cout << "done downloading torrent\n";
//     close(download_file);
//     return;
// }

// // send a file from peer server to peer client
// void send_file_to_peer(int socket_of_request, int port, string filename)
// {
//     int f = open(filename.c_str(), O_RDONLY, 0);
//     char file_buffer[BUF_SIZE] = {0};
//     int read_length;
//     cout << "Sending the file - " << filename << endl;
//     while ((read_length = read(f, file_buffer, BUF_SIZE)) > 0)
//     {
//         send(socket_of_request, file_buffer, read_length, 0);
//     }
//     cout << "Done Sending the file - " << filename << endl;
//     close(f);
//     return;
// }

// // request to server print helper
// void handle_request_to_server(int socket_of_request, int port)
// {
//     char read_buffer[BUF_SIZE] = {0};
//     int size_of_message;
//     while (1)
//     {
//         memset(read_buffer, 0, BUF_SIZE);
//         size_of_message = read(socket_of_request, read_buffer, BUF_SIZE);
//         if (size_of_message <= 0)
//         {
//             cout << "[*]Finished writing from 127:0.0.1:" << port << endl;
//             break;
//         }
//         cout << "[*]Data recieved from 127.0.0.1:" << port << endl;
//         // printing message to terminal
//         cout << read_buffer << endl;
//     }
//     return;
// }

// handshake with peer
int handshake_with_peer(int PEER_SERVER_PORT)
{
    int peer_client_socket_fd;
    sockaddr_in peer_server_addr;
    int addr_len = sizeof(peer_server_addr);
    peer_client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (peer_client_socket_fd < 0)
    {
        perror("[-]Peer Client Socket failed to create\n");
        exit(EXIT_FAILURE);
    }
    peer_server_addr.sin_family = AF_INET;
    peer_server_addr.sin_port = htons(PEER_SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &peer_server_addr.sin_addr) <= 0)
    {
        perror("[-]P2P Connection failed\n");
        exit(EXIT_FAILURE);
    }
    int p2p_connection;
    if (p2p_connection = connect(peer_client_socket_fd, (sockaddr *)&peer_server_addr, sizeof(peer_server_addr)) < 0)
    {
        perror("[-]Connection to peer failed\n");
        exit(EXIT_FAILURE);
    }
    // cout << "Connection with " << PEER_SERVER_PORT << " established\n";
    return peer_client_socket_fd;
}

// plain send request command to peer and return response
string send_request_to_peer_and_return_response(int PEER_SERVER_PORT, string request)
{
    int peer_client_socket_fd;
    sockaddr_in peer_server_addr;
    int addr_len = sizeof(peer_server_addr);
    peer_client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (peer_client_socket_fd < 0)
    {
        perror("[-]Peer Client Socket failed to create\n");
        exit(EXIT_FAILURE);
    }
    peer_server_addr.sin_family = AF_INET;
    peer_server_addr.sin_port = htons(PEER_SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &peer_server_addr.sin_addr) <= 0)
    {
        perror("[-]P2P Connection failed\n");
        exit(EXIT_FAILURE);
    }
    int p2p_connection;
    if (p2p_connection = connect(peer_client_socket_fd, (sockaddr *)&peer_server_addr, sizeof(peer_server_addr)) < 0)
    {
        perror("[-]Connection to peer failed\n");
        exit(EXIT_FAILURE);
    }
    // cout << "Connection with " << PEER_SERVER_PORT << " established\n";
    // sending get_file request
    send(peer_client_socket_fd, request.c_str(), request.length(), 0);
    // cout << "[*]Request sent to peer\n";
    char response[BUF_SIZE] = {0};
    int response_len = read(peer_client_socket_fd, response, BUF_SIZE);
    if (response_len < 0)
    {
        return "END";
    }
    response[response_len] = '\0';
    return response;
}

// send request to peer for a chunk and write response in a file
void get_chunk(int PEER_SERVER_PORT, string command, int chunk_no, string file_size, int log_fd)
{
    // get_chunk download_file group file dest
    string req = command + " " + to_string(chunk_no);
    // string file_size = to_string(calc_file_size(tokenize(command)[3]));
    float count = (float)(stoll(file_size)) / BUF_SIZE;
    long long piece_count = (int)ceil(count);
    int peer_connection = handshake_with_peer(PEER_SERVER_PORT);
    send(peer_connection, req.c_str(), BUF_SIZE, 0);
    int file_fd = open((tokenize(command)[4] + "/" + tokenize(command)[3]).c_str(), O_WRONLY | O_CREAT, 0644);
    long long offset = chunk_no * BUF_SIZE;
    long long max_offset = (chunk_no + 1) * BUF_SIZE;
    char read_buffer[65536] = {0};
    int read_len;
    int count2 = 0;
    while (offset < max_offset)
    {
        read_len = read(peer_connection, read_buffer, 65536);
        if (read_len < 0)
        {
            break;
        }
        pwrite(file_fd, read_buffer, read_len, offset);
        offset = offset + read_len;
        memset(read_buffer, 0, 65536);
    }
    string log = "receiving chunk number - " + to_string(chunk_no) + " from " + to_string(PEER_SERVER_PORT) + " size - " + to_string(read_len) + "\n";
    write(log_fd, log.c_str(), log.length());
    set_chunk_vector(tokenize(command)[2] + "$$" + tokenize(command)[3], chunk_no, piece_count, false);

    close(file_fd);
    close(peer_connection);
}

// piecewise algo takes peerslist and command as input
void piece_wise(vector<string> peers_with_file, string command, string file_size)
{
    int log_fd = open(("log.txt(" + tokenize(command)[2] + ")").c_str(), O_CREAT | O_WRONLY, 0644);
    // cout << log_fd << endl;
    write(log_fd, command.c_str(), command.length());
    in_downloaded_files[tokenize(command)[1] + "$$" + tokenize(command)[2]] = true;
    vector<vector<int>> current_download_peers_list; // piece->vector of peers
    vector<string> tokens = tokenize(command);
    // string file_size = to_string(calc_file_size(tokens[2]));
    float count = (float)(stoll(file_size)) / BUF_SIZE;
    long long piece_count = (int)ceil(count);
    current_download_peers_list.clear();
    current_download_peers_list.resize(piece_count);
    // cout << "Polling all users in peers list for chunk information" << endl;
    // getting all the chunks available at each user
    for (long long i = 0; i < peers_with_file.size(); i++)
    {
        string response = send_request_to_peer_and_return_response(stoi(peers_with_file[i]), "get_chunk_info " + tokens[1] + "$$" + tokens[2]);
        // cout << "PEER - " << peers_with_file[i] << " chunk info:" << endl;
        // cout << response << endl;
        for (long long j = 0; j < current_download_peers_list.size(); j++)
        {
            // cout << "j - " << j << endl;
            if (response[j] == '1')
            {
                // cout << peers_with_file[i] << endl;
                current_download_peers_list[j].push_back(stoi(peers_with_file[i]));
            }
        }
    }
    // printing current downlaod peers list
    // cout << "current_download_peers_list" << endl;
    for (long long i = 0; i < current_download_peers_list.size(); i++)
    {
        // cout << "piece - " << i << endl;
        for (long long j = 0; j < current_download_peers_list[i].size(); j++)
        {
            // cout << current_download_peers_list[i][j] << " ";
        }
    }
    // checking whether all chunks are present in the network
    for (long long i = 0; i < current_download_peers_list.size(); i++)
    {
        if (current_download_peers_list[i].size() == 0)
        {
            cout << "All parts of the file are not available." << endl;
            return;
        }
    }
    srand((unsigned)time(0));
    vector<thread> download_threads;
    // long long pieces_received = 0;
    for (long long pieces_received = 0; pieces_received < piece_count; pieces_received++)
    {
        int random_peer = current_download_peers_list[pieces_received][rand() % current_download_peers_list[pieces_received].size()];
        usleep(10000);
        download_threads.push_back(thread(get_chunk, random_peer, "get_chunk " + command, pieces_received, file_size, log_fd));
    }
    for (auto it = download_threads.begin(); it != download_threads.end(); it++)
    {
        if (it->joinable())
            it->join();
    }
    shared_files[tokenize(command)[1] + "$$" + tokenize(command)[2]] = true;
    downloaded_files[tokenize(command)[1] + "$$" + tokenize(command)[2]] = true;
    in_downloaded_files[tokenize(command)[1] + "$$" + tokenize(command)[2]] = false;
    close(log_fd);
    return;
}

/* -------------------------------------------------------------------------- */
/*                             // ANCHOR HANDLERS                             */
/* -------------------------------------------------------------------------- */

// CREATE_USER sending <USERNAME> <PASSWORD> to tracker
void handle_create_user(int peer_client_socket_fd, sockaddr_in &address, string command)
{
    if (is_logged)
    {
        cout << "User already logged in\n";
        return;
    }
    request_to_tracker(peer_client_socket_fd, address, command);
    cout << ">> signup request sent to tracker\n";
    string res = response_from_tracker(peer_client_socket_fd);
    cout << res;
}

// LOGIN sending <USERNAME> <PASSWORD> <PORT> to tracker
void handle_login(int peer_client_socket_fd, sockaddr_in &address, string command)
{
    if (is_logged)
    {
        cout << "User already logged in\n";
        return;
    }
    request_to_tracker(peer_client_socket_fd, address, command + " " + to_string(PEER_SERVER_PORT));
    cout << ">> login request sent to tracker\n";
    string res = response_from_tracker(peer_client_socket_fd);
    cout << res;
    if (res == "Successfully Logged New User\n")
    {
        is_logged = true;
        curent_user_id = tokenize(command)[1];
    }
}

// CREATE_GROUP sending <GROUP_ID> <USERNAME> to tracker
void handle_create_group(int peer_client_socket_fd, sockaddr_in &address, string command)
{
    if (!is_logged)
    {
        cout << "Login to use this command\n";
        return;
    }
    request_to_tracker(peer_client_socket_fd, address, command + " " + curent_user_id);
    cout << ">> create group request sent to tracker\n";
    string res = response_from_tracker(peer_client_socket_fd);
    cout << res;
}

// JOIN_GROUP sending <GROUP_ID> <USER_NAME> to tracker
void handle_join_group(int peer_client_socket_fd, sockaddr_in &address, string command)
{
    if (!is_logged)
    {
        cout << "Login to use this command";
        return;
    }
    request_to_tracker(peer_client_socket_fd, address, command + " " + curent_user_id);
    cout << ">> join group request sent to tracker\n";
    string res = response_from_tracker(peer_client_socket_fd);
    cout << res;
}

// LIST_REQUESTS sending <GROUP_ID> <USER_NAME> to tracker
void handle_list_requests(int peer_client_socket_fd, sockaddr_in &address, string command)
{
    if (!is_logged)
    {
        cout << "Login to use this command\n";
        return;
    }
    request_to_tracker(peer_client_socket_fd, address, command + " " + curent_user_id);
    cout << ">> pending requests to be fetched tracker\n";
    string res = response_from_tracker(peer_client_socket_fd);
    cout << res;
}

// ACCEPT_REQUEST sending <GROUP_ID> <USER_NAME> <USER_NAME> to tracker
void handle_accept_request(int peer_client_socket_fd, sockaddr_in &address, string command)
{
    if (!is_logged)
    {
        cout << "Login to use this command\n";
        return;
    }
    request_to_tracker(peer_client_socket_fd, address, command + " " + curent_user_id);
    cout << ">> accept request sent to tracker\n";
    string res = response_from_tracker(peer_client_socket_fd);
    cout << res;
}

// LIST_GROUPS sending nothing but list_groups to tracker
void handle_list_groups(int peer_client_socket_fd, sockaddr_in &address, string command)
{
    if (!is_logged)
    {
        cout << "Login to use this command\n";
        return;
    }
    request_to_tracker(peer_client_socket_fd, address, command);
    cout << ">> list_groups request sent to tracker\n";
    string res = response_from_tracker(peer_client_socket_fd);
    cout << res;
}

// LIST_FILES sending <group_id> <user_id>
void handle_list_files(int peer_client_socket_fd, sockaddr_in &address, string command)
{
    if (!is_logged)
    {
        cout << "Login to use this command\n";
        return;
    }
    request_to_tracker(peer_client_socket_fd, address, command + " " + curent_user_id);
    cout << ">> list_files request sent to tracker\n";
    string res = response_from_tracker(peer_client_socket_fd);
    cout << res;
}

// LOGOUT sending <USERNAME> to tracker
void handle_logout(int peer_client_socket_fd, sockaddr_in &address, string command)
{
    if (!is_logged)
    {
        cout << "Login to use this command\n";
        return;
    }
    request_to_tracker(peer_client_socket_fd, address, command + " " + curent_user_id);
    cout << ">> logout request sent to tracker\n";
    string res = response_from_tracker(peer_client_socket_fd);
    cout << res;
}

// UPLOAD_FILE sending <FILE_META_DATA> to tracker
void handle_upload_file(int peer_client_socket_fd, sockaddr_in &address, string command)
{
    if (!is_logged)
    {
        cout << "Login to use this command\n";
        return;
    }
    string file_name = tokenize(command)[1];
    int file_fd = open(file_name.c_str(), O_RDONLY, 0);
    if (file_fd < 0)
    {
        cout << "File To Be Uploaded Not found\n";
        return;
    }
    string file_size = to_string(calc_file_size(file_name));
    float count = (float)(stoll(file_size)) / BUF_SIZE;
    long long piece_count = (int)ceil(count);
    // cout << "sending filename, group_id and user_id, file_size to tracker\n";
    // send the initial command to tracker **upload_file file_path group_id user_id**
    request_to_tracker(peer_client_socket_fd, address, command + " " + curent_user_id + " " + file_size);
    string res = response_from_tracker(peer_client_socket_fd);
    cout << res;
    if (res == "Group Not Present\n" || res == "Cannot Share File In The Group, Not A Member\n")
    {
        return;
    }
    // send piecewise hash to tracker
    char file_buffer[BUF_SIZE] = {0};
    int read_length;
    // cout << "sending the peicewise hash to tracker\n";
    memset(file_buffer, 0, BUF_SIZE);
    while ((read_length = read(file_fd, file_buffer, BUF_SIZE)) > 0)
    {
        file_buffer[read_length] = '\0';
        string file_buffer_string = file_buffer;
        string piece_hash = calcSHA(file_buffer_string);
        send(peer_client_socket_fd, piece_hash.c_str(), piece_hash.length(), 0);
        memset(file_buffer, 0, BUF_SIZE);
    }
    string end_marker = "END";
    send(peer_client_socket_fd, end_marker.c_str(), end_marker.length(), 0);
    cout << "Finished sending the piecewise hash\n";
    close(file_fd);
    shared_files[tokenize(command)[2] + "$$" + file_name] = true;
    set_chunk_vector(tokenize(command)[2] + "$$" + file_name, 0, piece_count, true);
    file_paths[tokenize(command)[1]] = tokenize(command)[1];
    // cout << tokenize(command)[1];
    // cout << "upload done, my chunk vrctor is - " << endl;
    // cout << vector_stringify(shared_files_chunk_info[tokenize(command)[2] + "$$" + file_name]) << endl;
}

// DOWNLOAD_FILE sending <GROUP_NAME> <FILE_NAME> <dest_name>
void handle_download_file(int peer_client_socket_fd, sockaddr_in &address, string command)
{
    if (!is_logged)
    {
        cout << "Login to use this command\n";
        return;
    }
    // get peers list for the file from the tracker
    request_to_tracker(peer_client_socket_fd, address, command + " " + curent_user_id);
    // map the filenam to destination
    file_paths[tokenize(command)[2]] = tokenize(command)[3] + "/" + tokenize(command)[2];
    // cout << "peers_list request sent to tracker\n";
    string res = response_from_tracker(peer_client_socket_fd);
    vector<string> peers_with_file = tokenize(res);
    if (peers_with_file.size() <= 1 || res == "File not present\n" || res == "Group Not Present\n" || res == "Cannot Fetch Peers List, Not A Member\n")
    {
        cout << "No file present (or) No Group Present (or) Not a member of the group" << endl;
        return;
    }
    string file_size = peers_with_file.back();
    peers_with_file.pop_back();
    thread master(piece_wise, peers_with_file, command, file_size);
    master.detach();
}

// SHOW_DOWNLOADS
void handle_show_downloads()
{
    if (!is_logged)
    {
        cout << "Login to use this command\n";
        return;
    }
    for (auto i : in_downloaded_files)
    {
        if (i.second)
        {

            cout << "[D] [" << split_string(i.first, "$$")[0] << "] " << split_string(i.first, "$$")[1] << endl;
        }
    }
    for (auto i : downloaded_files)
    {
        if (i.second)
        {
            cout << "[C] [" << split_string(i.first, "$$")[0] << "] " << split_string(i.first, "$$")[1] << endl;
        }
    }
}

// STOP_SHARE sends a request to tracker and also updates the global variables
void handle_stop_share(int peer_client_socket_fd, sockaddr_in &address, string command)
{
    // stop_share group file
    vector<string> tokens = tokenize(command);
    if (!is_logged)
    {
        cout << "Login to use this command\n";
        return;
    }
    shared_files[tokens[1] + "$$" + tokens[2]] = false;
    shared_files_chunk_info.erase(tokens[1] + "$$" + tokens[2]);
    request_to_tracker(peer_client_socket_fd, address, command + " " + curent_user_id);
    cout << "Stopped sharing the file - " << tokenize(command)[2] << endl;
}

/* -------------------------------------------------------------------------- */
/*                           // ANCHOR PEER TO PEER                           */
/* -------------------------------------------------------------------------- */

// sends a request to fellow peer(specified by SERVER PORT) and processes the response
void send_request_to_peer(int PEER_SERVER_PORT, string request)
{
    int peer_client_socket_fd;
    sockaddr_in peer_server_addr;
    int addr_len = sizeof(peer_server_addr);
    peer_client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (peer_client_socket_fd < 0)
    {
        perror("[-]Peer Client Socket failed to create\n");
        exit(EXIT_FAILURE);
    }
    peer_server_addr.sin_family = AF_INET;
    peer_server_addr.sin_port = htons(PEER_SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &peer_server_addr.sin_addr) <= 0)
    {
        perror("[-]P2P Connection failed\n");
        exit(EXIT_FAILURE);
    }
    int p2p_connection;
    if (p2p_connection = connect(peer_client_socket_fd, (sockaddr *)&peer_server_addr, sizeof(peer_server_addr)) < 0)
    {
        perror("[-]Connection to peer failed\n");
        exit(EXIT_FAILURE);
    }
    // cout << "Connection with " << PEER_SERVER_PORT << " established\n";
    // sending get_file request
    send(peer_client_socket_fd, request.c_str(), request.length(), 0);
    // cout << "[*]Request sent to peer\n";
    // saving the received file
    int d = open("download.txt", O_WRONLY | O_CREAT, 0644);
    char file_buffer[BUF_SIZE] = {0};
    int read_length;
    while ((read_length = read(peer_client_socket_fd, file_buffer, BUF_SIZE)) > 0)
    {
        write(d, file_buffer, read_length);
    }
    // cout << "Done Downloading the file - " << tokenize(request)[1] << endl;
    close(d);
    close(peer_client_socket_fd);
}

// handle request from fellow peer and return the response
void handle_request_from_peer(int socket_of_request, int port_of_request)
{
    char req[BUF_SIZE] = {0};
    int req_len = recv(socket_of_request, req, BUF_SIZE, 0);
    req[req_len] = '\0';
    if (req_len < 0)
    {
        close(socket_of_request);
        return;
    }
    vector<string> tokens = tokenize(req);
    // cout << "Request from - " << port_of_request << " recieved" << endl;
    // cout << req << endl;
    if (tokenize(req)[0] == "get_chunk_info")
    {
        send(socket_of_request, vector_stringify(shared_files_chunk_info[tokenize(req)[1]]).c_str(), BUF_SIZE, 0);
    }
    if (tokenize(req)[0] == "get_chunk")
    {
        // req -> get_chunk download_file group_id file_id dest_id chunk_no
        // pread, read_chunk func
        // cout << "opening - " << file_paths[tokens[3]] << " to send" << endl;
        int file_fd = open(file_paths[tokens[3]].c_str(), O_RDONLY, 0);
        long long offset = stoll(tokens[5]) * BUF_SIZE;
        long long max_offset = (stoll(tokens[5]) + 1) * BUF_SIZE;
        char read_buffer[65536] = {0};
        int read_len;
        while (offset < max_offset && (read_len = pread(file_fd, read_buffer, 65536, offset)) > 0)
        {
            // cout << "sending chunk number - " << tokens[5] << " to " << port_of_request << " size - " << read_len << endl;
            send(socket_of_request, read_buffer, read_len, 0);
            usleep(10000);
            offset = offset + read_len;
            memset(read_buffer, 0, 65536);
        }
        // send(socket_of_request, "END", 4, 0);
        close(file_fd);
    }
    close(socket_of_request);
}

// peer server thread handler
void run_server()
{
    int peer_server_socket_fd, peer_server_socket_connection;
    sockaddr_in peer_server_addr;
    int addr_len = sizeof(peer_server_addr);
    peer_server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (peer_server_socket_fd < 0)
    {
        perror("[-]Peer Server Socket failed to create\n");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    if (setsockopt(peer_server_socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("[-]Peer Server Setsocketopt failed\n]");
        exit(EXIT_FAILURE);
    }
    peer_server_addr.sin_family = AF_INET;
    peer_server_addr.sin_addr.s_addr = INADDR_ANY;
    peer_server_addr.sin_port = htons(PEER_SERVER_PORT);
    // if (inet_pton(AF_INET, &PEER_ADDR[0], &peer_server_addr.sin_addr) <= 0)
    // {
    //     perror("[-]Peer Address not supported\n");
    //     exit(EXIT_FAILURE);
    // }
    peer_server_socket_connection = bind(peer_server_socket_fd, (const sockaddr *)&peer_server_addr, (socklen_t)addr_len);
    if (peer_server_socket_connection < 0)
    {
        perror("[-]Peer Server Socket failed to bind\n");
        exit(EXIT_FAILURE);
    }
    if (listen(peer_server_socket_fd, INT_MAX) < 0)
    {
        perror("[-]Peer Server Socket failed to listen\n");
        exit(EXIT_FAILURE);
    }
    vector<thread> server_threads;
    while (1)
    {
        // cout << "[*]Server waiting for a request...\n";
        int socket_of_request;
        socket_of_request = accept(peer_server_socket_fd, (sockaddr *)&peer_server_addr, (socklen_t *)&addr_len);
        if (socket_of_request < 0)
        {
            perror("[-]Error while accepting request\n");
            exit(EXIT_FAILURE);
        }
        int port = ntohs(peer_server_addr.sin_port);
        server_threads.push_back(thread(handle_request_from_peer, socket_of_request, port));
    }
    for (auto it = server_threads.begin(); it != server_threads.end(); it++)
    {
        if (it->joinable())
            it->join();
    }
    close(peer_server_socket_fd);
    return;
}

/* -------------------------------------------------------------------------- */
/*                               // ANCHOR MAIN                               */
/* -------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    clrscr();
    string config = argv[1];
    vector<string> config_tokens = split_string(config, ":");
    PEER_ADDR = config_tokens[0];
    PEER_SERVER_PORT = stoi(config_tokens[1]);
    char curDir[128];
    getcwd(curDir, 128);
    string path = string(curDir);
    path += "/" + (string)argv[2];
    int track_fd = open(path.c_str(), O_RDONLY, 0);
    char track_buffer[1024] = {0};
    int len = read(track_fd, track_buffer, 1024);
    track_buffer[len] = '\0';
    TRACKER_PORT = stoi(track_buffer);
    // TRACKER_PORT = stoi(argv[2]);
    int peer_client_socket_fd;
    sockaddr_in address;
    int addr_len = sizeof(address);
    peer_client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (peer_client_socket_fd < 0)
    {
        perror("[-]Peer Client Socket failed to create\n");
        exit(EXIT_FAILURE);
    }
    thread server_thread(run_server);
    handshake_with_tracker(peer_client_socket_fd, address);
    while (true)
    {
        cout << "\n\033[1;30m\033[1;42m"
             << "Enter Command:"
             << "\033[0m  ";
        string command;
        getline(cin, command);
        if (command.length() < 1)
            continue;
        vector<string> tokens = tokenize(command);
        if (tokens[0] == "create_user")
        {
            if (tokens.size() != 3)
            {
                cout << "Invalid number of arguments, requires <user_id> and <password>\n";
                continue;
            }

            handle_create_user(peer_client_socket_fd, address, command);
        }
        if (tokens[0] == "login")
        {
            if (tokens.size() != 3)
            {
                cout << "Invalid number of arguments, requires <user_id> and <password>\n";
                continue;
            }

            handle_login(peer_client_socket_fd, address, command);
        }
        if (tokens[0] == "list_groups")
        {
            if (tokens.size() != 1)
            {
                cout << "Invalid, No arguments required\n";
                continue;
            }

            handle_list_groups(peer_client_socket_fd, address, command);
        }
        if (tokens[0] == "list_files")
        {
            if (tokens.size() != 2)
            {
                cout << "Invalid number of arguments, requires <group_id>\n";
                continue;
            }

            handle_list_files(peer_client_socket_fd, address, command);
        }
        if (tokens[0] == "create_group")
        {
            if (tokens.size() != 2)
            {
                cout << "Invalid number of arguments, requires <group_id>\n";
                continue;
            }

            handle_create_group(peer_client_socket_fd, address, command);
        }
        if (tokens[0] == "join_group")
        {
            if (tokens.size() != 2)
            {
                cout << "Invalid number of arguments, requires <group_id>\n";
                continue;
            }

            handle_join_group(peer_client_socket_fd, address, command);
        }
        if (tokens[0] == "list_requests")
        {
            if (tokens.size() != 2)
            {
                cout << "Invalid number of arguments, requires <group_id>\n";
                continue;
            }

            handle_list_requests(peer_client_socket_fd, address, command);
        }
        if (tokens[0] == "accept_request")
        {
            if (tokens.size() != 3)
            {
                cout << "Invalid number of arguments, requires <group_id> and <user_id>\n";
                continue;
            }

            handle_accept_request(peer_client_socket_fd, address, command);
        }
        if (tokens[0] == "upload_file")
        {
            if (tokens.size() != 3)
            {
                cout << "Invalid number of arguments, requires <file_id> and <group_id>\n";
                continue;
            }

            handle_upload_file(peer_client_socket_fd, address, command);
        }
        if (tokens[0] == "logout")
        {
            if (tokens.size() != 1)
            {
                cout << "Invalid, No arguments required\n";
                continue;
            }
            handle_logout(peer_client_socket_fd, address, command);
        }
        if (tokens[0] == "download_file")
        {
            if (tokens.size() != 4)
            {
                cout << "Invalid number of arguments, requires <group_id> <file_id> <dest_path>\n";
                continue;
            }
            handle_download_file(peer_client_socket_fd, address, command);
            // thread master_download(handle_download_file, ref(peer_client_socket_fd), ref(address), ref(command));
            // master_download.detach();
        }
        if (tokens[0] == "show_downloads")
        {
            if (tokens.size() != 1)
            {
                cout << "Invalid, No arguments required\n";
                continue;
            }
            handle_show_downloads();
        }
        if (tokens[0] == "stop_share")
        {
            if (tokens.size() != 3)
            {
                cout << "Invalid number of arguments, requires <group_id> <file_id>\n";
                continue;
            }
            handle_stop_share(peer_client_socket_fd, address, command);
        }
    }
    server_thread.join();
    return 0;
}