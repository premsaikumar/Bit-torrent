#include <iostream>
#include <bits/stdc++.h>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <fcntl.h>
#define BUF_SIZE 524288
using namespace std;

/* -------------------------------------------------------------------------- */
/*                               // ANCHOR CLASS                              */
/* -------------------------------------------------------------------------- */
class User
{
public:
    string user_id;
    string password;
    int port;
    bool is_alive;
    User()
    {
    }
    User(string name, string pass)
    {
        user_id = name;
        password = pass;
        is_alive = false;
    }
};

class Group
{
public:
    string group_id;                 // group_id
    string owner;                    // contains group owner's user_id
    vector<string> members;          // contains members user_id
    vector<string> files_shared;     // contains files names
    vector<string> pending_requests; // contains user_id's of pending
    Group()
    {
    }
    Group(string name, string own)
    {
        group_id = name;
        owner = own;
        members.push_back(own);
    }
};

class File
{
public:
    string file_id;
    string file_name;
    string group_name;
    long long int file_size;
    long long int piece_count;
    vector<string> seeders;
    // map<long long, vector<string>> leechers;
    File() {}
    File(string fname, string gname, long long size, long long pcount, string seed)
    {
        file_id = gname + "$$" + fname;
        file_name = fname;
        group_name = gname;
        file_size = size;
        piece_count = pcount;
        seeders.emplace_back(seed);
        //     for (long long i = 0; i < pcount; i++)
        //     {
        //         leechers.insert({i, {seed}});
        //     }
    }
};

/* -------------------------------------------------------------------------- */
/*                              // ANCHOR GLOBALS                             */
/* -------------------------------------------------------------------------- */
int TRACKER_PORT = 3000;
unordered_map<string, User> users_map;   // key is user_id
unordered_map<string, Group> groups_map; // key is group_id
unordered_map<string, File> files_map;   // key is file_id which is gname$$fname

/* -------------------------------------------------------------------------- */
/*                              // ANCHOR HELPERS                             */
/* -------------------------------------------------------------------------- */

// get filename from a filepath helper
string file_name_from_path(string file_path)
{
    return file_path.substr(file_path.find_last_of("/") + 1);
}

