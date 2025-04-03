#ifndef _MATRIX_H_
#define _MATRIX_H_

//
// originally implemented by Justin Legakis
//

#include <cassert>

// ====================================================================
// ====================================================================

class Matrix
{

public:
  // -----------------------------------------------
  // CONSTRUCTORS, ASSIGNMENT OPERATOR, & DESTRUCTOR
  Matrix() { clear(); }
  Matrix(const Matrix &m);
  Matrix(const float d[16])
  {
    for (int i = 0; i < 16; i++)
    {
      data[i] = d[i];
    }
  }
  Matrix &operator=(const Matrix &m);

  // ----------------------------
  // SIMPLE ACCESSORS & MODIFIERS
  const double *get() const { return data; }
  double get(int row, int col) const
  {
    assert(row >= 0 && row < 4);
    assert(col >= 0 && col < 4);
    return data[row + col * 4];
  }
  void glGet(float float_data[16]) const
  {
    for (int i = 0; i < 16; i++)
    {
      float_data[i] = (float)data[i];
    }
  }
  void set(int row, int col, double v)
  {
    assert(row >= 0 && row < 4);
    assert(col >= 0 && col < 4);
    data[row + col * 4] = v;
  }
  void setToIdentity(); // 1s on the diagonal
  void clear();         // 0s everywhere

  // --------------------------
  // STANDARD MATRIX OPERATIONS
  void Transpose(Matrix &m) const;
  void Transpose() { Transpose(*this); }
  int Inverse(Matrix &m, double epsilon = 1e-08) const;
  int Inverse(double epsilon = 1e-08) { return Inverse(*this, epsilon); }

  // --------------------
  // OVERLOADED OPERATORS
  friend Matrix operator+(const Matrix &m1, const Matrix &m2);
  friend Matrix operator-(const Matrix &m1, const Matrix &m2);
  friend Matrix operator*(const Matrix &m1, const Matrix &m2);
  friend Vector3 operator*(const Matrix &m1, const Vector3 &v);
  friend Vector3 operator*(const Vector3 &v, const Matrix &m1) { return operator*(m1, v); }
  friend Vector4 operator*(const Matrix &m1, const Vector4 &v);
  friend Vector4 operator*(const Vector4 &v, const Matrix &m1) { return operator*(m1, v); }
  friend Matrix operator*(const Matrix &m1, double d);
  friend Matrix operator*(double d, const Matrix &m) { return m * d; }
  Matrix &operator+=(const Matrix &m)
  {
    *this = *this + m;
    return *this;
  }
  Matrix &operator-=(const Matrix &m)
  {
    *this = *this - m;
    return *this;
  }
  Matrix &operator*=(const double d)
  {
    *this = *this * d;
    return *this;
  }
  Matrix &operator*=(const Matrix &m)
  {
    *this = *this * m;
    return *this;
  }

  // ---------------
  // TRANSFORMATIONS
  static Matrix MakeTranslation(const Vector3 &v);
  static Matrix MakeScale(const Vector3 &v);
  static Matrix MakeScale(double s) { return MakeScale(Vector3(s, s, s)); }
  static Matrix MakeXRotation(double theta);
  static Matrix MakeYRotation(double theta);
  static Matrix MakeZRotation(double theta);
  static Matrix MakeAxisRotation(const Vector3 &v, double theta);

  // Use to transform a point with a matrix
  // that may include translation
  void Transform(Vector4 &v) const;
  void Transform(Vector3 &v) const
  {
    Vector4 v2 = Vector4(v.x(), v.y(), v.z(), 1);
    Transform(v2);
    v.set(v2.x() / v2.w(), v2.y() / v2.w(), v2.z() / v2.w());
  }

  // Use to transform the direction of the ray
  // (ignores any translation)
  void TransformDirection(Vector3 &v) const
  {
    Vector4 v2 = Vector4(v.x(), v.y(), v.z(), 0);
    Transform(v2);
    v.set(v2.x(), v2.y(), v2.z());
  }

  // --------------
  // INPUT / OUTPUT
  friend std::ostream &operator<<(std::ostream &ostr, const Matrix &m);
  friend std::istream &operator>>(std::istream &istr, Matrix &m);

  // ------------
  // DETERMINANTS
  static double det4x4(double a1, double a2, double a3, double a4,
                       double b1, double b2, double b3, double b4,
                       double c1, double c2, double c3, double c4,
                       double d1, double d2, double d3, double d4);
  static double det3x3(double a1, double a2, double a3,
                       double b1, double b2, double b3,
                       double c1, double c2, double c3);
  static double det2x2(double a, double b,
                       double c, double d);

private:
  // --------------
  // REPRESENTATION
  // column-major order
  double data[16];
};

// ====================================================================
// ====================================================================

#endif
