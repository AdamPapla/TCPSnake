#pragma once
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>

// A simple thread safe queue for passing data between threads
template < typename T >
class TSQueue {
 public:
   T pop() {
      std::unique_lock< std::mutex > lk( mutex_ );
      cv_.wait( lk, [ this ] { return !queue_.empty(); } );
      T val = std::move( queue_.front() );
      queue_.pop();
      return std::move( val );
   }
   void push( T newElem ) {
      {
         std::lock_guard< std::mutex > lk( mutex_ );
         queue_.push( std::move( newElem ) );
      }
      cv_.notify_one();
   }
   std::optional< T > try_front() const {
      std::lock_guard< std::mutex > lk( mutex_ );
      return queue_.empty() ? std::nullopt : std::optional< T >( queue_.front() );
   }
   std::size_t size() const {
      std::lock_guard< std::mutex > lk( mutex_ );
      return queue_.size();
   }

 private:
   mutable std::mutex mutex_;
   std::condition_variable cv_;
   std::queue< T > queue_;
};
