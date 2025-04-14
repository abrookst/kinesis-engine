#ifndef __VECTORS_H__
#define __VECTORS_H__

//
// originally implemented by Justin Legakis
//

#include <iostream>
#include <cmath>
#include <cassert>

#include "math/math.h"

class Matrix;

// ====================================================================
// ====================================================================

class Vector3
{

public:
    // -----------------------------------------------
    // CONSTRUCTORS, ASSIGNMENT OPERATOR, & DESTRUCTOR
    Vector3() { data[0] = data[1] = data[2] = 0; }
    Vector3(const Vector3 &V)
    {
        data[0] = V.data[0];
        data[1] = V.data[1];
        data[2] = V.data[2];
    }
    Vector3(double d0, double d1, double d2)
    {
        data[0] = d0;
        data[1] = d1;
        data[2] = d2;
    }
    const Vector3 &operator=(const Vector3 &V)
    {
        data[0] = V.data[0];
        data[1] = V.data[1];
        data[2] = V.data[2];
        return *this;
    }

    // ----------------------------
    // SIMPLE ACCESSORS & MODIFIERS
    double operator[](int i) const
    {
        assert(i >= 0 && i < 3);
        return data[i];
    }
    double x() const { return data[0]; }
    double y() const { return data[1]; }
    double z() const { return data[2]; }
    double r() const { return data[0]; }
    double g() const { return data[1]; }
    double b() const { return data[2]; }
    void setx(double x) { data[0] = x; }
    void sety(double y) { data[1] = y; }
    void setz(double z) { data[2] = z; }
    void set(double d0, double d1, double d2)
    {
        data[0] = d0;
        data[1] = d1;
        data[2] = d2;
    }

    // ----------------
    // EQUALITY TESTING
    int operator==(const Vector3 &V)
    {
        return ((data[0] == V.data[0]) &&
                (data[1] == V.data[1]) &&
                (data[2] == V.data[2]));
    }
    int operator!=(const Vector3 &V)
    {
        return ((data[0] != V.data[0]) ||
                (data[1] != V.data[1]) ||
                (data[2] != V.data[2]));
    }

    // ------------------------
    // COMMON VECTOR OPERATIONS
    inline double Magnitude() const
    {
        return sqrt(data[0] * data[0] + data[1] * data[1] + data[2] * data[2]);
    }
    inline double SqrMagnitude() const
    {
        return data[0] * data[0] + data[1] * data[1] + data[2] * data[2];
    }
    void Normalize()
    {
        double length = Magnitude();
        if (length > 0)
        {
            Scale(1 / length);
        }
    }
    Vector3 Normalized()
    {
        Vector3 v1 = *this;
        v1.Normalize();
        return v1;
    }
    void Scale(double d) { Scale(d, d, d); }
    void Scale(double d0, double d1, double d2)
    {
        data[0] *= d0;
        data[1] *= d1;
        data[2] *= d2;
    }
    void Negate() { Scale(-1.0); }
    double Dot3(const Vector3 &V) const
    {
        return data[0] * V.data[0] +
               data[1] * V.data[1] +
               data[2] * V.data[2];
    }
    static void Cross3(Vector3 &c, const Vector3 &v1, const Vector3 &v2)
    {
        double x = v1.data[1] * v2.data[2] - v1.data[2] * v2.data[1];
        double y = v1.data[2] * v2.data[0] - v1.data[0] * v2.data[2];
        double z = v1.data[0] * v2.data[1] - v1.data[1] * v2.data[0];
        c.data[0] = x;
        c.data[1] = y;
        c.data[2] = z;
    }

    // ---------------------
    // VECTOR MATH OPERATORS
    Vector3 &operator+=(const Vector3 &V)
    {
        data[0] += V.data[0];
        data[1] += V.data[1];
        data[2] += V.data[2];
        return *this;
    }
    Vector3 &operator-=(const Vector3 &V)
    {
        data[0] -= V.data[0];
        data[1] -= V.data[1];
        data[2] -= V.data[2];
        return *this;
    }
    Vector3 &operator*=(double d)
    {
        data[0] *= d;
        data[1] *= d;
        data[2] *= d;
        return *this;
    }
    Vector3 &operator/=(double d)
    {
        data[0] /= d;
        data[1] /= d;
        data[2] /= d;
        return *this;
    }
    friend Vector3 operator+(const Vector3 &v1, const Vector3 &v2)
    {
        Vector3 v3 = v1;
        v3 += v2;
        return v3;
    }
    friend Vector3 operator-(const Vector3 &v1)
    {
        Vector3 v2 = v1;
        v2.Negate();
        return v2;
    }
    friend Vector3 operator-(const Vector3 &v1, const Vector3 &v2)
    {
        Vector3 v3 = v1;
        v3 -= v2;
        return v3;
    }
    friend Vector3 operator*(const Vector3 &v1, double d)
    {
        Vector3 v2 = v1;
        v2.Scale(d);
        return v2;
    }
    friend Vector3 operator*(const Vector3 &v1, const Vector3 &v2)
    {
        Vector3 v3 = v1;
        v3.Scale(v2.x(), v2.y(), v2.z());
        return v3;
    }
    friend Vector3 operator*(double d, const Vector3 &v1)
    {
        return v1 * d;
    }
    friend Vector3 operator/(const Vector3 &v1, double d)
    {
        Vector3 v2 = v1;
        v2 /= d;
        return v2;
    }

