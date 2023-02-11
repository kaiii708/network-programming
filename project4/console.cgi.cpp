// #include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <vector>

#define MAX_CLIENT_NUM 5

using namespace std;
using boost::asio::ip::tcp;

boost::asio::io_service io_service;

struct server_info
{
    string hostname;
    string port;
    string file;
    ifstream fin;
    string segment;
    bool isOccupied = false;
};

string proxy_hostname;
string proxy_port;
void split_query_string(vector<string> &sigmentTable)
{
    // char *query_string = getenv("QUERY_STRING");
    // // memset(query_string, '\0', sizeof(query_string));
    // // strcpy(query_string, getenv("QUERY_STRING"));
    // const char *d = "&";
    // char *pch = strtok(query_string, d);
    // while (pch != NULL)
    // {
    //     sigmentTable[(int)sigmentTable.size()] = pch;
    //     pch = strtok(NULL, d);
    // }

    string query_string = string(getenv("QUERY_STRING"));
    boost::split(sigmentTable, query_string, boost::is_any_of("&"));
}
void parse_query_string(vector<string> sigmentTable, server_info s[])
{
    for (int i = 0; i < (int)sigmentTable.size(); i++)
    {
        if (sigmentTable[i].back() == '=')
        {
            continue;
        }
        // int serverIndex = stoi(sigmentTable[i].at(1));
        int serverIndex = (int)(sigmentTable.at(i)).at(1) - 48;
        int index = sigmentTable[i].find("=");
        switch (sigmentTable[i][0])
        {
        case 'h':

            s[serverIndex].hostname = sigmentTable[i].substr(index + 1);
            break;
        case 'p':
            s[serverIndex].port = sigmentTable[i].substr(index + 1);
            break;
        case 'f':
            s[serverIndex].file = "./test_case/" + sigmentTable[i].substr(index + 1);
            break;

        case 's':
            switch (sigmentTable[i][1])
            {
            case 'h':
                proxy_hostname = sigmentTable[i].substr(index + 1);
                break;
            case 'p':
                proxy_port = sigmentTable[i].substr(index + 1);
                break;
            }
        }
    }
    for (int i = 0; i < MAX_CLIENT_NUM; i++)
    {
        if (s[i].hostname != "" && s[i].port != "" && s[i].file != "")
        {
            s[i].isOccupied = true;
            s[i].fin.open(s[i].file, ifstream::in);
            s[i].segment = "s" + to_string(i);
        }
    }
}

void cout_table_head(server_info s[])
{
    cout << "<thead>" << endl;
    cout << "<tr>" << endl;
    for (int i = 0; i < MAX_CLIENT_NUM; i++)
    {
        // if (true)
        if (s[i].isOccupied)
        {
            cout << "<th scope=\"col\">" << s[i].hostname << ":" << s[i].port << "</th>" << endl;
        }
    }
    cout << "</tr>" << endl;
    cout << "</thead>" << endl;
}

void cout_table_body(server_info s[])
{
    cout << "<tbody>" << endl;
    cout << "<tr>" << endl;
    for (int i = 0; i < MAX_CLIENT_NUM; ++i)
    {
        // if (true)
        if (s[i].isOccupied)
        {
            cout << "<td><pre id=\"" << s[i].segment << "\" class=\"mb-0\"></pre></td>" << endl;
        }
    }
    cout << "</tr>" << endl;
    cout << "</tbody>" << endl;
}

