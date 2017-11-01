#include "ServerImpl.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <pthread.h>
#include <signal.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include <afina/execute/Command.h>
#include <afina/Storage.h>

namespace Afina {
namespace Network {
namespace Blocking {

void *ServerImpl::RunAcceptorPr(void *p)
{
    ServerImpl *srv = reinterpret_cast<ServerImpl *>(p);
    try
    {
        srv->RunAcceptor();
    }
    catch (std::runtime_error &ex)
    {
        std::cerr << "Error in server: " << ex.what() << std::endl;
    }
    return 0;
}

void *ServerImpl::RunConnectionProxy(void *p) {
    auto *srv_pair = reinterpret_cast<std::pair<ServerImpl*, int>*>(p);
    ServerImpl* srv = srv_pair->first;
    int client_socket = srv_pair->second;
    try {
        srv->RunConnection(client_socket);
    } catch (std::runtime_error &ex) {
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }
    return 0;
}

// See Server.h
ServerImpl::ServerImpl(std::shared_ptr<Afina::Storage> ps) : Server(ps) {}

// See Server.h
ServerImpl::~ServerImpl() {}

// See Server.h
void ServerImpl::Start(uint32_t port, uint16_t n_workers) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // If a client closes a connection, this will generally produce a SIGPIPE
    // signal that will kill the process. We want to ignore this signal, so send()
    // just returns -1 when this happens.
    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &sig_mask, NULL) != 0) {
        throw std::runtime_error("Unable to mask SIGPIPE");
    }

    // Setup server parameters BEFORE thread created, that will guarantee
    // variable value visibility
    max_workers = n_workers;
    listen_port = port;

    // The pthread_create function creates a new thread.
    //
    // The first parameter is a pointer to a pthread_t variable, which we can use
    // in the remainder of the program to manage this thread.
    //
    // The second parameter is used to specify the attributes of this new thread
    // (e.g., its stack size). We can leave it NULL here.
    //
    // The third parameter is the function this thread will run. This function *must*
    // have the following prototype:
    //    void *f(void *args);
    //
    // Note how the function expects a single parameter of type void*. We are using it to
    // pass this pointer in order to proxy call to the class member function. The fourth
    // parameter to pthread_create is used to specify this parameter value.
    //
    // The thread we are creating here is the "server thread", which will be
    // responsible for listening on port 23300 for incoming connections. This thread,
    // in turn, will spawn threads to service each incoming connection, allowing
    // multiple clients to connect simultaneously.
    // Note that, in this particular example, creating a "server thread" is redundant,
    // since there will only be one server thread, and the program's main thread (the
    // one running main()) could fulfill this purpose.
    running.store(true);
    if (pthread_create(&accept_thread, NULL, ServerImpl::RunAcceptorPr, this) < 0) {
        throw std::runtime_error("Could not create server thread");
    }
}

// See Server.h
void ServerImpl::Stop()
{
  std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
  if (pthread_kill(this->accept_thread, 0) == 0)
  {
    pthread_cancel(this->accept_thread);
  }

  running.store(false);
 
  for (auto it = this->connections.begin(); it != this->connections.end(); )
  {
    if (pthread_kill(*it, 0) != 0)
    {
      it = this->connections.erase(it);
    } else {
      pthread_join(*it, NULL);
    }
  }
}

// See Server.h
void ServerImpl::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    pthread_join(accept_thread, 0);
}

