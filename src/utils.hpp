#ifndef _utils_h
#define _utils_h

#include <cstdint>

void(* reboot) (void) = 0;

signed long diffUnsignedLong(unsigned long first, unsigned long second) {
    unsigned  long abs_diff = (first > second) ? (first - second): (second - first);
    return (first > second) ? (signed long)abs_diff : -(signed long)abs_diff;
}

int16_t diff16(uint16_t first, uint16_t second)
{
    uint16_t abs_diff = (first > second) ? (first - second): (second - first);
    return (first > second) ? (int16_t )abs_diff : -(int16_t )abs_diff;
}



#endif