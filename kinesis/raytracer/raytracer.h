#ifndef _RAYTRACER_H
#define _RAYTRACER_H

#ifndef EPSILON
#define EPSILON 0.0001
#endif

#include "ray.h"
#include "hit.h"
#include "./spectral_distribution.h"
#include "./mesh/mesh.h"

using namespace Kinesis::Raytracing::Spectra;

namespace Kinesis {
    namespace Raytracing {
  
    class Pixel {
    public:
        glm::vec3 v1,v2,v3,v4;
        glm::vec3 color;
    
    };

    class RayTracer {

        public:
            // CONSTRUCTOR & DESTRUCTOR
            RayTracer(/*Mesh *m*/) {
                //mesh = m;
            }  
          
            // casts a single ray through the scene geometry and finds the closest hit
            bool CastRay(const Ray &ray, Hit &h) const;
          
            // does the recursive work
            SpectralDistribution TraceRay(Ray &ray, Hit &hit, int bounce_count = 0) const;

            glm::vec3 RayTraceDrawPixel();
        
        private:
            // REPRESENTATION
            Kinesis::Mesh::Mesh *mesh;
            //ArgParser *args;
        };
    }
}

#endif