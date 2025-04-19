#include "raytracer.h"
#include "./gameobject.h"

using namespace Kinesis::Raytracing;

// ===========================================================================
// casts a single ray through the scene geometry and finds the closest hit

// rasterization is fine to remove since that was used with radiosity
bool Kinesis::Raytracing::RayTracer::CastRay(const Ray &ray, Hit &h) const
{
    bool answer = false;

    // TODO: check each gameobject in the scene and see if there is an intersection w/ a tri on the object
    // checks if material is transmissive. see triangle.cpp
    for (Kinesis::GameObject obj : gameObjects) {
        // get each tri in game object's mesh
        for (Kinesis::Mesh::Triangle *t : ) {
            if (t->intersect(ray, h))
                answer = true;
        }
    }
    return answer;
}

// ===========================================================================
// does the recursive (shadow rays & recursive rays) work
SpectralDistribution RayTracer::TraceRay(Ray &ray, Hit &hit, int bounce_count) const
{

    // First cast a ray and see if we hit anything.
    hit = Hit();
    bool intersect = CastRay(ray, hit);

    // if there is no intersection, simply return the background color
    if (intersect == false)
    {
        // TODO: figure out what bg color we want here
        return /* */;
    }

    // otherwise decide what to do based on the material
    // TODO: figure out material class
    /* material class for this requires:
        - normal material rgb info 
            - upon first compute there could be an appended spd for the material
            - then, at the location of the hit for that lambda, the pdf is grabbed
            - if the exact lambda isn't in the spd, estimate?
        - refractive index
        - emits light or no
    */
    Kinesis::Mesh::Material *m = hit.getMaterial();
    assert(m != NULL);

    glm::vec3 normal = hit.getNormal();
    glm::vec3 point = ray.pointAtParameter(hit.getT());
    glm::vec3 answer;
    SpectralDistribution spd = SpectralDistribution();


    // ----------------------------------------------
    // add contributions from each light that is not in shadow
    // TODO: get the lights int he scene and do shadow calculations
    //       idk how tf. shadow calculation works in this case
    //       leave this alone for now??
    // Get the lights in the scenes. Store somewhere globally??
    //*
    /*
    int num_lights = mesh->getLights().size();
    for (int i = 0; i < num_lights; i++)
    {
        Kinesis::Light *l = ///;
        SpectralDistribution lightColor = // get light color spd :);
        
        glm::vec3 dirToLightCentroid = l->position - point;
        dirToLightCentroid = glm::normal(dirToLightCentroid);

        float distToLightCentroid = 0;
        glm::vec3 shadows = glm::vec3(0, 0, 0);

            glm::vec3 light_point =  l->position;
            glm::vec3 light_vec = light_point - point;
            light_vec = glm::normal(light_vec);
            Ray light_ray = Ray(point, light_vec);
            Hit light_hit = Hit();

            CastRay(light_ray, light_hit, false);

            if ((light_ray.pointAtParameter(light_hit.getT()) - light_point).Length() < EPSILON)
            {
                dirToLightCentroid = light_point - point;
                distToLightCentroid = dirToLightCentroid.Length();
                dirToLightCentroid.Normalize();
                myLightColor = 1 / float(M_PI * distToLightCentroid * distToLightCentroid) * lightColor;
                shadows += m->Shade(ray, hit, dirToLightCentroid, myLightColor);
                spd.combineSPD()
            }
                
        
        if (args->mesh_data->num_shadow_samples == 0)
        {
            distToLightCentroid = (lightCentroid - point).Length();
            myLightColor = 1 / float(M_PI * distToLightCentroid * distToLightCentroid) * lightColor;
            answer += m->Shade(ray, hit, dirToLightCentroid, myLightColor);
            continue;
        }

        shadows /= args->mesh_data->num_shadow_samples;
        answer += shadows;
    }
        // compute the perfect mirror direction
inline Vec3f MirrorDirection(const Vec3f &normal, const Vec3f &incoming) {
  float dot = incoming.Dot3(normal);
  Vec3f r = (incoming*-1.0f) + normal * (2 * dot);
  return r*-1.0f;
}
    //*/

    // ----------------------------------------------
    // add contribution from reflection, if the surface is shiny
    if (m->isReflective()) {    // 2
        // Hopefully we don't have this in our scenes, but I want the logic to be there
        if (bounce_count > 0)
        {
            // TODO: instead of mirror direction, bounce in a random direction
            glm::vec3 mirror_dir = (ray.getDirection()*-1.0f) + glm::dot(ray.getDirection(), normal) * 2 * normal;
            Ray reflect_ray = Ray(point, mirror_dir, ray.getLambda());
            Hit reflect_hit = Hit();
            
            // calculate fresnel
            float fresnel = 1.0f;
            spd.combineSPD(TraceRay(reflect_ray, reflect_hit, bounce_count - 1), reflect_ray.getDirection(), reflect_hit.getNormal(), fresnel);
        }
    }
    else if (m->isTransmissive()) { // 1
        // split ray and CRY.
        for (unsigned int i = 0; i < Spectra::num_lambdas; i++) {
            glm::vec3 transmiss_dir;
            Ray transmiss_ray = Ray(point, transmissive_dir, ray.getLambda());
            Hit transmiss_hit = Hit();
            TraceRay(transmiss_ray, transmiss_hit, spd, bounce_count);
        }
    }

    return spd;
}

