#include <fcntl.h>
#include <sys/epoll.h>

#include "SnakeCommon.h"
#include "SnakeServer.h"

namespace {

void
setNonBlocking( const int fd ) {
   int flags = fcntl( fd, F_GETFL, 0 );
   if ( flags == -1 || fcntl( fd, F_SETFL, flags | O_NONBLOCK ) == -1 ) {
      throw std::runtime_error(
          std::format( "Failed to set non-blocking for socket {}", fd ) );
   }
}

} // namespace

namespace Network {

void
SnakeServer::initialize() {
   listenSock_ = socket( AF_INET, SOCK_STREAM, 0 );
   if ( listenSock_ == -1 ) {
      throw std::runtime_error( "Failed to open listening socket" );
   }
   sockaddr_in addr{};
   addr.sin_family = config_.family;
   addr.sin_addr.s_addr = INADDR_ANY;
   uint16_t port = 8080;
   addr.sin_port = htons( port );
   if ( bind( listenSock_,
              reinterpret_cast< sockaddr * >( &addr ),
              sizeof( addr ) ) == -1 ) {
      throw std::runtime_error( std::format( "Could not find port at {}", port ) );
   }
   if ( ::listen( listenSock_, 10 ) == -1 ) {
      throw std::runtime_error( std::format(
          "Failed to listen on socket {}, port {}", listenSock_, port ) );
   }

   epollFd_ = epoll_create1( 0 );
   if ( epollFd_ == -1 ) {
      throw std::runtime_error( "Failed to create epoll fd" );
   }
   registerListener();
}

void
SnakeServer::registerListener() {
   setNonBlocking( listenSock_ );
   epoll_event ev{ EPOLLIN, &listenSock_ };
   if ( epoll_ctl( epollFd_, EPOLL_CTL_ADD, listenSock_, &ev ) ) {
      throw std::runtime_error( "Failed to register epoll event for listener" );
   }
}

void
SnakeServer::registerConnection( const int clientSock ) {
   LOG( "Registering connection on socket {}", clientSock );
   setNonBlocking( clientSock );
   // Register the epoll
   auto [ it, success ] = connections_.insert(
       { nextClientId_,
         std::make_unique< ClientConnection >( nextClientId_, clientSock ) } );
   ++nextClientId_;
   assert( success );
   epoll_event ev{ EPOLLIN, it->second.get() };
   if ( epoll_ctl( epollFd_, EPOLL_CTL_ADD, clientSock, &ev ) ) {
      throw std::runtime_error( "Failed to register epoll event for new client" );
   }
}

void
SnakeServer::networkLoop() {
   LOG( "Entering server network loop" );
   constexpr int maxEvents = 1024;
   constexpr int timeoutMs = 1000;
   std::array< epoll_event, maxEvents > events;
   while ( !stop_.stop_requested() ) {
      int eventCount = epoll_wait( epollFd_, events.data(), maxEvents, timeoutMs );
      if ( eventCount == -1 && errno != EINTR ) {
         throw std::runtime_error(
             std::format( "epoll_wait failed with errno {}", errno ) );
      }
      for ( int i = 0; i < eventCount; ++i ) {
         if ( events[ i ].data.ptr == &listenSock_ ) {
            int clientSock = accept( listenSock_, nullptr, nullptr );
            if ( clientSock == -1 ) {
               LOG( "Failed to allocate socket for incoming client request" );
               continue;
            }
            registerConnection( clientSock );
            continue;
         }
         ClientConnection *readyClient =
             static_cast< ClientConnection * >( events[ i ].data.ptr );
         bool clientAlive = readyClient != nullptr;
         if ( clientAlive && events[ i ].events & EPOLLIN ) {
            clientAlive = onReadable( readyClient );
         }
         if ( clientAlive && events[ i ].events & EPOLLOUT ) {
            clientAlive = onWritable( readyClient );
         }
      }
   }
   LOG( "Stop requested on server -- exiting" );
}

void
SnakeServer::markWritable( ClientConnection *client ) {
   LOG( "Client {} marked as writable", client->clientId );
   epoll_event ev{ EPOLLIN | EPOLLOUT, { client } };
   if ( epoll_ctl( epollFd_, EPOLL_CTL_MOD, client->sock, &ev ) ) {
      throw std::runtime_error(
          std::format( "Failed to modify epoll event for existing client id {}",
                       client->clientId ) );
   }
}

bool
SnakeServer::onReadable( ClientConnection *readyClient ) {
   auto readBytes =
       ::recv( readyClient->sock, ingressBuff_.data(), recvBuffSize, 0 );
   LOG( "Read {} bytes from client {}", readBytes, readyClient->clientId );
   if ( readBytes == -1 && errno == EWOULDBLOCK ) {
      return true;
   } else if ( readBytes <= 0 ) {
      close( readyClient->sock );
      connections_.erase( readyClient->clientId );
      return false;
   }
   readyClient->accumulator.insert( readyClient->accumulator.end(),
                                    ingressBuff_.begin(),
                                    ingressBuff_.begin() + readBytes );
   SessionCommon::onReceive( readyClient->accumulator, readyClient->ingress, stop_ );
   return true;
}

bool
SnakeServer::onWritable( ClientConnection *readyClient ) {
   auto toSend = SessionCommon::prepareOutgoing( readyClient->egressBuff_,
                                                 readyClient->writeOffset_,
                                                 readyClient->sendOffset_,
                                                 readyClient->egress,
                                                 stop_ );
   if ( toSend.empty() ) {
      return true;
   }
   auto sentBytes = ::send( readyClient->sock, toSend.data(), toSend.size(), 0 );
   if ( sentBytes == -1 && errno == EWOULDBLOCK ) {
      return true;
   } else if ( sentBytes == -1 ) {
      close( readyClient->sock );
      connections_.erase( readyClient->clientId );
      readyClient = nullptr;
      return false;
   }
   LOG( "Sent {} bytes to client {}", sentBytes, readyClient->clientId );
   readyClient->sendOffset_ += sentBytes;
   if ( readyClient->sendOffset_ == readyClient->writeOffset_ ) {
      readyClient->sendOffset_ = 0;
      readyClient->writeOffset_ = 0;
   }
   return true;
}

SnakeServer::~SnakeServer() {
   if ( listenSock_ != -1 ) {
      epoll_ctl( epollFd_, EPOLL_CTL_DEL, listenSock_, nullptr );
      close( listenSock_ );
   }
   for ( auto & [ _, conn ] : connections_ ) {
      epoll_ctl( epollFd_, EPOLL_CTL_DEL, conn->sock, nullptr );
      close( conn->sock );
   }
   close( epollFd_ );
}

} // namespace Network
