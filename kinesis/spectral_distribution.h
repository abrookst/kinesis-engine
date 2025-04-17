#ifndef _SPECTRAL_DISTRIBUTION_H_
#define _SPECTRAL_DISTRIBUTION_H_

#include <unordered_map>
#include <glm/glm.hpp>


namespace Kinesis {
    namespace Raytracing {
        namespace Spectra {
            // array of lambdas
            const int num_lambdas = 7;
            const int lambdas[] = {400, 450, 475, 550, 580, 600, 700};

            // Helper functions for conversion from CIE XYZ to rgb and back
            glm::vec3 rgb_to_xyz(glm::vec3 &rgb);
            glm::vec3 xyz_to_rgb(glm::vec3 &xyz);

            // TODO: matching curves calculations here
            float x(float lambda);
            float y(float lambda);
            float z(float lambda);


            class SpectralDistribution {
            public:
                SpectralDistribution();
                SpectralDistribution(float *l);
                void combineSPD(SpectralDistribution spd, glm::vec3 dir, glm::vec3 norm, float f);
                void combineLambda(int lambda, float power, glm::vec3 dir, glm::vec3 norm, float f);
                glm::vec3 toXYZ();

            private:
                std::unordered_map<int, float> samples;
            };
        }
    }
}

#endif