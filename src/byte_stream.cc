#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  // Your code here.
  return is_closed_;
}

void Writer::push( string data )
{
  // Your code here.
  if (is_closed_ || available_capacity() == 0 || data.empty()) {
    return;
  }
  uint64_t const push_size = std::min(data.size(),available_capacity());
  if(push_size < data.size()) {
    data = data.substr(0,push_size);
  }
  buffer_.push_back(std::move(data));
  buffer_bytes_size_ += push_size;
  pushcnt_ += push_size;
  return;
}

void Writer::close()
{
  // Your code here.
  is_closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return (capacity_ - buffer_bytes_size_);
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return pushcnt_;
}

bool Reader::is_finished() const
{
  // Your code here.
  return (is_closed_ && pushcnt_ == popcnt_);
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return popcnt_;
}

string_view Reader::peek() const
{
  // Your code here.
  if (buffer_.empty()) {
    return {};
  }
  return std::string_view(buffer_.front());
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  uint64_t pop_size = std::min(len,buffer_bytes_size_);
  buffer_bytes_size_ -= pop_size;
  popcnt_ += pop_size;
  while (pop_size > 0)
  {
    uint64_t const to_pop_size = buffer_.front().size();
    if(to_pop_size <= pop_size) {
      buffer_.pop_front();
      pop_size -= to_pop_size;
    } else {
      buffer_.front().erase(0, pop_size);
      pop_size = 0;
    }
  }
  return;
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return pushcnt_ - popcnt_;
}
