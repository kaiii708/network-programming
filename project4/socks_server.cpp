#include <boost/asio/io_service.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>
#include <utility>
#include <boost/asio.hpp>
#include <sys/wait.h>
#include <fstream>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

using namespace std;
using boost::asio::ip::address;
using boost::asio::ip::tcp;
boost::asio::io_context io_context;

struct firewall_frame
{
  char oper[1];
  vector<string> permit_ip;
};

void ChildHandler(int signo)
{
  int status;
  while (waitpid(-1, &status, WNOHANG) > 0)
  {
  }
}

class session
    : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
      : client_socket_(std::move(socket)), server_socket_(io_context)
  {
    memset(request_data, '\0', max_length);
    memset(server_data, '\0', max_length);
    memset(client_data, '\0', max_length);
    uint16_t zero_port = 0;
    string zero_ip = "0.0.0.0";
    tcp::endpoint temp_zero_endpoint(address::from_string(zero_ip), zero_port);
    zero_endpoint = temp_zero_endpoint;
  }
  void start()
  {
    read_sock4_request();
  }

private:
  enum reply
  {
    Accept = 90,
    Reject
  };
  void do_resolve()
  {
    uint8_t ip_1 = (uint8_t)request_data[4];
    uint8_t ip_2 = (uint8_t)request_data[5];
    uint8_t ip_3 = (uint8_t)request_data[6];
    uint8_t ip_4 = (uint8_t)request_data[7];
    string ip;
    uint16_t port;
    port = ntohs(*((uint16_t *)&request_data[2]));
    if (ip_1 == 0 && ip_2 == 0 && ip_3 == 0 && ip_4 != 0)
    {
      // string ip = to_string(ip_1) + to_string(ip_2) + to_string(ip_3) + to_string(ip_4);
      ip = (string)&request_data[9];

      tcp::resolver resolver_(io_context);
      tcp::resolver::query query(ip, to_string(port));
      tcp::resolver::iterator it = resolver_.resolve(query);

      server_endpoint = *it;
      string new_ip = server_endpoint.address().to_string();
      vector<string> done_ip;
      boost::split(done_ip, new_ip, boost::is_any_of("."), boost::token_compress_on);
      request_data[4] = stoi(done_ip[0]) & 0xFF;
      request_data[5] = stoi(done_ip[1]) & 0xFF;
      request_data[6] = stoi(done_ip[2]) & 0xFF;
      request_data[7] = stoi(done_ip[3]) & 0xFF;
      // ip = ((tcp::endpoint)(*it)).address().to_string();
    }
    else
    {
      ip = to_string(ip_1) + "." + to_string(ip_2) + "." + to_string(ip_3) + "." + to_string(ip_4);

      tcp::endpoint temp_endpoint(address::from_string(ip), port);
      server_endpoint = temp_endpoint;
    }
  }
  // firewall:
  //  先將sockscofig做開檔並傳入ifstream,
  //  如果request_data中的CD為1,比較permit中的c
  //  else if CD為2,讀b厚的那一串
  bool firewall()
  {
    int i = 0;
    ifstream conf("./socks.conf");
    if (conf.is_open() == false)
    {
      return false;
    }
    string temp, head, cmd, permit_ip, request_ip_token[4];
    stringstream ss;

    ss << server_endpoint.address().to_string();
    while (getline(ss, temp, '.'))
    {
      request_ip_token[i++] = temp;
    }

    while (conf >> head >> cmd >> permit_ip)
    {
      bool permit = true;

      //
      if (cmd == "c" && request_data[1] != 1)
      {
        continue;
      }
      else if (cmd == "b" && request_data[1] != 2)
      {
        continue;
      }
      else
      {
        ss.clear();
        ss << permit_ip;
        for (i = 0; i < 4; ++i)
        {
          getline(ss, temp, '.');
          if (temp == "*")
          {
            continue;
          }
          else if (request_ip_token[i] != temp)
          {
            permit = false;
          }
        }
      }

      if (permit)
      {
        conf.close();
        return true;
      }
    }
    conf.close();
    return false;
  }
  // bool firewall()
  // {
  //   firewall_frame fframe[100];
  //   ifstream ifs("socks.conf", ios::in);
  //   if (!ifs.is_open())
  //   {
  //     cout << "Failed to open file.\n";
  //     return false; // EXIT_FAILURE
  //   }
  //   string permit_s;
  //   char oper[1];
  //   int index = 0;
  //   string ip;
  //   while (ifs >> permit_s >> oper >> ip)
  //   {
  //     strcpy(fframe[index].oper, oper);
  //     boost::split(fframe[index].permit_ip, ip, boost::is_any_of("."), boost::token_compress_on);
  //     index++;
  //   }
  //   ifs.close();

  //   if (request_data[1] == 1)
  //   {
  //     for (int i = 0; i < index; i++)
  //     {
  //       if (!strcmp(fframe[i].oper, "c"))
  //       {
  //         for (int j = 0; j < fframe[i].permit_ip.size(); j++)
  //         {
  //           string permit_ip = fframe[i].permit_ip[j];
  //           if (permit_ip == "*")
  //           {
  //             continue;
  //           }
  //           string ip = to_string((uint8_t)request_data[4 + j]);
  //           cout << "test_ip: " << ip << endl;
  //           if (ip != fframe[i].permit_ip[j])
  //           {
  //             cout << "test_ip: " << ip << endl;
  //             cout << "fframe[i].permit_ip[j]: " << fframe[i].permit_ip[j] << endl;
  //             write_proxy_status(Reject);
  //             // delete fframe;
  //             return false;
  //           }
  //         }
  //         write_proxy_status(Accept);
  //         cout << "firewallgothrough" << endl;
  //         // delete fframe;
  //         return true;
  //       }
  //     }
  //   }
  //   else if (request_data[1] == 2)
  //   {
  //     for (int i = 0; i < index; i++)
  //     {
  //       if (!strcmp(fframe[i].oper, "b"))
  //       {
  //         for (int j = 0; j < fframe[i].permit_ip.size(); j++)
  //         {
  //           string permit_ip = fframe[i].permit_ip[j];
  //           if (permit_ip == "*")
  //           {
  //             continue;
  //           }
  //           string ip = to_string((uint8_t)request_data[4 + j]);
  //           if (ip != fframe[i].permit_ip[j])
  //           {
  //             write_proxy_status(Reject);
  //             // delete fframe;
  //             return false;
  //           }
  //         }
  //         // delete fframe;
  //         return true;
  //       }
  //     }
  //   }
  // }

  void write_sock4_reply(int reply, tcp::endpoint proxy_endpoint)
  {
    uint32_t ip = (uint32_t)proxy_endpoint.address().to_v4().to_ulong();
    uint16_t port = proxy_endpoint.port();
    char reply_packet[8];
    memset(reply_packet, '\0', sizeof(reply_packet));
    reply_packet[0] = 0;
    reply_packet[1] = reply;
    reply_packet[2] = port >> 8 & 0xFF;
    reply_packet[3] = port & 0xFF;
    reply_packet[4] = ip >> 24 & 0xFF;
    reply_packet[5] = ip >> 16 & 0xFF;
    reply_packet[6] = ip >> 8 & 0xFF;
    reply_packet[7] = ip & 0xFF;

    boost::asio::write(client_socket_, boost::asio::buffer(reply_packet, 8));
  }

  void write_proxy_status(reply Reply)
  {
    string source_ip = client_socket_.remote_endpoint().address().to_string();
    string source_port = to_string(client_socket_.remote_endpoint().port());
    string des_ip = to_string(*(uint8_t *)&request_data[4]) + "." + to_string(*(uint8_t *)&request_data[5]) + "." + to_string(*(uint8_t *)&request_data[6]) + "." + to_string(*(uint8_t *)&request_data[7]);
    string des_port = to_string(ntohs(*((uint16_t *)&request_data[2])));

    cout << "<S_IP>: " << source_ip << endl;
    cout << "<S_PORT>: " << source_port << endl;

    cout << "<D_IP>: " << des_ip << endl;
    cout << "<D_PORT>: " << des_port << endl;
    if (request_data[1] == 1)
    {
      cout << "<Command>: "
           << "CONNECT" << endl;
    }
    else if (request_data[1] == 2)
    {
      cout << "<Command>: "
           << "BIND" << endl;
    }
    if (Reply == 90)
    {
      cout << "<Reply>: "
           << "Accept" << endl;
    }
    else if (Reply == 91)
    {
      cout << "<Reply>: "
           << "Reject" << endl;
    }
  }
  void do_close()
  {
    server_socket_.close();
    client_socket_.close();
  }
  void client_socket_read()
  {
    auto self(shared_from_this());
    client_socket_.async_read_some(boost::asio::buffer(client_data, max_length),
                                   [this, self](boost::system::error_code ec, std::size_t length)
                                   {
                                     if (!ec)
                                     {
                                       server_socket_write(length);
                                     }
                                     else
                                     {
                                       do_close();
                                     }
                                   });
  }
  void client_socket_write(std::size_t length)
  {
    auto self(shared_from_this());
    boost::asio::async_write(client_socket_, boost::asio::buffer(server_data, length),
                             [this, self](boost::system::error_code ec, std::size_t /*length*/)
                             {
                               if (!ec)
                               {
                                 memset(server_data, 0, max_length);
                                 server_socket_read();
                               }
                               else
                               {
                                 do_close();
                               }
                             });
  }

  void server_socket_read()
  {
    auto self(shared_from_this());
    server_socket_.async_read_some(boost::asio::buffer(server_data, max_length),
                                   [this, self](boost::system::error_code ec, std::size_t length)
                                   {
                                     if (!ec)
                                     {
                                       client_socket_write(length);
                                     }
                                     else
                                     {
                                       do_close();
                                     }
                                   });
  }

  void server_socket_write(std::size_t length)
  {
    auto self(shared_from_this());
    boost::asio::async_write(server_socket_, boost::asio::buffer(client_data, length),
                             [this, self](boost::system::error_code ec, std::size_t /*length*/)
                             {
                               if (!ec)
                               {
                                 memset(client_data, 0, max_length);
                                 client_socket_read();
                               }
                               else
                               {
                                 do_close();
                               }
                             });
  }

  void do_connect()
  {
    server_socket_.connect(server_endpoint);
  }
  void do_bind()
  {
    auto self(shared_from_this());
    tcp::acceptor ftpserv_acceptor_(io_context, tcp::endpoint(tcp::v4(), 0));
    write_sock4_reply(90, ftpserv_acceptor_.local_endpoint());
    ftpserv_acceptor_.accept(server_socket_);
    // write_proxy_status(Accept);
    write_sock4_reply(90, ftpserv_acceptor_.local_endpoint());
  }
  // void parse_sock4_request()
  // {
  //   if (request_data[0] != 4)
  //   {
  //     write_proxy_status(Reject);
  //     write_sock4_reply(91, zero_endpoint);
  //     return;
  //   }
  //   do_resolve();
  //   if (!firewall())
  //   {
  //     write_sock4_reply(91, zero_endpoint);
  //     client_socket_.close();
  //     return;
  //   }
  //   if (request_data[1] == 1)
  //   {
  //     do_connect();
  //   }
  //   else if (request_data[1] == 2)
  //   {
  //     do_bind();
  //   }
  //   server_socket_read();
  //   client_socket_read();
  // }

  void read_sock4_request()
  {
    auto self(shared_from_this());
    client_socket_.async_read_some(boost::asio::buffer(request_data, max_length),
                                   [this, self](boost::system::error_code ec, std::size_t length)
                                   {
                                     if (!ec)
                                     {
                                       if (request_data[0] != 4)
                                       {
                                         write_sock4_reply(91, client_socket_.local_endpoint());
                                         return;
                                       }
                                       do_resolve();
                                       if (!firewall())
                                       {
                                         write_proxy_status(Reject);
                                         write_sock4_reply(91, zero_endpoint);
                                         do_close();
                                         return;
                                       }
                                       if (request_data[1] == 1)
                                       {
                                         do_connect();
                                         if (server_socket_.native_handle() < 0)
                                         {
                                           write_proxy_status(Reject);
                                           write_sock4_reply(91, client_socket_.local_endpoint());

                                           do_close();
                                           return;
                                         }
                                         else
                                         {
                                           write_proxy_status(Accept);
                                           write_sock4_reply(90, client_socket_.local_endpoint());
                                         }
                                       }
                                       else if (request_data[1] == 2)
                                       {
                                         do_bind();
                                         if (server_socket_.native_handle() < 0)
                                         {
                                           write_proxy_status(Reject);
                                           write_sock4_reply(91, client_socket_.local_endpoint());
                                           do_close();
                                           return;
                                         }
                                         else
                                         {
                                           write_proxy_status(Accept);
                                         }
                                       }
                                       server_socket_read();
                                       client_socket_read();
                                     }
                                   });
  }

  tcp::socket client_socket_, server_socket_;
  tcp::endpoint server_endpoint;

  tcp::endpoint zero_endpoint;
  enum
  {
    max_length = 1024
  };

  char request_data[max_length];
  char client_data[max_length];
  char server_data[max_length];
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
            io_context.notify_fork(boost::asio::io_context::fork_prepare);
            signal(SIGCHLD, ChildHandler);
            pid_t pid = fork();
            while (pid < 0)
            {
              int status;
              waitpid(-1, &status, 0);
              pid = fork();
            }
            if (pid == 0)
            {
              // This is the child process.
              io_context.notify_fork(boost::asio::io_context::fork_child);
              acceptor_.close();
              std::make_shared<session>(std::move(socket))->start();
            }
            else
            {
              // This is the parent process.
              io_context.notify_fork(boost::asio::io_context::fork_parent);
              socket.close();
              do_accept();
            }
          }
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

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception &e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}