    // --------------
    // STATIC METHODS
    static Vector3 Lerp(const Vector3 &a, const Vector3 &b, float t)
    {
        t = clamp(t, 0.f, 1.f);
        return a + (b - a) * t;
    }
    static Vector3 LerpUnclamped(const Vector3 &a, const Vector3 &b, float t)
    {
        return a + (b - a) * t;
    }
    static Vector3 Reflect(const Vector3 &inDir, const Vector3 &normal)
    {
        return -2.f * normal.Dot3(inDir) * normal + inDir;
    }
    static Vector3 Project(const Vector3 &vec, const Vector3 &onto)
    {
        double sqrMag = onto.SqrMagnitude();
        double eps = EPSILON;
        if (sqrMag < eps)
            return Vector3(0, 0, 0);
        double dot = vec.Dot3(onto);
        return (dot / sqrMag) * onto;
    }
    static double Distance(const Vector3 &a, const Vector3 &b)
    {
        return std::sqrt(a.x() * b.x() + a.y() * b.y() + a.z() * b.z());
    }

    static Vector3 Zero() { return Vector3(0, 0, 0); }
    static Vector3 One() { return Vector3(1, 1, 1); }

    // --------------
    // INPUT / OUTPUT
    friend std::ostream &operator<<(std::ostream &ostr, const Vector3 &v)
    {
        ostr << v.data[0] << " " << v.data[1] << " " << v.data[2];
        return ostr;
    }
    friend std::istream &operator>>(std::istream &istr, Vector3 &v)
    {
        istr >> v.data[0] >> v.data[1] >> v.data[2];
        return istr;
    }

private:
    friend class Matrix;

    // REPRESENTATION
    double data[3];
};

// ====================================================================
// ====================================================================

class Vector4
{

public:
    // -----------------------------------------------
    // CONSTRUCTORS, ASSIGNMENT OPERATOR, & DESTRUCTOR
    Vector4() { data[0] = data[1] = data[2] = data[3] = 0; }
    Vector4(const Vector4 &V)
    {
        data[0] = V.data[0];
        data[1] = V.data[1];
        data[2] = V.data[2];
        data[3] = V.data[3];
    }
    Vector4(double d0, double d1, double d2, double d3)
    {
        data[0] = d0;
        data[1] = d1;
        data[2] = d2;
        data[3] = d3;
    }
    Vector4 &operator=(const Vector4 &V)
    {
        data[0] = V.data[0];
        data[1] = V.data[1];
        data[2] = V.data[2];
        data[3] = V.data[3];
        return *this;
    }

    // SIMPLE ACCESSORS & MODIFIERS
    double operator[](int i) const
    {
        assert(i >= 0 && i < 4);
        return data[i];
    }
    double x() const { return data[0]; }
    double y() const { return data[1]; }
    double z() const { return data[2]; }
    double w() const { return data[3]; }
    double r() const { return data[0]; }
    double g() const { return data[1]; }
    double b() const { return data[2]; }
    double a() const { return data[3]; }
    void setx(double x) { data[0] = x; }
    void sety(double y) { data[1] = y; }
    void setz(double z) { data[2] = z; }
    void setw(double w) { data[3] = w; }
    void set(double d0, double d1, double d2, double d3)
    {
        data[0] = d0;
        data[1] = d1;
        data[2] = d2;
        data[3] = d3;
    }

    // ----------------
    // EQUALITY TESTING
    int operator==(const Vector4 &V) const
    {
        return ((data[0] == V.data[0]) &&
                (data[1] == V.data[1]) &&
                (data[2] == V.data[2]) &&
                (data[3] == V.data[3]));
    }
    int operator!=(const Vector4 &V) const
    {
        return ((data[0] != V.data[0]) ||
                (data[1] != V.data[1]) ||
                (data[2] != V.data[2]) ||
                (data[3] != V.data[3]));
    }

