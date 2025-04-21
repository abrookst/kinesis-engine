#include <cassert>
#include "mesh/material.h"
#include "raytracer/ray.h"
#include "raytracer/hit.h"

namespace Kinesis::Mesh
{
    Material::~Material()
    {
        // if (image != NULL) delete image;
    }

    const glm::vec3 Material::getDiffuseColor(float s, float t) const
    {
        return diffuseColor;
        /*
        if (!hasTextureMap())
            return diffuseColor;

        assert(image != NULL);

        // this is just using nearest neighbor and could be improved to
        // bilinear interpolation, etc.
        int i = int(s * image->Width()) % image->Width();
        int j = int(t * image->Height()) % image->Height();
        if (i < 0)
            i += image->Width();
        if (j < 0)
            j += image->Height();
        assert(i >= 0 && i < image->Width());
        assert(j >= 0 && j < image->Height());
        Color c = image->GetPixel(i, j);

        // we assume the texture is stored in sRGB and convert to linear for
        // computation.  It will be converted back to sRGB before display.
        float r = srgb_to_linear(c.r / 255.0);
        float g = srgb_to_linear(c.g / 255.0);
        float b = srgb_to_linear(c.b / 255.0);

        return glm::vec3(r, g, b);
        */
    }
/*
    void Material::ComputeAverageTextureColor()
    {
        assert(hasTextureMap());
        float r = 0;
        float g = 0;
        float b = 0;
        for (int i = 0; i < image->Width(); i++)
        {
            for (int j = 0; j < image->Height(); j++)
            {
                Color c = image->GetPixel(i, j);
                r += srgb_to_linear(c.r / 255.0);
                g += srgb_to_linear(c.g / 255.0);
                b += srgb_to_linear(c.b / 255.0);
            }
        }
        int count = image->Width() * image->Height();
        r /= float(count);
        g /= float(count);
        b /= float(count);
        diffuseColor = glm::vec3(r, g, b);
    }
*/
    glm::vec3 Material::Shade(const Kinesis::Raytracing::Ray &ray,
                              const Kinesis::Raytracing::Hit &hit,
                              const glm::vec3 &dirToLight,
                              const glm::vec3 &lightColor) const
    {

        glm::vec3 n = hit.getNormal();
        glm::vec3 e = ray.getDirection() * -1.0f;
        glm::vec3 l = dirToLight;

        glm::vec3 answer = glm::vec3(0, 0, 0);

        // emitted component
        // -----------------
        answer += getEmittedColor();

        // diffuse component
        // -----------------
        float dot_nl = glm::dot(n, l);
        if (dot_nl < 0)
            dot_nl = 0;
        answer += lightColor * getDiffuseColor(hit.get_s(), hit.get_t()) * dot_nl;

        // specular component (Phong)
        // ------------------
        // make up reasonable values for other Phong parameters
        glm::vec3 specularColor = reflectiveColor;
        float exponent = 100;

        // compute ideal reflection angle
        glm::vec3 r = (l * -1.0f) + n * (2 * dot_nl);
        r = glm::normalize(r);
        float dot_er = glm::dot(e, r);
        if (dot_er < 0)
            dot_er = 0;
        answer += lightColor * specularColor * float(pow(dot_er, exponent)) * dot_nl;

        return answer;
    }
}