#include <common/universalfunc.h>
#include <common/types.h>

using namespace os::common;

        uint64_t factorial(int n) {
            if (n == 0) return 1;
            return n * factorial(n - 1);
        }
        double pow(double base, double exponent) {
            if (exponent == 0) {
                return 1.0;
            }

            double result = 1.0;
            int32_t positiveExponent = 1;

            if (exponent < 0) {
                positiveExponent = 0;
                exponent = -exponent;
            }

            for (int i = 0; i < exponent; i++) {
                result *= base;
            }

            if (!positiveExponent) {
                result = 1.0 / result;
            }

            return result;
        }
        double sin(double x, int n) {
            double result = 0;
            for (int32_t i = 0; i < n; i++) {
                result += (i % 2 == 0 ? 1 : -1) * (pow(x, 2 * i + 1) / factorial(2 * i + 1));
            }
            return result;
        }
    
        double cos(double x, int n) {
            double result = 0;
            for (int32_t i = 0; i < n; i++) {
                result += (i % 2 == 0 ? 1 : -1) * (pow(x, 2 * i) / factorial(2 * i));
            }
            return result;
        }
        static uint32_t rand_seed = 42;  // Initial seed value

// Custom random number generator
        uint32_t customRand() {
            // Linear congruential generator parameters
            const uint32_t a = 1664525;
            const uint32_t c = 1013904223;

            rand_seed = a * rand_seed + c;
            return rand_seed;
        }

        // Function to generate a random number within a specified range
        uint32_t getRandomNumber(uint32_t min, uint32_t max) {
            // Ensure the range is valid
            if (min >= max) {
                return min;
            }

            // Generate a random number within the specified range
            uint32_t randomNum = customRand() % (max - min) + min;
            return randomNum;
        }
       