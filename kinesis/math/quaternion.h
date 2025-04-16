#ifndef __QUATERNION_H__
#define __QUATERNION_H__

#include <iostream>
#include <cmath>
#include <cassert>
#include "math/vectors.h"
#include "math/math.h"

namespace Kinesis::Math {
	class Quaternion
	{
		public:
			// -----------------------------------------------
			// CONSTRUCTORS, ASSIGNMENT OPERATOR, & DESTRUCTOR
			Quaternion()
			{
				data[0] = data[1] = data[2] = 0;
				data[3] = 1;
			}
			Quaternion(const Quaternion &Q)
			{
				data[0] = Q.data[0];
				data[1] = Q.data[1];
				data[2] = Q.data[2];
				data[3] = Q.data[3];
			}
			Quaternion(double x, double y, double z, double w)
			{
				data[0] = x;
				data[1] = y;
				data[2] = z;
				data[3] = w;
			}
			Quaternion(double x, double y, double z) { set(x, y, z); }
			Quaternion(const Vector3 &V) { set(V); }
			const Quaternion &operator=(const Quaternion &Q)
			{
				data[0] = Q.data[0];
				data[1] = Q.data[1];
				data[2] = Q.data[2];
				data[3] = Q.data[3];
				return *this;
			}

			// ----------------------------
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
			void setx(double x) { data[0] = x; }
			void sety(double y) { data[1] = y; }
			void setz(double z) { data[2] = z; }
			void setw(double w) { data[3] = w; }
			void set(double x, double y, double z, double w)
			{
				data[0] = x;
				data[1] = y;
				data[2] = z;
				data[3] = w;
			}
			void set(double x, double y, double z)
			{
				double angle;

				angle = x / 2;
				double sr = std::sin(angle);
				double cr = std::cos(angle);

				angle = y / 2;
				double sp = std::sin(angle);
				double cp = std::cos(angle);

				angle = z / 2;
				double sy = std::sin(angle);
				double cy = std::cos(angle);

				double cpcy = cp * cy;
				double spcy = sp * cy;
				double cpsy = cp * sy;
				double spsy = sp * sy;

				data[0] = sr * cpcy - cr * spsy;
				data[1] = cr * spcy + sr * cpsy;
				data[2] = cr * cpsy - sr * spcy;
				data[3] = cr * cpcy + sr * spsy;

				Normalize();
			}
			void set(const Vector3 &V)
			{
				set(V.x(), V.y(), V.z());
			}

			// ----------------
			// EQUALITY TESTING
			int operator==(const Quaternion &Q) const
			{
				return ((data[0] == Q.data[0]) &&
						(data[1] == Q.data[1]) &&
						(data[2] == Q.data[2]) &&
						(data[3] == Q.data[3]));
			}
			int operator!=(const Quaternion &Q) const
			{
				return ((data[0] != Q.data[0]) ||
						(data[1] != Q.data[1]) ||
						(data[2] != Q.data[2]) ||
						(data[3] != Q.data[3]));
			}

