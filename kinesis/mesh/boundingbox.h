#ifndef __BOUNDINGBOX_H__
#define __BOUNDINGBOX_H__

#include "math/vectors.h"

namespace Kinesis::Mesh {
	class BoundingBox
	{
		public:
			// ========================
			// CONSTRUCTOR & DESTRUCTOR
			BoundingBox()
			{
				Set(Kinesis::Math::Vector3::Zero(), Kinesis::Math::Vector3::Zero());
			}
			BoundingBox(const Kinesis::Math::Vector3 &pt)
			{
				Set(pt, pt);
			}
			BoundingBox(const Kinesis::Math::Vector3 &_minimum, const Kinesis::Math::Vector3 &_maximum)
			{
				Set(_minimum, _maximum);
			}

			// =========
			// ACCESSORS
			void Get(Kinesis::Math::Vector3 &_minimum, Kinesis::Math::Vector3 &_maximum) const
			{
				_minimum = minimum;
				_maximum = maximum;
			}
			const Kinesis::Math::Vector3 &getMin() const { return minimum; }
			const Kinesis::Math::Vector3 &getMax() const { return maximum; }
			void getCenter(Kinesis::Math::Vector3 &c) const
			{
				c = maximum;
				c -= minimum;
				c *= 0.5f;
				c += minimum;
			}
			double maxDim() const
			{
				double x = maximum.x() - minimum.x();
				double y = maximum.y() - minimum.y();
				double z = maximum.z() - minimum.z();
				return std::max(x, std::max(y, z));
			}
			// =========
			// MODIFIERS
			void Set(const BoundingBox &bb)
			{
				minimum = bb.minimum;
				maximum = bb.maximum;
			}
			void Set(const Kinesis::Math::Vector3 &_minimum, const Kinesis::Math::Vector3 &_maximum)
			{
				assert(minimum.x() <= maximum.x() &&
						minimum.y() <= maximum.y() &&
						minimum.z() <= maximum.z());
				minimum = _minimum;
				maximum = _maximum;
			}
			void Extend(const Kinesis::Math::Vector3 v)
			{
				minimum = Kinesis::Math::Vector3(std::min(minimum.x(), v.x()),
						std::min(minimum.y(), v.y()),
						std::min(minimum.z(), v.z()));
				maximum = Kinesis::Math::Vector3(std::max(maximum.x(), v.x()),
						std::max(maximum.y(), v.y()),
						std::max(maximum.z(), v.z()));
			}
			void Extend(const BoundingBox &bb)
			{
				Extend(bb.minimum);
				Extend(bb.maximum);
			}

		private:
			Kinesis::Math::Vector3 minimum;
			Kinesis::Math::Vector3 maximum;
	};

}

#endif // __BOUNDINGBOX_H__