    // ------------------------
    // COMMON VECTOR OPERATIONS
    inline double Magnitude() const
    {
        return (double)sqrt(data[0] * data[0] + data[1] * data[1] + data[2] * data[2] + data[3] * data[3]);
    }
    inline double SqrMagnitude() const
    {
        return data[0] * data[0] + data[1] * data[1] + data[2] * data[2] + data[3] * data[3];
    }
    void Normalize()
    {
        double l = Magnitude();
        if (l > 0)
        {
            data[0] /= l;
            data[1] /= l;
            data[2] /= l;
        }
    }
    Vector4 Normalized()
    {
        Vector4 v1 = *this;
        v1.Normalize();
        return v1;
    }
    void Scale(double d) { Scale(d, d, d, d); }
    void Scale(double d0, double d1, double d2, double d3)
    {
        data[0] *= d0;
        data[1] *= d1;
        data[2] *= d2;
        data[3] *= d3;
    }
    void Negate() { Scale(-1.0); }
    double Dot4(const Vector4 &V) const
    {
        return data[0] * V.data[0] +
               data[1] * V.data[1] +
               data[2] * V.data[2] +
               data[3] * V.data[3];
    }
    static void Cross3(Vector4 &c, const Vector4 &v1, const Vector4 &v2)
    {
        double x = v1.data[1] * v2.data[2] - v1.data[2] * v2.data[1];
        double y = v1.data[2] * v2.data[0] - v1.data[0] * v2.data[2];
        double z = v1.data[0] * v2.data[1] - v1.data[1] * v2.data[0];
        c.data[0] = x;
        c.data[1] = y;
        c.data[2] = z;
        c.data[3] = 1;
    }
    void DivideByW()
    {
        if (data[3] != 0)
        {
            data[0] /= data[3];
            data[1] /= data[3];
            data[2] /= data[3];
        }
        else
        {
            data[0] = data[1] = data[2] = 0;
        }
        data[3] = 1;
    }

    // ---------------------
    // VECTOR MATH OPERATORS
    Vector4 &operator+=(const Vector4 &V)
    {
        data[0] += V.data[0];
        data[1] += V.data[1];
        data[2] += V.data[2];
        data[3] += V.data[3];
        return *this;
    }
    Vector4 &operator-=(const Vector4 &V)
    {
        data[0] -= V.data[0];
        data[1] -= V.data[1];
        data[2] -= V.data[2];
        data[3] -= V.data[3];
        return *this;
    }
    Vector4 &operator*=(double d)
    {
        data[0] *= d;
        data[1] *= d;
        data[2] *= d;
        data[3] *= d;
        return *this;
    }
    Vector4 &operator/=(double d)
    {
        data[0] /= d;
        data[1] /= d;
        data[2] /= d;
        data[3] /= d;
        return *this;
    }
    friend Vector4 operator+(const Vector4 &v1, const Vector4 &v2)
    {
        Vector4 v3 = v1;
        v3 += v2;
        return v3;
    }
    friend Vector4 operator-(const Vector4 &v1)
    {
        Vector4 v2 = v1;
        v2.Negate();
        return v2;
    }
    friend Vector4 operator-(const Vector4 &v1, const Vector4 &v2)
    {
        Vector4 v3 = v1;
        v3 -= v2;
        return v3;
    }
    friend Vector4 operator*(const Vector4 &v1, double d)
    {
        Vector4 v2 = v1;
        v2.Scale(d);
        return v2;
    }
    friend Vector4 operator*(const Vector4 &v1, const Vector4 &v2)
    {
        Vector4 v3 = v1;
        v3.Scale(v2.x(), v2.y(), v2.z(), v2.w());
        return v3;
    }
    friend Vector4 operator*(double d, const Vector4 &v1)
    {
        return v1 * d;
    }
    friend Vector4 operator/(const Vector4 &v1, double d)
    {
        Vector4 v2 = v1;
        v2 /= d;
        return v2;
    }

    // --------------
    // STATIC METHODS
    static Vector4 Lerp(const Vector4 &a, const Vector4 &b, float t)
    {
        t = clamp(t, 0.f, 1.f);
        return a + (b - a) * t;
    }
    static Vector4 LerpUnclamped(const Vector4 &a, const Vector4 &b, float t)
    {
        return a + (b - a) * t;
    }
    static Vector4 Reflect(const Vector4 &inDir, const Vector4 &normal)
    {
        return -2.f * normal.Dot4(inDir) * normal + inDir;
    }
    static Vector4 Project(const Vector4 &vec, const Vector4 &onto)
    {
        double sqrMag = onto.SqrMagnitude();
        double eps = EPSILON;
        if (sqrMag < eps)
            return Vector4(0, 0, 0, 0);
        double dot = vec.Dot4(onto);
        return (dot / sqrMag) * onto;
    }
    static double Distance(const Vector4 &a, const Vector4 &b)
    {
        return std::sqrt(a.x() * b.x() + a.y() * b.y() + a.z() * b.z() + a.w() * b.w());
    }

    static Vector4 Zero() { return Vector4(0, 0, 0, 0); }
    static Vector4 One() { return Vector4(1, 1, 1, 1); }

    // --------------
    // INPUT / OUTPUT
    friend std::ostream &operator<<(std::ostream &ostr, const Vector4 &v)
    {
        ostr << v.data[0] << " " << v.data[1] << " " << v.data[2] << " " << v.data[3];
        return ostr;
    }
    friend std::istream &operator>>(std::istream &istr, Vector4 &v)
    {
        istr >> v.data[0] >> v.data[1] >> v.data[2] >> v.data[3];
        return istr;
    }

private:
    friend class Matrix;

    // REPRESENTATION
    double data[4];
};

// ====================================================================
// ====================================================================

#endif // __VECTORS_H__