// prints the snapshot of a group when group_id is supplied
string print_group_map(string key)
{
    Group g = groups_map[key];
    string group = "Group: " + key + "\n";
    ;
    string owner = "OWNER: " + g.owner + "\n";
    string members = "MEMBERS: ";
    for (auto m : g.members)
    {
        members += m + " ";
    }
    members += "\n";
    string files = "FILES: \n";
    for (int i = 0; i < g.files_shared.size(); i++)
    {
        cout << g.files_shared[i] << endl;
        File f = files_map[g.files_shared[i]];
        cout << files_map[g.files_shared[i]].file_id << endl;
        files += "Name: " + f.file_id + "\nPiece Count: " + to_string(f.piece_count);
    }
    return group + owner + members + files + "\n";
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

// add a user as seeder to a file
void add_seeder_to_file(string gname, string fname, string user_id)
{
    cout << "adding " << user_id << " as a seeder to file " << gname << "$$" << fname << endl;
    files_map[gname + "$$" + fname].seeders.emplace_back(user_id);
}

string get_peers_list(string filename)
{
    string res;
    if (files_map.find(filename) == files_map.end())
    {
        cout << "Requesterd file not present" << endl;
        return "";
    }
    File f = files_map[filename];
    for (auto i : f.seeders)
    {
        if (users_map[i].is_alive)
            res += to_string(users_map[i].port) + " ";
    }
    res.pop_back();
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

// prints a single peer message and prints it to terminal helper
// void handle_request_to_server(int socket_of_request, int port)
// {
//     char read_buffer[BUF_SIZE] = {0};
//     int size_of_message;
//     memset(read_buffer, 0, BUF_SIZE);
//     size_of_message = read(socket_of_request, read_buffer, BUF_SIZE);
//     cout << "[*]Data recieved from 127.0.0.1:" << port << endl;
//     cout << read_buffer << endl;
//     return;
// }

// send the command response to peer helper
void send_response_to_peer(int socket_of_request, string response)
{
    response += "\n";
    send(socket_of_request, response.c_str(), response.length(), 0);
}

/* -------------------------------------------------------------------------- */
/*                             // ANCHOR HANDLERS                             */
/* -------------------------------------------------------------------------- */

// CREATE_USER contains **create_user username password**
void handle_create_user(int socket_of_request, vector<string> tokens)
{
    if (users_map.find(tokens[1]) != users_map.end())
    {
        cout << "already present\n";
        send_response_to_peer(socket_of_request, "User Already Present");
    }
    else
    {
        User u(tokens[1], tokens[2]);
        users_map.insert({tokens[1], u});
        cout << "created new user\n";
        send_response_to_peer(socket_of_request, "Successfully Created New User");
    }
}

// LOGIN contains **create_user username password port**
void handle_login(int socket_of_request, vector<string> tokens)
{
    if (users_map.find(tokens[1]) == users_map.end())
    {
        cout << "user not present\n";
        send_response_to_peer(socket_of_request, "User Not Present");
    }
    else
    {
        User u = users_map[tokens[1]];
        if (u.password != tokens[2])
        {
            cout << "password incorrect\n";
            send_response_to_peer(socket_of_request, "Password Incorrect");
            return;
        }
        if (u.is_alive == true)
        {
            cout << "user trying to login from multiple terminals\n";
            send_response_to_peer(socket_of_request, "Illegal Login");
            return;
        }
        u.is_alive = true;
        u.port = stoi(tokens[3]);
        users_map[tokens[1]] = u;
        cout << "logged a user with peer_server_port: " << tokens[3] << endl;
        send_response_to_peer(socket_of_request, "Successfully Logged New User");
    }
}

// CREATE_GROUP contains **create_group group_id username**
void handle_create_group(int socket_of_request, vector<string> tokens)
{
    if (groups_map.find(tokens[1]) != groups_map.end())
    {
        cout << "group already present\n";
        send_response_to_peer(socket_of_request, "Group Already Present");
    }
    else
    {
        Group g(tokens[1], tokens[2]);
        groups_map.insert({tokens[1], g});
        cout << "created group user\n";
        send_response_to_peer(socket_of_request, "Successfully Created New Group");
    }
}

// JOIN_GROUP contains **join_group group_id user_id**
void handle_join_group(int socket_of_request, vector<string> tokens)
{
    if (groups_map.find(tokens[1]) == groups_map.end())
    {
        cout << "group not present\n";
        send_response_to_peer(socket_of_request, "Group Not Present");
        return;
    }
    if (find(groups_map[tokens[1]].members.begin(), groups_map[tokens[1]].members.end(), tokens[2]) != groups_map[tokens[1]].members.end())
    {
        cout << "member already present in group\n";
        send_response_to_peer(socket_of_request, "Member Already Present In Requested Group");
        return;
    }
    groups_map[tokens[1]].pending_requests.emplace_back(tokens[2]);
    send_response_to_peer(socket_of_request, "Request To Join Group Stored");
}

// LIST_REQUESTS contains **list_requests group_id user_id**
void handle_list_requests(int socket_of_request, vector<string> tokens)
{
    if (groups_map.find(tokens[1]) == groups_map.end())
    {
        cout << "group not present\n";
        send_response_to_peer(socket_of_request, "Group Not Present");
        return;
    }
    Group g = groups_map[tokens[1]];
    if (g.owner != tokens[2])
    {
        cout << "request not made by owner\n";
        send_response_to_peer(socket_of_request, "Illegal Request, Must Be The Owner Of Group");
        return;
    }
    cout << "sending pending requests in a group\n";
    string pending = "";
    for (auto i : g.pending_requests)
    {
        cout << i << endl;
        pending += i + "\n";
    }
    send_response_to_peer(socket_of_request, pending);
}

// ACCEPT_REQUEST contains **accept_request group_id user_id owner_id**
void handle_accept_request(int socket_of_request, vector<string> tokens)
{
    if (groups_map.find(tokens[1]) == groups_map.end())
    {
        cout << "group not present\n";
        send_response_to_peer(socket_of_request, "Group Not Present");
        return;
    }
    Group g = groups_map[tokens[1]];
    if (g.owner != tokens[3])
    {
        cout << "request not made by owner\n";
        send_response_to_peer(socket_of_request, "Illegal Request, Must Be The Owner Of Group");
        return;
    }
    groups_map[tokens[1]].members.emplace_back(tokens[2]);
    auto marker = find(groups_map[tokens[1]].pending_requests.begin(), groups_map[tokens[1]].pending_requests.end(), tokens[2]);
    if (marker == groups_map[tokens[1]].pending_requests.end())
    {
        cout << "no pending request for the user: " << tokens[2] << endl;
        send_response_to_peer(socket_of_request, "No Request Made From User: " + tokens[2]);
        return;
    }
    groups_map[tokens[1]].pending_requests.erase(marker);
    send_response_to_peer(socket_of_request, "Request Accepted, " + tokens[2] + " is now a member");
}

// LIST_GROUPS contains **list_groups**
void handle_list_groups(int socket_of_request, vector<string> tokens)
{
    if (groups_map.size() == 0)
    {
        send_response_to_peer(socket_of_request, "No Groups Present");
    }
    else
    {
        string groups = "***********************\n";
        for (auto g : groups_map)
        {
            groups += print_group_map(g.first);
        }
        groups += "************************\n";
        send_response_to_peer(socket_of_request, groups);
    }
}

// LIST_FILES contains **list_files group_id user_id**
void handle_list_files(int socket_of_request, vector<string> tokens)
{
    if (groups_map.find(tokens[1]) == groups_map.end())
    {
        cout << "group not present\n";
        send_response_to_peer(socket_of_request, "Group Not Present");
        return;
    }
    if (find(groups_map[tokens[1]].members.begin(), groups_map[tokens[1]].members.end(), tokens[2]) == groups_map[tokens[1]].members.end())
    {
        cout << "user not present int the group, cannot be shared\n";
        send_response_to_peer(socket_of_request, "Cannot Fetch Files List, Not A Member");
        return;
    }
    else
    {
        string files = "";
        for (auto f : groups_map[tokens[1]].files_shared)
        {
            files += f.substr(f.find("$") + 2) + "\n";
        }
        send_response_to_peer(socket_of_request, files);
    }
}

// LOGOUT contains **logout user_id**
void handle_logout(int socket_of_request, vector<string> tokens)
{
    users_map[tokens[1]].is_alive = false;
    send_response_to_peer(socket_of_request, "Logout Successfull");
}

// UPLOAD_FILE contains **upload_file file_path group_id user_id file_size**
void handle_upload_file(int socket_of_request, vector<string> tokens)
{
    if (groups_map.find(tokens[2]) == groups_map.end())
    {
        cout << "group not present\n";
        send_response_to_peer(socket_of_request, "Group Not Present");
        return;
    }
    if (find(groups_map[tokens[2]].members.begin(), groups_map[tokens[2]].members.end(), tokens[3]) == groups_map[tokens[2]].members.end())
    {
        cout << "user not present int the group, cannot be shared\n";
        send_response_to_peer(socket_of_request, "Cannot Share File In The Group, Not A Member");
        return;
    }
    string file_name = tokens[2] + "$$" + file_name_from_path(tokens[1]) + ".torrent";
    if (open(file_name.c_str(), O_RDONLY) > 0)
    {
        cout << "torrent file already present\n";
        add_seeder_to_file(tokens[2], file_name_from_path(tokens[1]), tokens[3]);
        send_response_to_peer(socket_of_request, "File Already Shared With The Group");
        return;
    }
    cout << file_name << endl;
    int torrent_file = open(file_name.c_str(), O_WRONLY | O_CREAT, 0644);
    if (torrent_file < 0)
    {
        cout << "unable to create torrent file\n";
        send_response_to_peer(socket_of_request, "Unable To Create The Torrent File");
        return;
    }
    send_response_to_peer(socket_of_request, "File Upload Started");

    string file_name_line = "file_name=" + file_name_from_path(tokens[1]) + "\n";
    string group_name_line = "group_id=" + tokens[2] + "\n";
    string file_size_line = "file_size=" + tokens[4] + "\n";
    float count = (float)(stoll(tokens[4])) / BUF_SIZE;
    string piece_count_line = "piece_count=" + to_string((int)ceil(count)) + "\n";
    string new_line = "\n";
    write(torrent_file, file_name_line.c_str(), file_name_line.length());
    write(torrent_file, group_name_line.c_str(), group_name_line.length());
    write(torrent_file, file_size_line.c_str(), file_size_line.length());
    write(torrent_file, piece_count_line.c_str(), piece_count_line.length());
    int size_of_message;
    char read_buffer[BUF_SIZE] = {0};
    while ((size_of_message = recv(socket_of_request, read_buffer, 40, 0)) > 0)
    {
        string read_buffer_string = read_buffer;
        if (read_buffer_string == "END")
        {
            break;
        }
        write(torrent_file, read_buffer, size_of_message);
        write(torrent_file, new_line.c_str(), new_line.length());
        memset(read_buffer, 0, BUF_SIZE);
    }
    cout << "done writing torrent\n";
    close(torrent_file);
    File f(file_name_from_path(tokens[1]), tokens[2], stoll(tokens[4]), (long long)ceil(count), tokens[3]);
    groups_map[tokens[2]].files_shared.emplace_back(f.file_id);
    files_map.insert({f.file_id, f});
}

// DOWNLOAD_FILE contains **downlload_file group_id file_id dest_id user_id**
void handle_download_file(int socket_of_request, vector<string> tokens)
{
    if (files_map.find(tokens[1] + "$$" + tokens[2]) == files_map.end())
    {
        cout << "file not present\n";
        send_response_to_peer(socket_of_request, "File not present");
        return;
    }
    if (groups_map.find(tokens[1]) == groups_map.end())
    {
        cout << "group not present\n";
        send_response_to_peer(socket_of_request, "Group Not Present");
        return;
    }
    if (find(groups_map[tokens[1]].members.begin(), groups_map[tokens[1]].members.end(), tokens[4]) == groups_map[tokens[1]].members.end())
    {
        cout << "user not present int the group, cannot fetch peers list\n";
        send_response_to_peer(socket_of_request, "Cannot Fetch Peers List, Not A Member");
        return;
    }
    else
    {
        send_response_to_peer(socket_of_request, get_peers_list(tokens[1] + "$$" + tokens[2]) + " " + to_string(files_map[tokens[1] + "$$" + tokens[2]].file_size));
        add_seeder_to_file(tokens[1], tokens[2], tokens[4]);
    }
}

// STOP_SHARE contians **stop_share group file user_id**
void handle_stop_share(int socket_of_request, vector<string> tokens)
{
    auto mark = find(files_map[tokens[1] + "$$" + tokens[2]].seeders.begin(), files_map[tokens[1] + "$$" + tokens[2]].seeders.end(), tokens[3]);
    if (mark == files_map[tokens[1] + "$$" + tokens[2]].seeders.end())
    {
        cout << "user not shared the file earlier\n";
        return;
    }
    cout << "user removed from file-share list\n";
    files_map[tokens[1] + "$$" + tokens[2]].seeders.erase(mark);
}

// after a peer request is received, this function will choose appropriate handler
void process_request(int socket_of_request, int port, string request)
{
    cout << "[*]Data recieved from " << port << endl;

    vector<string> tokens = tokenize(request);
    if (tokens[0] == "create_user")
    {
        handle_create_user(socket_of_request, tokens);
    }
    if (tokens[0] == "login")
    {
        handle_login(socket_of_request, tokens);
    }
    if (tokens[0] == "create_group")
    {
        handle_create_group(socket_of_request, tokens);
    }
    if (tokens[0] == "join_group")
    {
        handle_join_group(socket_of_request, tokens);
    }
    if (tokens[0] == "list_requests")
    {
        handle_list_requests(socket_of_request, tokens);
    }
    if (tokens[0] == "accept_request")
    {
        handle_accept_request(socket_of_request, tokens);
    }
    if (tokens[0] == "list_groups")
    {
        handle_list_groups(socket_of_request, tokens);
    }
    if (tokens[0] == "list_files")
    {
        handle_list_files(socket_of_request, tokens);
    }
    if (tokens[0] == "upload_file")
    {
        handle_upload_file(socket_of_request, tokens);
    }
    if (tokens[0] == "logout")
    {
        handle_logout(socket_of_request, tokens);
    }
    if (tokens[0] == "download_file")
    {
        handle_download_file(socket_of_request, tokens);
    }
    if (tokens[0] == "stop_share")
    {
        handle_stop_share(socket_of_request, tokens);
    }
}

// waits for a peer message, prints it and starts waiting again helper
void receive_request_from_peer(int socket_of_request, int port)
{
    char read_buffer[BUF_SIZE] = {0};
    int size_of_message;
    while (true)
    {
        memset(read_buffer, 0, BUF_SIZE);
        size_of_message = read(socket_of_request, read_buffer, BUF_SIZE);
        if (size_of_message <= 0)
        {
            // cout << "[*]Finished writing from 127:0.0.1:" << port << endl;
            break;
        }
        process_request(socket_of_request, port, read_buffer);
    }
    return;
}

// server thread handler
void run_server()
{
    int tracker_server_socket_fd, tracker_server_socket_connection;
    sockaddr_in tracker_server_addr;
    int addr_len = sizeof(tracker_server_addr);
    tracker_server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tracker_server_socket_fd < 0)
    {
        perror("[-]Tracker Server Socket failed to create\n");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    if (setsockopt(tracker_server_socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("[-]Tracker Server Setsocketopt failed\n]");
        exit(EXIT_FAILURE);
    }
    tracker_server_addr.sin_family = AF_INET;
    tracker_server_addr.sin_addr.s_addr = INADDR_ANY;
    tracker_server_addr.sin_port = htons(TRACKER_PORT);
    tracker_server_socket_connection = bind(tracker_server_socket_fd, (const sockaddr *)&tracker_server_addr, (socklen_t)addr_len);
    if (tracker_server_socket_connection < 0)
    {
        perror("[-]Tracker Server Socket failed to bind\n");
        exit(EXIT_FAILURE);
    }
    if (listen(tracker_server_socket_fd, 5) < 0)
    {
        perror("[-]Tracker Server Socket failed to listen\n");
        exit(EXIT_FAILURE);
    }
    vector<thread> server_threads;
    while (1)
    {
        // cout << "[*]Server waiting for a request...\n";
        int socket_of_request;
        socket_of_request = accept(tracker_server_socket_fd, (sockaddr *)&tracker_server_addr, (socklen_t *)&addr_len);
        if (socket_of_request < 0)
        {
            perror("[-]Error while accepting request\n");
            exit(EXIT_FAILURE);
        }
        int port = ntohs(tracker_server_addr.sin_port);
        server_threads.push_back(thread(receive_request_from_peer, socket_of_request, port));
    }
    for (auto it = server_threads.begin(); it != server_threads.end(); it++)
    {
        if (it->joinable())
            it->join();
    }
    return;
}

int main(int argc, char **argv)
{
    // test users and groups
    

    char curDir[128];
    getcwd(curDir, 128);
    string path = string(curDir);
    path += "/" + (string)argv[1];
    int track_fd = open(path.c_str(), O_RDONLY, 0);
    char track_buffer[1024] = {0};
    int len = read(track_fd, track_buffer, 1024);
    track_buffer[len] = '\0';
    TRACKER_PORT = stoi(track_buffer);

    int tracker_client_socket_fd;
    sockaddr_in tracker_client_socket_addr;
    int addr_len = sizeof(tracker_client_socket_addr);
    tracker_client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tracker_client_socket_fd < 0)
    {
        perror("[-]tracker Client Socket failed to create\n");
        exit(EXIT_FAILURE);
    }

    thread server_thread(run_server);
    while (1)
    {
        string c;
        getline(cin, c);
        if (c == "quit")
        {
            exit(EXIT_SUCCESS);
        }
    }
    server_thread.join();
    return 0;
}