void cout_html(server_info s[])
{
    cout << "<!DOCTYPE html>" << endl;
    cout << "<html lang=\"en\">" << endl;
    cout << "<head>" << endl;
    cout << "<meta charset=\"UTF-8\" />" << endl;
    cout << "<title>NP Project 3 Sample Console</title>" << endl;
    cout << "<link" << endl;
    cout << "rel=\"stylesheet\"" << endl;
    cout << "href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"" << endl;
    cout << "integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"" << endl;
    cout << "crossorigin=\"anonymous\"" << endl;
    cout << "/>" << endl;
    cout << "<link" << endl;
    cout << "href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"" << endl;
    cout << "rel=\"stylesheet\"" << endl;
    cout << "/>" << endl;
    cout << "<link" << endl;
    cout << "rel=\"icon\"" << endl;
    cout << "type=\"image/png\"" << endl;
    cout << "href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"" << endl;
    cout << "/>" << endl;
    cout << "<style>" << endl;
    cout << "* {" << endl;
    cout << "font-family: 'Source Code Pro', monospace;" << endl;
    cout << "font-size: 1rem !important;" << endl;
    cout << "}" << endl;
    cout << "body {" << endl;
    cout << "background-color: #212529;" << endl;
    cout << "}" << endl;
    cout << "pre {" << endl;
    cout << "color: #cccccc;" << endl;
    cout << "}" << endl;
    cout << "b {" << endl;
    cout << "color: #01b468;" << endl;
    cout << "}" << endl;
    cout << "</style>" << endl;
    cout << "</head>" << endl;
    cout << "<body>" << endl;
    cout << "<table class=\"table table-dark table-bordered\">" << endl;

    cout_table_head(s);
    cout_table_body(s);

    cout << "</table>" << endl;
    cout << "</body>" << endl;
    cout << "</html>" << endl;
}

class client
{
public:
    client()
        : socket_(io_service),
          resolver_(io_service)
    {
        memset(data_, '\0', max_length);
    }
    void start(server_info &server)
    {
        do_resolve(server);
    }

private:
    void read_sock4_reply(server_info &server)
    {
        socket_.async_read_some(
            boost::asio::buffer(data_, 8),
            [this, &server](boost::system::error_code ec, size_t length)
            {
                if (!ec)
                {
                    if (data_[1] == 90)
                    {
                        do_read(server);
                    }
                }
            });
    }
    void write_sock4_request(server_info &server)
    {
        unsigned int ip = *(unsigned int *)&server.hostname;
        unsigned int port = stoi(server.port);
        // cout << "wrong_port_number:" << port << endl;
        // cout << "wrong_port_number to string:" << to_string(port) << endl;
        char request_packet[max_length];
        memset(request_packet, '\0', sizeof(request_packet));
        request_packet[0] = 4;
        request_packet[1] = 1;
        request_packet[2] = port >> 8 & 0xFF;
        request_packet[3] = port & 0xFF;
        request_packet[4] = 0;
        request_packet[5] = 0;
        request_packet[6] = 0;
        request_packet[7] = 1;
        request_packet[8] = 0;
        memcpy(request_packet + 9, server.hostname.c_str(), sizeof(server.hostname));
        request_packet[9 + server.hostname.size()] = 0;

        ///
        // string vm = to_string((uint8_t)request_packet[0]);
        // string cd = to_string((uint8_t)request_packet[1]);
        // cout << "Fat" << endl;
        // cout << vm << endl;
        // cout << cd << endl;
        // //
        // // uint8_t two = (uint32_t)request_packet[2];
        // // uint8_t three = (uint32_t)request_packet[3];
        // // string dst_port = to_string((uint32_t)two * 256 + three);
        // // cout << "port::" << dst_port << endl;
        // string dst_port;
        // dst_port = to_string(ntohs(*((uint16_t *)&request_packet[2])));
        // cout << dst_port << endl;

        // uint8_t ip_1 = (uint8_t)request_packet[4];
        // uint8_t ip_2 = (uint8_t)request_packet[5];
        // uint8_t ip_3 = (uint8_t)request_packet[6];
        // uint8_t ip_4 = (uint8_t)request_packet[7];
        // string test_ip;
        // test_ip = to_string(ip_1) + "." + to_string(ip_2) + "." + to_string(ip_3) + "." + to_string(ip_4);
        // cout << "ip:" << test_ip << endl;

        // string try_ip;
        // try_ip = (string)&request_packet[9];

        // cout << "try_ip" << try_ip << endl;
        ///

        // 最後一格空白須寫入
        boost::asio::async_write(socket_, boost::asio::buffer(request_packet, 10 + server.hostname.size()), [&, this](boost::system::error_code ec, size_t write_length)
                                 {
            if (!ec) {
                        read_sock4_reply(server);
                    } else {
                        cerr << "Write => " << ec << endl;
                    } });
    }
    // void write_sock4_reply(int reply, tcp::endpoint proxy_endpoint)
    // {
    //     uint32_t ip = (uint32_t)proxy_endpoint.address().to_v4().to_ulong();
    //     uint16_t port = proxy_endpoint.port();
    //     char reply_packet[8];
    //     memset(reply_packet, '\0', sizeof(reply_packet));
    //     reply_packet[0] = 0;
    //     reply_packet[1] = reply;
    //     reply_packet[2] = port >> 8 & 0xFF;
    //     reply_packet[3] = port & 0xFF;
    //     reply_packet[4] = ip >> 24 & 0xFF;
    //     reply_packet[5] = ip >> 16 & 0xFF;
    //     reply_packet[6] = ip >> 8 & 0xFF;
    //     reply_packet[7] = ip & 0xFF;