/**
 * Special case of ray trace
 */
void TraceRay(Ray &ray, Hit &hit, SpectralDistribution &spd, int bounce_count = 0) const {
    hit = Hit();
    bool intersect = CastRay(ray, hit);

    // This is when a ray goes into the distance and cries
    if (intersect == false)
    {
        // TODO: figure out what bg color we want here
        // This should be default and can be searched with SpectralDistribution
        /**combineLambda(int lambda, float power, glm::vec3 dir, glm::vec3 norm, float f) */
        spd.combineLambda(ray.getLambda(), 1.0f, ray.getDirection(), hit.getNormal(), 1.0f);
        return;
    }
    
    Kinesis::Mesh::Material *m = hit.getMaterial();
    assert(m != NULL);

    glm::vec3 normal = hit.getNormal();
    glm::vec3 point = ray.pointAtParameter(hit.getT());
    glm::vec3 answer;
    SpectralDistribution spd;

    /*
    for (lights) {
        
    }
     */
}


/**
 * THE REAL MEAT
 */
glm::vec3 RayTracer::RayTraceDrawPixel() {
    // gets the color of a specified pixel
    // trace multiple rays with randomized lambda
}

/*
bool Kinesis::Mesh::Triangle::intersect(const Ray &r, Hit &h) const {
  // intersect with each of the subtriangles
  Vertex *a = (*this)[0];
  Vertex *b = (*this)[1];
  Vertex *c = (*this)[2];
  return triangle_intersect(r,h,a,b,c,intersect_backfacing);
}

bool Kinesis::Mesh::Triangle::plane_intersect(const Ray &r, Hit &h) const {

  // insert the explicit equation for the ray into the implicit equation of the plane

  // equation for a plane
  // ax + by + cz = d;
  // normal . p + direction = 0
  // plug in ray
  // origin + direction * t = p(t)
  // origin . normal + t * direction . normal = d;
  // t = d - origin.normal / direction.normal;

  Vec3f normal = computeNormal();
  float d = normal.Dot3((*this)[0]->get());

  float numer = d - r.getOrigin().Dot3(normal);
  float denom = r.getDirection().Dot3(normal);

  if (denom == 0) return 0;  // parallel to plane

  if (!this->material.isTransmissive() && normal.Dot3(r.getDirection()) >= 0) 
    return 0; // hit the backside

  float t = numer / denom;
  if (t > EPSILON && t < h.getT()) {
    h.set(t,this->getMaterial(),normal);
    assert (h.getT() >= EPSILON);
    return 1;
  }
  return 0;
}

bool Kinesis::Mesh::Triangle::triangle_intersect(const Ray &r, Hit &h, Vertex *a, Vertex *b, Vertex *c) const {

  // compute the intersection with the plane of the triangle
  Hit h2 = Hit(h);
  if (!plane_intersect(r,h2,intersect_backfacing)) return 0;  

  // figure out the barycentric coordinates:
  Vec3f Ro = r.getOrigin();
  Vec3f Rd = r.getDirection();
  // [ ax-bx   ax-cx  Rdx ][ beta  ]     [ ax-Rox ] 
  // [ ay-by   ay-cy  Rdy ][ gamma ]  =  [ ay-Roy ] 
  // [ az-bz   az-cz  Rdz ][ t     ]     [ az-Roz ] 
  // solve for beta, gamma, & t using Cramer's rule
  
  float detA = Matrix::det3x3(a->get().x()-b->get().x(),a->get().x()-c->get().x(),Rd.x(),
                              a->get().y()-b->get().y(),a->get().y()-c->get().y(),Rd.y(),
                              a->get().z()-b->get().z(),a->get().z()-c->get().z(),Rd.z());

  if (fabs(detA) <= 0.000001) return 0;
  assert (fabs(detA) >= 0.000001);
  
  float beta = Matrix::det3x3(a->get().x()-Ro.x(),a->get().x()-c->get().x(),Rd.x(),
                              a->get().y()-Ro.y(),a->get().y()-c->get().y(),Rd.y(),
                              a->get().z()-Ro.z(),a->get().z()-c->get().z(),Rd.z()) / detA;
  float gamma = Matrix::det3x3(a->get().x()-b->get().x(),a->get().x()-Ro.x(),Rd.x(),
                               a->get().y()-b->get().y(),a->get().y()-Ro.y(),Rd.y(),
                               a->get().z()-b->get().z(),a->get().z()-Ro.z(),Rd.z()) / detA;
  
  if (beta >= -0.00001 && beta <= 1.00001 &&
      gamma >= -0.00001 && gamma <= 1.00001 &&
      beta + gamma <= 1.00001) {
    h = h2;
    // interpolate the texture coordinates
    float alpha = 1 - beta - gamma;
    float t_s = alpha * a->get_s() + beta * b->get_s() + gamma * c->get_s();
    float t_t = alpha * a->get_t() + beta * b->get_t() + gamma * c->get_t();
    h.setTextureCoords(t_s,t_t);
    assert (h.getT() >= EPSILON);
    return 1;
  }

  return 0;
}
*/