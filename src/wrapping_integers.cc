#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 {static_cast<uint32_t>((static_cast<uint32_t>(n) + zero_point.raw_value_) % (1UL << 32))};
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Calculate the difference with 32-bit wraparound handling
  uint64_t diff = static_cast<uint64_t>(raw_value_ - zero_point.raw_value_);
  const uint64_t MODULO = 1UL << 32;
  
  uint64_t remainder = checkpoint%MODULO;
  uint64_t quotient = checkpoint/MODULO;

  if(diff+MODULO-remainder >= MODULO + (MODULO>>1))
  {
    if(quotient>0)
      diff = diff+(quotient-1)*MODULO;
  }
  else if(diff+MODULO-remainder < MODULO - (MODULO>>1))
  {
    diff = diff+(quotient+1)*MODULO;
  }
  else
    diff = diff+quotient*MODULO;

  return diff;
}
