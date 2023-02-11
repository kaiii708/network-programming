#include <boost/asio/io_service.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
// #include <sstream>
#include <fstream>
#include <unistd.h>
#include <utility>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>

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
// (a)REQUEST METHOD
// (b) REQUEST URI
// (c) QUERY STRING
// (d) SERVER PROTOCOL
// (e) HTTP HOST  127.0.0.1:8787
// (f) SERVER ADDR
// (g) SERVER PORT
// (h) REMOTE ADDR
// (i) REMOTE PORT
struct env_val
{
    string request_method;
    string request_uri;
    string query_string;
    string server_protocol;
    string http_host;
    string server_addr;
    string server_port;
    string remote_addr;
    string remote_port;
};
class client
{
public:
    client()
        : np_socket_(io_service),
          resolver_(io_service)
    {
        memset(data_, '\0', max_length);
    }
    void start(server_info &server, tcp::socket *socket)
    {
        browser_socket_ = socket;
        do_resolve(server);
    }

private:
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
        output = "<script>document.getElementById('" + element_id + "').innerHTML += '" + output + "';</script>\r\n";
        browser_do_write(output.c_str(), output.size());
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
        output = "<script>document.getElementById('" + server.segment + "').innerHTML += '<b>" + output + "</b>';</script>\r\n";
        browser_do_write(output.c_str(), output.size());
    }
    void do_write(server_info &server, const char *buffer, size_t length)
    {
        boost::asio::async_write(np_socket_, boost::asio::buffer(buffer, length),
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
    void browser_do_write(const char *buffer, size_t length)
    {
        boost::asio::async_write(*browser_socket_, boost::asio::buffer(buffer, length),
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
        np_socket_.async_read_some(
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
        np_socket_.async_connect(endpoint, [this, &server](boost::system::error_code ec)
                                 {
                if (!ec) {
                    do_read(server);
                }
                else {
                    cerr << "Connect => " << ec << endl << endl;
                } });
    }
    void do_resolve(server_info &server)
    {
        tcp::resolver::query query(server.hostname, server.port);
        // don't know what is this
        tcp::resolver::iterator it = resolver_.resolve(query);
        resolver_.async_resolve(query, [this, &server](boost::system::error_code ec, tcp::resolver::iterator it)
                                {
                if (!ec) {
                    do_connect(server, it);
                }
                else {
                    cerr << "Resolve => " << ec << endl;
                } });
    }

    tcp::socket np_socket_, *browser_socket_;
    tcp::resolver resolver_;
    enum
    {
        max_length = 1024
    };
    char data_[max_length];
};
class session
    : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket)
        : socket_(std::move(socket))
    {
    }

    void start()
    {
        do_read();
    }

private:
    void print_panel_cgi_host_menu(string &output)
    {
        for (int i = 1; i <= 12; ++i)
        {
            output.append("<option value=\"nplinux" + to_string(i) + "." + "cs.nctu.edu.tw\">nplinux" + to_string(i) + "</option>\r\n");
        }
    }

    void print_panel_cgi_test_case_menu(string &output)
    {
        for (int i = 1; i <= 5; ++i)
        {
            output.append("<option value=\"t" + to_string(i) + ".txt\">t" + to_string(i) + ".txt</option>\r\n");
        }
    }

    void print_panel_cgi_html_head(string &output)
    {
        output.append("<head>\r\n");
        output.append("<title>NP Project 3 Panel</title>\r\n");
        output.append("<link \r\n");
        output.append("rel=\"stylesheet\"\r\n");
        output.append("href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\r\n");
        output.append("integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\r\n");
        output.append("crossorigin=\"anonymous\"\r\n");
        output.append("/>\r\n");
        output.append("<link\r\n");
        output.append("href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\r\n");
        output.append("rel=\"stylesheet\"\r\n");
        output.append("/>\r\n");
        output.append("<link\r\n");
        output.append("rel=\"icon\"\r\n");
        output.append("type=\"image/png\"\r\n");
        output.append("href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\"\r\n");
        output.append("/>\r\n");
        output.append("<style>\r\n");
        output.append("* {\r\n");
        output.append("font-family: 'Source Code Pro', monospace;\r\n");
        output.append("}\r\n");
        output.append("</style>\r\n");
        output.append("</head>\r\n");
    }

    void print_panel_cgi_table_head(string &output)
    {
        output.append("<thead class=\"thead-dark\">\r\n");
        output.append("<tr>\r\n");
        output.append("<th scope=\"col\">#</th>\r\n");
        output.append("<th scope=\"col\">Host</th>\r\n");
        output.append("<th scope=\"col\">Port</th>\r\n");
        output.append("<th scope=\"col\">Input File</th>\r\n");
        output.append("</tr>\r\n");
        output.append("</thead>\r\n");
    }

    void print_panel_cgi_table_body(string &host_menu, string &test_case_menu, string &output)
    {
        output.append("<tbody>\r\n");

        for (int i = 0; i < MAX_CLIENT_NUM; ++i)
        {
            output.append("<tr>\r\n");
            output.append("<th scope=\"row\" class=\"align-middle\">Session " + to_string(i + 1) + "</th>\r\n");
            output.append("<td>\r\n");
            output.append("<div class=\"input-group\">\r\n");
            output.append("<select name=\"h" + to_string(i) + "\" class=\"custom-select\">\r\n");
            output.append("<option></option>" + host_menu);
            output.append("</select>\r\n");
            output.append("<div class=\"input-group-append\">\r\n");
            output.append("<span class=\"input-group-text\">.cs.nctu.edu.tw</span>\r\n");
            output.append("</div>\r\n");
            output.append("</div>\r\n");
            output.append("</td>\r\n");
            output.append("<td>\r\n");
            output.append("<input name=\"p" + to_string(i) + "\" type=\"text\" class=\"form-control\" size=\"5\" />\r\n");
            output.append("</td>\r\n");
            output.append("<td>\r\n");
            output.append("<select name=\"f" + to_string(i) + "\" class=\"custom-select\">\r\n");
            output.append("<option></option>\r\n");
            output.append(test_case_menu);
            output.append("</select>\r\n");
            output.append("</td>\r\n");
            output.append("</tr>\r\n");
        }

        output.append("<tr>\r\n");
        output.append("<td colspan=\"3\"></td>\r\n");
        output.append("<td>\r\n");
        output.append("<button type=\"submit\" class=\"btn btn-info btn-block\">Run</button>\r\n");
        output.append("</td>\r\n");
        output.append("</tr>\r\n");
        output.append("</tbody>\r\n");
    }

    void print_panel_cgi()
    {
        string output, host_menu, test_case_menu;
        print_panel_cgi_host_menu(host_menu);
        print_panel_cgi_test_case_menu(test_case_menu);

        output.append("Content-type: text/html\r\n\r\n");

        output.append("<!DOCTYPE html>\r\n");
        output.append("<html lang=\"en\">\r\n");
        print_panel_cgi_html_head(output);
        output.append("<body class=\"bg-secondary pt-5\">\r\n");
        output.append("<form action=\"console.cgi\" method=\"GET\">\r\n");
        output.append("<table class=\"table mx-auto bg-light\" style=\"width: inherit\">\r\n");
        print_panel_cgi_table_head(output);
        print_panel_cgi_table_body(host_menu, test_case_menu, output);
        output.append("</table>\r\n");
        output.append("</form>\r\n");
        output.append("</body>\r\n");
        output.append("</html>\r\n");

        do_write(output.c_str(), output.size());
    }
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

        string query_string = env.query_string;
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

    void cout_table_head(server_info s[], string &output)
    {
        output.append("<thead>\r\n");
        output.append("<tr>\r\n");
        for (int i = 0; i < MAX_CLIENT_NUM; ++i)
        {
            if (s[i].isOccupied)
            {
                output.append("<th scope=\"col\">" + s[i].hostname + ":" + s[i].port + "</th>\r\n");
            }
        }
        output.append("</tr>\r\n");
        output.append("</thead>\r\n");
    }

    void cout_table_body(server_info s[], string &output)
    {
        output.append("<tbody>\r\n");
        output.append("<tr>\r\n");
        for (int i = 0; i < MAX_CLIENT_NUM; ++i)
        {
            if (s[i].isOccupied)
            {
                output.append("<td><pre id=\"" + s[i].segment + "\" class=\"mb-0\"></pre></td>\r\n");
            }
        }
        output.append("</tr>\r\n");
        output.append("</tbody>\r\n");
    }

    void cout_html(server_info s[])
    {
        string output;
        output.append("Content-type: text/html\r\n\r\n");

        output.append("<!DOCTYPE html>\r\n");
        output.append("<html lang=\"en\">\r\n");
        output.append("<head>\r\n");
        output.append("<meta charset=\"UTF-8\" />\r\n");
        output.append("<title>NP Project 3 Sample Console</title>\r\n");
        output.append("<link\r\n");
        output.append("rel=\"stylesheet\"\r\n");
        output.append("href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\r\n");
        output.append("integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\r\n");
        output.append("crossorigin=\"anonymous\"\r\n");
        output.append("/>\r\n");
        output.append("<link\r\n");
        output.append("href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\r\n");
        output.append("rel=\"stylesheet\"\r\n");
        output.append("/>\r\n");
        output.append("<link\r\n");
        output.append("rel=\"icon\"\r\n");
        output.append("type=\"image/png\"\r\n");
        output.append("href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\r\n");
        output.append("/>\r\n");
        output.append("<style>\r\n");
        output.append("* {\r\n");
        output.append("font-family: 'Source Code Pro', monospace;\r\n");
        output.append("font-size: 1rem !important;\r\n");
        output.append("}\r\n");
        output.append("body {\r\n");
        output.append("background-color: #212529;\r\n");
        output.append("}\r\n");
        output.append("pre {\r\n");
        output.append("color: #cccccc;\r\n");
        output.append("}\r\n");
        output.append("b {\r\n");
        output.append("color: #01b468;\r\n");
        output.append("}\r\n");
        output.append("</style>\r\n");
        output.append("</head>\r\n");
        output.append("<body>\r\n");
        output.append("<table class=\"table table-dark table-bordered\">\r\n");

        cout_table_head(s, output);
        cout_table_body(s, output);

        output.append("</table>\r\n");
        output.append("</body>\r\n");
        output.append("</html>\r\n");

        do_write(output.c_str(), output.size());
    }

    void set_env_var()
    {
        vector<string> paras;
        boost::split(paras, data_, boost::is_any_of(" ,\r\n"));
        env.request_method = paras[0];
        env.request_uri = paras[1];
        env.server_protocol = paras[2];
        env.http_host = paras[5];
        cout << "data_:" << data_ << endl;
        cout << "env.request_method:" << env.request_method << endl;
        cout << "env.request_ur:" << env.request_uri << endl;
        cout << "env_server_protocol:" << env.server_protocol << endl;
        cout << "env.http_hos:" << env.http_host << endl;
        // sscanf(data_, "%s%s%s%s%s", &env.request_method, &env.request_uri, &env.server_protocol, temp, &env.http_host);
        int query_start_index = env.request_uri.find("?");
        if (query_start_index == -1)
        {
            env.query_string = "";
        }
        else
        {
            env.query_string = env.request_uri.substr(query_start_index + 1);
        }
        env.server_addr = socket_.local_endpoint().address().to_string();
        env.server_port = to_string(socket_.local_endpoint().port());
        env.remote_addr = socket_.remote_endpoint().address().to_string();
        env.remote_port = to_string(socket_.remote_endpoint().port());
    }

    void do_read()
    {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
                                [this, self](boost::system::error_code ec, std::size_t length)
                                {
                                    if (!ec)
                                    {
                                        set_env_var();
                                        strcpy(data_, "HTTP/1.1 200 OK\n");

                                        do_write(data_, strlen(data_));

                                        string to_exec;
                                        to_exec = env.request_uri;
                                        int index_to_erase = to_exec.find("?");
                                        if (index_to_erase != -1)
                                        {
                                            to_exec.erase(index_to_erase);
                                        }
                                        // cout << to_exec << endl;
                                        if (to_exec == "/panel.cgi")
                                        {
                                            print_panel_cgi();
                                        }
                                        else if (to_exec == "/console.cgi")
                                        {
                                            io_service.reset();
                                            string output;
                                            output = "Content-type: text/html\r\n";
                                            do_write(output.c_str(), output.size());

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
                                                    c[i].start(s[i], &socket_);
                                                }
                                            }

                                            io_service.run();
                                        }
                                        //  else
                                        //  {
                                        //      do_read();
                                        //  }
                                    }
                                });
    }

    void do_write(const char buffer[], std::size_t length)
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(buffer, length),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                     }
                                 });
    }

    tcp::socket socket_;
    enum
    {
        max_length = 1024
    };
    char data_[max_length];
    env_val env;
};

class server
{
public:
    server(boost::asio::io_context &io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    std::make_shared<session>(std::move(socket))->start();
                }

                do_accept();
            });
    }

    tcp::acceptor acceptor_;
};

int main(int argc, char *argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;

        server s(io_context, std::atoi(argv[1]));

        io_context.run();
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}