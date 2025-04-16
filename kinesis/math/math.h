#ifndef __MATH_H__
#define __MATH_H__

#define EPSILON 1e-04;
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Kinesis::Math {

template <class T>
T clamp(T x, T min, T max) {
    return x < 0 ? 0 : x > 1 ? 1 : x;
}

}

#endif // __MATH_H__
