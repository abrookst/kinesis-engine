#ifndef _RAYTRACE_HIT_H_
#define _RAYTRACE_HIT_H_

#include <float.h>
#include <ostream>

#include "ray.h"
#include "./spectral_distribution.h"
#include "./mesh/material.h"

class Material;

// Hit class mostly copied from Peter Shirley and Keith Morley
// ====================================================================
// ====================================================================

namespace Kinesis {
    namespace Raytracing {
        class Hit {
        public:

            // CONSTRUCTOR & DESTRUCTOR
            Hit() { 
              t = FLT_MAX;
              material = NULL;
              normal = glm::vec3(0,0,0); 
              texture_s = 0;
              texture_t = 0;
            }

            // ACCESSORS
            float getT() const { return t; }
            Kinesis::Mesh::Material* getMaterial() const { return material; }
            glm::vec3 getNormal() const { return normal; }
            float get_s() const { return texture_s; }
            float get_t() const { return texture_t; }

            // MODIFIER
            void set(float _t, Kinesis::Mesh::Material *m, glm::vec3 n) {
              t = _t; material = m; normal = n; 
              texture_s = 0; texture_t = 0; }

            void setTextureCoords(float t_s, float t_t) {
              texture_s = t_s; texture_t = t_t; 
            }

        private: 

            // REPRESENTATION
            float t;
            Kinesis::Mesh::Material *material;
            glm::vec3 normal;
            float texture_s, texture_t;
        };
    }

    inline std::ostream &operator<<(std::ostream &os, const Kinesis::Raytracing::Hit &h) {
      os << "Hit <" << h.getT() << ", < "
        << h.getNormal()[0] << "," 
        << h.getNormal()[1] << "," 
        << h.getNormal()[2] << " > > ";
      return os;
    }
}
// ====================================================================
// ====================================================================

#endif