    //     boost::asio::write(client_socket_, boost::asio::buffer(reply_packet, 8));
    // }
    void replace_string(string &data)
    {
        boost::replace_all(data, "\r", "");
        boost::replace_all(data, "\n", "&NewLine;");
        boost::replace_all(data, "<", "&lt;");
        boost::replace_all(data, "<", "&lt;");
        boost::replace_all(data, ">", "&gt;");
        boost::replace_all(data, " ", "&nbsp;");
        boost::replace_all(data, "\"", "&quot;");
        boost::replace_all(data, "\'", "&apos;");
    }
    void output_shell(const string element_id)
    {
        string output = string(data_);
        replace_string(output);
        cout << "<script>document.getElementById('" << element_id << "').innerHTML += '" << output << "';</script>" << endl;
    }
    void output_command(server_info &server)
    {
        string output;

        // 讀一行txt
        getline(server.fin, output);

        if (output == "exit")
        {
            server.fin.close();
            server.isOccupied = false;
        }

        boost::replace_all(output, "\r", "");
        output.append("\n");

        do_write(server, output.c_str(), output.size());

        replace_string(output);
        cout << "<script>document.getElementById('" << server.segment << "').innerHTML += '<b>" << output << "</b>';</script>" << endl;
    }
    void do_write(server_info &server, const char *buffer, size_t length)
    {
        boost::asio::async_write(socket_, boost::asio::buffer(buffer, length),
                                 [&, this, buffer](boost::system::error_code ec, size_t write_length)
                                 {
                                     if (!ec)
                                     {
                                     }
                                     else
                                     {
                                         cerr << "Write => " << ec << endl;
                                     }
                                 });
    }
    void do_read(server_info &server)
    {
        socket_.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, &server](boost::system::error_code ec, size_t length)
            {
                if (!ec)
                {
                    // 把np server傳來的東西印在瀏覽器
                    output_shell(server.segment);
                    // 如果data (np server的response)包含%，代表可以書一條指令給它。
                    if (strstr(data_, "% "))
                    {
                        output_command(server);
                    }
                    memset(data_, 0, max_length);
                    do_read(server);
                }
                else if (ec != boost::asio::error::eof)
                {
                    cerr << "Read => " << ec << endl;
                }
            });
    }
    void do_connect(server_info &server, tcp::resolver::iterator &it)
    {
        tcp::endpoint endpoint = it->endpoint();
        socket_.async_connect(endpoint, [this, &server](boost::system::error_code ec)
                              {
                if (!ec) {
                    write_sock4_request(server);
                }
                else {
                    cerr << "Connect => " << ec << endl << endl;
                } });
    }
    void do_resolve(server_info &server)
    {
        // cout << "proxy_host:" << proxy_hostname << endl;
        // cout << "proxy_port:" << proxy_port << endl;
        tcp::resolver::query query(proxy_hostname, proxy_port);
        resolver_.async_resolve(query, [this, &server](boost::system::error_code ec, tcp::resolver::iterator it)
                                {
                if (!ec) {
                    do_connect(server, it);
                }
                else {
                    cerr << "Resolve => " << ec << endl;
                } });
    }

    tcp::socket socket_;
    tcp::resolver resolver_;
    enum
    {
        max_length = 1024
    };
    char data_[max_length];
};

int main()
{
    cout << "Content-type: text/html" << endl
         << endl;

    server_info s[MAX_CLIENT_NUM];
    client c[MAX_CLIENT_NUM];
    vector<string> sigmentTable;

    split_query_string(sigmentTable);
    parse_query_string(sigmentTable, s);
    cout_html(s);

    for (int i = 0; i < MAX_CLIENT_NUM; i++)
    {
        if (s[i].isOccupied)
        {
            c[i].start(s[i]);
        }
    }

    io_service.run();
    return 0;
}