			// ----------------------------
			// COMMON QUATERNION OPERATIONS
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
					Scale(1 / length);
			}
			Quaternion Normalized()
			{
				Quaternion q1 = *this;
				q1.Normalize();
				return q1;
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
			void Invert() { Scale(-1.0, -1.0, -1.0, 1.0); }
			Quaternion Inverse()
			{
				Quaternion q = *this;
				q.Invert();
				return q;
			}
			double Dot(const Quaternion &Q) const
			{
				return data[0] * Q.data[0] +
					data[1] * Q.data[1] +
					data[2] * Q.data[2] +
					data[3] * Q.data[3];
			}

			// -------------------------
			// QUATERNION MATH OPERATORS
			Quaternion &operator+=(const Quaternion &Q)
			{
				data[0] += Q.data[0];
				data[1] += Q.data[1];
				data[2] += Q.data[2];
				data[3] += Q.data[3];
				return *this;
			}
			Quaternion &operator-=(const Quaternion &Q)
			{
				data[0] -= Q.data[0];
				data[1] -= Q.data[1];
				data[2] -= Q.data[2];
				data[3] -= Q.data[3];
				return *this;
			}
			Quaternion &operator*=(const Quaternion &Q)
			{
				double W, X, Y, Z;

				W = w() * Q.w() - x() * Q.x() - y() * Q.y() - z() * Q.z();
				X = x() * Q.w() - w() * Q.x() - z() * Q.y() - y() * Q.z();
				Y = y() * Q.w() - w() * Q.y() - x() * Q.z() - z() * Q.x();
				Z = z() * Q.w() - w() * Q.z() - y() * Q.x() - x() * Q.y();

				data[0] = X;
				data[1] = Y;
				data[2] = Z;
				data[3] = W;
				return *this;
			}
			Quaternion &operator*=(double d)
			{
				data[0] *= d;
				data[1] *= d;
				data[2] *= d;
				data[3] *= d;
				return *this;
			}
			Quaternion &operator/=(double d)
			{
				data[0] /= d;
				data[1] /= d;
				data[2] /= d;
				data[3] /= d;
				return *this;
			}
			friend Quaternion operator+(const Quaternion &q1, const Quaternion &q2)
			{
				Quaternion q3 = q1;
				q3 += q2;
				return q3;
			}
			friend Quaternion operator-(const Quaternion &q1)
			{
				Quaternion q2 = q1;
				q2.Negate();
				return q2;
			}
			friend Quaternion operator-(const Quaternion &q1, const Quaternion &q2)
			{
				Quaternion q3 = q1;
				q3 -= q2;
				return q3;
			}
			friend Quaternion operator*(const Quaternion &q1, const Quaternion &q2)
			{
				Quaternion q3 = q1;
				q3 *= q2;
				return q3;
			}
			friend Quaternion operator*(const Quaternion &q1, double d)
			{
				Quaternion q2 = q1;
				q2 *= d;
				return q2;
			}
			friend Quaternion operator*(double d, const Quaternion &q1)
			{
				return q1 * d;
			}
			Vector3 operator*(const Vector3 &V)
			{
				Vector3 uv, uuv;
				Vector3 qvec(data[0], data[1], data[2]);
				Vector3::Cross3(uv, qvec, V);
				Vector3::Cross3(uuv, qvec, uv);
				uv *= (2.0 * data[3]);
				uuv *= 2.0;
				return V + uv + uuv;
			}
			friend Quaternion operator/(const Quaternion &q1, double d)
			{
				Quaternion q2 = q1;
				q2 /= d;
				return q2;
			}

			// --------------
			// STATIC METHODS
			static Quaternion Lerp(const Quaternion &a, const Quaternion &b, float t)
			{
				t = clamp(t, 0.f, 1.f);
				return (1 - t) * a + t * b;
			}
			static Quaternion LerpUnclamped(const Quaternion &a, const Quaternion &b, float t)
			{
				return (1 - t) * a + t * b;
			}
			static Quaternion Slerp(Quaternion a, const Quaternion &b, float t)
			{
				double angle = a.Dot(b);
				if (angle < 0)
				{
					a *= -1;
					angle *= -1;
				}

				double theta = std::acos(angle);
				double arcsintheta = 1.0 / std::sin(theta);
				double scale = std::sin(theta * (1.0 - t)) * arcsintheta;
				double invscale = std::sin(theta * t) * arcsintheta;
				return a * scale + b * invscale;
			}
			static Quaternion AngleAxis(double angle, const Vector3 &axis)
			{
				double halfAngle = 0.5 * angle;
				double sA = std::sin(halfAngle);
				return Quaternion(sA * axis.x(), sA * axis.y(), sA * axis.z(), std::cos(halfAngle));
			}

			static Quaternion Identity() { return Quaternion(0, 0, 0, 1); }

			// --------------
			// INPUT / OUTPUT
			friend std::ostream &operator<<(std::ostream &ostr, const Quaternion &q)
			{
				ostr << q.data[0] << " " << q.data[1] << " " << q.data[2] << " " << q.data[3];
				return ostr;
			}
			friend std::istream &operator>>(std::istream &istr, Quaternion &q)
			{
				istr >> q.data[0] >> q.data[1] >> q.data[2] >> q.data[3];
				return istr;
			}

		private:
			double data[4];
	};

}

#endif // __QUATERNION_H__
