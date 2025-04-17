#include "triangle.h"

bool Kinesis::Mesh::Triangle::intersect(const Kinesis::Raytracing::Ray &r, Kinesis::Raytracing::Hit &h) const {
    // intersect with each of the subtriangles
    Vertex *a = (*this)[0];
    Vertex *b = (*this)[1];
    Vertex *c = (*this)[2];
    return triangle_intersect(r,h,a,b,c);
  }
  
  bool Kinesis::Mesh::Triangle::plane_intersect(const Kinesis::Raytracing::Ray &r, Kinesis::Raytracing::Hit &h) const {
  
    // insert the explicit equation for the ray into the implicit equation of the plane
  
    // equation for a plane
    // ax + by + cz = d;
    // normal . p + direction = 0
    // plug in ray
    // origin + direction * t = p(t)
    // origin . normal + t * direction . normal = d;
    // t = d - origin.normal / direction.normal;
  
    glm::vec3 normal = computeNormal();
    float d = glm::dot(normal, (*this)[0]->position);
  
    float numer = d - glm::dot(r.getOrigin(), normal);
    float denom = glm::dot(r.getDirection(), normal);
  
    if (denom == 0) return 0;  // parallel to plane
  
    // TODO: add the correct
    if (/*!this->material.isTransmissive() &&*/ glm::dot(normal, r.getDirection()) >= 0) 
      return 0; // hit the backside
  
    float t = numer / denom;
    if (t > 0.0001 && t < h.getT()) {
      h.set(t, this->getMaterial(),normal);
      assert (h.getT() >= 0.0001);
      return 1;
    }
    return 0;
  }
  
  bool Kinesis::Mesh::Triangle::triangle_intersect(const Kinesis::Raytracing::Ray &r, Kinesis::Raytracing::Hit &h, Kinesis::Mesh::Vertex *a, Kinesis::Mesh::Vertex *b, Kinesis::Mesh::Vertex *c) const {
  
    // compute the intersection with the plane of the triangle
    Kinesis::Raytracing::Hit h2 = Kinesis::Raytracing::Hit(h);
    if (!plane_intersect(r,h2)) return 0;  
  
    // figure out the barycentric coordinates:
    glm::vec3 Ro = r.getOrigin();
    glm::vec3 Rd = r.getDirection();
    // [ ax-bx   ax-cx  Rdx ][ beta  ]     [ ax-Rox ] 
    // [ ay-by   ay-cy  Rdy ][ gamma ]  =  [ ay-Roy ] 
    // [ az-bz   az-cz  Rdz ][ t     ]     [ az-Roz ] 
    // solve for beta, gamma, & t using Cramer's rule
    
    float detA = this->det3x3(a->position[0]-b->position[0],a->position[0]-c->position[0],Rd[0],
                                a->position[1]-b->position[1],a->position[1]-c->position[1],Rd[1],
                                a->position[2]-b->position[2],a->position[2]-c->position[2],Rd[2]);
  
    if (fabs(detA) <= 0.000001) return 0;
    assert (fabs(detA) >= 0.000001);
    
    float beta = this->det3x3(a->position[0]-Ro[0],a->position[0]-c->position[0],Rd[0],
                                a->position[1]-Ro[1],a->position[1]-c->position[1],Rd[1],
                                a->position[2]-Ro[2],a->position[2]-c->position[2],Rd[2]) / detA;
    float gamma = this->det3x3(a->position[0]-b->position[0],a->position[0]-Ro[0],Rd[0],
                                 a->position[1]-b->position[1],a->position[1]-Ro[1],Rd[1],
                                 a->position[2]-b->position[2],a->position[2]-Ro[2],Rd[2]) / detA;
    
    if (beta >= -0.00001 && beta <= 1.00001 &&
        gamma >= -0.00001 && gamma <= 1.00001 &&
        beta + gamma <= 1.00001) {
      h = h2;
      // interpolate the texture coordinates
      /*
      float alpha = 1 - beta - gamma;
      float t_s = alpha * a->get_s() + beta * b->get_s() + gamma * c->get_s();
      float t_t = alpha * a->get_t() + beta * b->get_t() + gamma * c->get_t();
      h.setTextureCoords(t_s,t_t);
      */
      
      assert (h.getT() >= 0.0001);
      return 1;
    }
  
    return 0;
  }

double Kinesis::Mesh::Triangle::det3x3(double a1,double a2,double a3, double b1,double b2,double b3, double c1,double c2,double c3) const {
    return
    a1 * det2x2( b2, b3, c2, c3 )
    - b1 * det2x2( a2, a3, c2, c3 )
    + c1 * det2x2( a2, a3, b2, b3 );
}

double Kinesis::Mesh::Triangle::det2x2(double a, double b, double c, double d) const {
    return a * d - b * c;
}