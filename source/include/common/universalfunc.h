#ifndef __OS__COMMON__UNIVERSALFUNC_H
#define __OS__COMMON__UNIVERSALFUNC_H
#include <common/types.h>
using namespace os::common;

        uint64_t factorial(int n);
        double sin(double x, int n);
		double cos(double x, int n);
        double pow(double base, double exponent);

        uint32_t customRand();
        uint32_t getRandomNumber(uint32_t min, uint32_t max);

#endif