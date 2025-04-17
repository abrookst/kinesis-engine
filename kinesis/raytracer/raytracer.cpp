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
    /*
    for (int i = 0; i < mesh->numOriginalQuads(); i++)
    {
        Triangle *t = mesh->getOriginalQuad(i);
        if (t->intersect(ray, h))
            answer = true;
    }
    */
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
    Material *m = hit.getMaterial();
    assert(m != NULL);

    // rays coming from the light source are set to white, don't bother to ray trace further.
    if (m->getEmittedColor().Length() > 0.001)
    {
        // return distribution for light source if so
        // or empty so it doesn't cause issues with the rest of the distribution
        return /* */;
    }

    glm::vec3 normal = hit.getNormal();
    glm::vec3 point = ray.pointAtParameter(hit.getT());
    glm::vec3 answer;
    SpectralDistribution spd;

    // TODO: do we need this?
    /*
    glm::vec3 ambient_light = glm::vec3(args->mesh_data->ambient_light.data[0],
                                args->mesh_data->ambient_light.data[1],
                                args->mesh_data->ambient_light.data[2]);
    */

    // ----------------------------------------------
    //  start with the indirect light (ambient light)
    // TODO: this is point to be pdf at lambda actually
    glm::vec3 diffuse_color = m->getDiffuseColor(hit.get_s(), hit.get_t());
    // the usual ray tracing hack for indirect light
    answer = diffuse_color * ambient_light;

    // ----------------------------------------------
    // add contributions from each light that is not in shadow
    // TODO: get the lights int he scene and do shadow calculations
    //       idk how tf. shadow calculation works in this case
    //       leave this alone for now??
    /*
    int num_lights = mesh->getLights().size();
    for (int i = 0; i < num_lights; i++)
    {

        Face *f = mesh->getLights()[i];
        glm::vec3 lightColor = f->getMaterial()->getEmittedColor() * f->getArea();
        glm::vec3 myLightColor;
        glm::vec3 lightCentroid = f->computeCentroid();
        glm::vec3 dirToLightCentroid = lightCentroid - point;
        dirToLightCentroid.Normalize();

        float distToLightCentroid = 0;
        glm::vec3 light_point;
        glm::vec3 shadows = glm::vec3(0, 0, 0);

        for (int k = 0; k < args->mesh_data->num_shadow_samples; k++)
        {
            glm::vec3 light_point = f->RandomPoint();
            if (args->mesh_data->num_shadow_samples == 1)
                light_point = f->computeCentroid();
            else
                light_point = f->RandomPoint();

            glm::vec3 light_vec = light_point - point;
            light_vec.Normalize();
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
            }
        }
        
        if (args->mesh_data->num_shadow_samples == 0)
        {
            distToLightCentroid = (lightCentroid - point).Length();
            myLightColor = 1 / float(M_PI * distToLightCentroid * distToLightCentroid) * lightColor;
            answer += m->Shade(ray, hit, dirToLightCentroid, myLightColor);
            continue;
        }
        

        // add the lighting contribution from this particular light at this point
        // (fix this to check for blockers between the light & this surface)

        shadows /= args->mesh_data->num_shadow_samples;
        answer += shadows;
    }
        */

    // ----------------------------------------------
    // add contribution from reflection, if the surface is shiny

    glm::vec3 reflectiveColor = Spectra::rgb_to_xyz(m->getReflectiveColor()); // this is the color of the thing

    if (bounce_count > 0)
    {
        // TODO: instead of mirror direction, bounce in a random direction
        Ray reflect_ray = Ray(point, /*MirrorDirection(normal, ray.getDirection())*/, ray.getLambda());
        Hit reflect_hit = Hit();
        
        // calculate fresnel
        float fresnel;
        spd.combineSPD(TraceRay(reflect_ray, reflect_hit, bounce_count - 1), reflect_ray.getDirection(), reflect_hit.getNormal(), fresnel);
    }

    return spd;
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