// See Server.h
void ServerImpl::RunAcceptor()
{
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // For IPv4 we use struct sockaddr_in:
    // struct sockaddr_in {
    //     short int          sin_family;  // Address family, AF_INET
    //     unsigned short int sin_port;    // Port number
    //     struct in_addr     sin_addr;    // Internet address
    //     unsigned char      sin_zero[8]; // Same size as struct sockaddr
    // };
    //
    // Note we need to convert the port to network order

    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;          // IPv4
    server_addr.sin_port = htons(listen_port); // TCP port number
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Bind to any address

    // Arguments are:
    // - Family: IPv4
    // - Type: Full-duplex stream (reliable)
    // - Protocol: TCP
    int server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == -1) {
        throw std::runtime_error("Failed to open socket");
    }

    // when the server closes the socket,the connection must stay in the TIME_WAIT state to
    // make sure the client received the acknowledgement that the connection has been terminated.
    // During this time, this port is unavailable to other processes, unless we specify this option
    //
    // This option let kernel knows that we are OK that multiple threads/processes are listen on the
    // same port. In a such case kernel will balance input traffic between all listeners (except those who
    // are closed already)
    int opts = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opts, sizeof(opts)) == -1) {
        close(server_socket);
        throw std::runtime_error("Socket setsockopt() failed");
    }

    // Bind the socket to the address. In other words let kernel know data for what address we'd
    // like to see in the socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        close(server_socket);
        throw std::runtime_error("Socket bind() failed");
    }

    // Start listening. The second parameter is the "backlog", or the maximum number of
    // connections that we'll allow to queue up. Note that listen() doesn't block until
    // incoming connections arrive. It just makesthe OS aware that this process is willing
    // to accept connections on this socket (which is bound to a specific IP and port)
    if (listen(server_socket, 5) == -1) {
        close(server_socket);
        throw std::runtime_error("Socket listen() failed");
    }

    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t sinSize = sizeof(struct sockaddr_in);
    while (running.load()) {
        std::cout << "network debug: waiting for connection..." << std::endl;

        // When an incoming connection arrives, accept it. The call to accept() blocks until
        // the incoming connection arrives
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &sinSize)) == -1) {
            close(server_socket);
            throw std::runtime_error("Socket accept() failed");
        }
        for (auto it = this->connections.begin(); it != this->connections.end();)
        {
          if (pthread_kill(*it, 0) != 0)
          {
            it = this->connections.erase(it);
          } else {
            ++it;
          }
        }
        if (this->connections.size() < this->max_workers)
        {
          pthread_t client_thread;
          auto srv_pair = std::make_pair(this, client_socket);
          if (pthread_create(&client_thread, NULL, ServerImpl::RunConnectionProxy, &srv_pair) < 0)
          {
            throw std::runtime_error("Could not create client connection thread");
          }
          this->connections.push_back(client_thread);
        } else {
          std::cout << "network debug: maximum number of workers is achieved." << std::endl;
          close(client_socket);
        }
    }
    // Cleanup on exit...
    close(server_socket);

    // Wait until for all connections to be complete
    std::unique_lock<std::mutex> __lock(connections_mutex);
    while (!connections.empty()) {
        connections_cv.wait(__lock);
    }
}

// See Server.h

void ServerImpl::RunConnection(int client_socket)
{
  std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl; 
  int sendbuf_len; 
  socklen_t sendbuf_type_size = sizeof(sendbuf_len);
  if (getsockopt(client_socket, SOL_SOCKET, SO_SNDBUF, &sendbuf_len, &sendbuf_type_size))
  {
    throw std::runtime_error("Error getsockopt()");
  }

  char buf[sendbuf_len + 1];
  std::string command_buf;
  ssize_t s;
  size_t parsed; 
  Protocol::Parser parser;
  uint32_t body_size;
  
  while (running.load())
  {
    std::string out("");

    if ((s = recv(client_socket, buf, sendbuf_len, 0)) < 0)
    {
      close(client_socket);
      throw std::runtime_error("Socket recv() error");
    }
    if (s == 0)
    {
      break;
    }
    buf[s] = '\0';
    std::cout << "network debug: " << "received command: " << std::string(buf) << std::endl;

    command_buf += std::string(buf);

    bool b_str = parser.Parse(command_buf.data(), s, parsed);
    command_buf.erase(0, parsed);

    if (b_str)
    {
      auto com = parser.Build(body_size);

      if (body_size > 0)
      {
        std::cout << "network debug: " << "parsed command, " << std::endl 
          << "body_size = " << body_size << std::endl
          << "parsed = " << parsed << std::endl;

        while(command_buf.size() < body_size)
        {
          ssize_t t = recv(client_socket, buf, sendbuf_len, 0);
          if (t < 0)
          {
            close(client_socket);
            throw std::runtime_error("Cannot read arguments, socket recv() failed");
          }
          buf[t] = '\0';
          command_buf += std::string(buf);
        }

        std::cout << "network debug: " << "received arguments" << std::endl; 
        std::string args;
        std::copy(command_buf.begin(), command_buf.begin() + body_size, std::back_inserter(args));
        command_buf.erase(0, body_size);
        com->Execute(*this->pStorage, args, out);
        if (send(client_socket, out.data(), out.size(), 0) <= 0)
        {
          close(client_socket);
          throw std::runtime_error("Socket send() error");
        }
        std::cout << "network debug: " << "executed command and sent results" << std::endl; 
      }
    }
  }
  close(client_socket);
}

} // namespace Blocking
} // namespace Network
} // namespace Afina
