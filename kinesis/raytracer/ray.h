#ifndef _RAYTRACE_RAY_H
#define _RAYTRACE_RAY_H

#include <iostream>
#include <glm/glm.hpp>

// Ray class mostly copied from Peter Shirley and Keith Morley
// ====================================================================
// ====================================================================

namespace Kinesis {
    namespace Raytracing {
        class Ray {
        public:
            // CONSTRUCTOR & DESTRUCTOR
            Ray (const glm::vec3 &orig, const glm::vec3 &dir, int l) {
                origin = orig; 
                direction = dir; 
                lambda = l; }

            // ACCESSORS
            const glm::vec3& getOrigin() const { return origin; }
            const glm::vec3& getDirection() const { return direction; }
            const int& getLambda() const { return lambda; }
            glm::vec3 pointAtParameter(float t) const {
                return origin+direction*t; }

        private:
            Ray () { assert(0); } // don't use this constructor

            // REPRESENTATION
            glm::vec3 origin;
            glm::vec3 direction;
            int lambda;
        };
    }
}

inline std::ostream &operator<<(std::ostream &os, const Kinesis::Raytracing::Ray &r) {
  os << "Ray < < " 
     << r.getOrigin()[0] << "," 
     << r.getOrigin()[1] << "," 
     << r.getOrigin()[2] << " > " 
     <<", "
     << r.getDirection()[0] << "," 
     << r.getDirection()[1] << "," 
     << r.getDirection()[2] << " > "
     <<", " 
     << r.getLambda() << ">";
  return os;
}

// ====================================================================
// ====================================================================

#endif
