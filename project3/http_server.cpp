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

using namespace std;
using boost::asio::ip::tcp;

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
  static void childHandler(int signo)
  {
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0)
    {
    }
  }
  void set_env_var()
  {
    // (a)REQUEST METHOD
    // (b) REQUEST URI
    // (c) QUERY STRING
    // (d) SERVER PROTOCOL
    // (e) HTTP HOST  127.0.0.1:8787
    // (f) SERVER ADDR
    // (g) SERVER PORT
    // (h) REMOTE ADDR
    // (i) REMOTE PORT
    char request_method[max_length];
    char request_uri[max_length];
    char server_protocol[max_length];
    char temp[max_length];
    char http_host[max_length];

    sscanf(data_, "%s%s%s%s%s", request_method, request_uri, server_protocol, temp, http_host);
    setenv("REQUEST_METHOD", request_method, 1);
    setenv("REQUEST_URI", request_uri, 1);
    // strtok(request_uri, "?");
    // char *query_string = strtok(NULL, "?");
    char *query_string = strchr(request_uri, '?');
    if (query_string == NULL)
    {
      setenv("QUERY_STRING", "", 1);
    }
    else
    {
      setenv("QUERY_STRING", query_string + 1, 1);
    }
    setenv("SERVER_PROTOCOL", server_protocol, 1);
    setenv("HTTP_HOST", http_host, 1);
    setenv("SERVER_ADDR", socket_.local_endpoint().address().to_string().c_str(), 1);
    setenv("SERVER_PORT", to_string(socket_.local_endpoint().port()).c_str(), 1);
    setenv("REMOTE_ADDR", socket_.remote_endpoint().address().to_string().c_str(), 1);
    setenv("REMOTE_PORT", to_string(socket_.remote_endpoint().port()).c_str(), 1);
  }

  void get_file(char *exec_file)
  {
    // strcpy(exec_file, getenv("REQUEST_URI"));
    // exec_file = strtok(exec_file, "?");

    char uri[max_length];
    strcpy(exec_file, ".");
    strcpy(uri, getenv("REQUEST_URI"));
    strcat(exec_file, strtok(uri, "?"));
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
                                do_write(strlen(data_));
                              }
                            });
  }

  void do_write(std::size_t length)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(data_, length),
                             [this, self](boost::system::error_code ec, std::size_t /*length*/)
                             {
                               if (!ec)
                               {
                                 signal(SIGCHLD, childHandler);

                                 pid_t pid;
                                 pid = fork();
                                 if (pid == 0)
                                 {
                                   int sockfd = socket_.native_handle();
                                   dup2(sockfd, STDIN_FILENO);
                                   dup2(sockfd, STDOUT_FILENO);
                                   dup2(sockfd, STDERR_FILENO);
                                   socket_.close();
                                   char exec_file[max_length];
                                   get_file(exec_file);
                                   if (execlp(exec_file, "", NULL) < 0)
                                   {
                                     cerr << "ERROR: execlp()" << endl;
                                     exit(1);
                                   }
                                 }
                                 else
                                 {
                                   socket_.close();
                                 }
                                 do_read();
                               }
                             });
  }

  tcp::socket socket_;
  enum
  {
    max_length = 1024
  };
  char data_[max_length];
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