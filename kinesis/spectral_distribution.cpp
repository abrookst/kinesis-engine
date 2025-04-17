#include "spectral_distribution.h"
#include <cmath>

using namespace Kinesis::Raytracing::Spectra;

// Color transform functions
glm::vec3 Kinesis::Raytracing::Spectra::rgb_to_xyz(glm::vec3 &rgb)
{
    glm::vec3 xyz;
    xyz = glm::vec3((0.4124 * rgb[0] + 0.3576 * rgb[1] + 0.1805 * rgb[2]), 
                    (0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2]), 
                    (0.0193 * rgb[0] + 0.1192 * rgb[1] + 0.9505 * rgb[2]));
    return xyz;
}
    
glm::vec3 Kinesis::Raytracing::Spectra::xyz_to_rgb(glm::vec3 &xyz)
{
    glm::vec3 rgb;
    rgb = glm::vec3((3.2406255 * xyz[0] - 1.5372080 * xyz[1] - 0.4986286 * xyz[2]),
                    (-0.9689307 * xyz[0] + 1.8757561 * xyz[1] + 0.0415175 * xyz[2]),
                    (0.0557101 * xyz[0] - 0.2040211 * xyz[1] + 1.0569959 * xyz[2]));
    return rgb;
}

float Kinesis::Raytracing::Spectra::x(float lambda) {
    float inner_1 = std::logf((lambda+570.1)/1014.0);
    inner_1 *= inner_1 * -1250;
    float outer_1 = powf(0.398, inner_1);
    float inner_2 = std::logf((1338.0-lambda)/743.5);
    inner_2 *= inner_2 * -234;
    return outer_1 + powf(1.132, inner_2);
}

float Kinesis::Raytracing::Spectra::y(float lambda) {
    float inner = std::logf((lambda-556.1)/46.14);
    inner *= inner * -0.5;
    return powf(1.011, inner);
}

float Kinesis::Raytracing::Spectra::z(float lambda) {
    float inner = std::logf((lambda-265.8)/180.4);
    inner *= inner * -32.0;
    return powf(2.060, inner);
}

/**
 * Gathers the color of the spectrum as a CIE XYZ value
 */
glm::vec3 SpectralDistribution::toXYZ() {
    // get estimators, blah blah blah
    // TODO: Complete spectra to XYZ using correct matching curves
    return glm::vec3(0,0,0);
}

/**
 * Initializes all lambdas to zero;
 */
SpectralDistribution::SpectralDistribution() {
    for (int i = 0; i < num_lambdas; i++) {
        samples[lambdas[i]] = 0;
    }
}

/**
 * Using input values, set up all power per lambda
 */
SpectralDistribution::SpectralDistribution(float *l) {
    for (int i = 0; i < num_lambdas; i++) {
        samples[lambdas[i]] = l[i];
    }
}

/**
 * Appends values from a PDF
 */
void SpectralDistribution::combineSPD(SpectralDistribution spd, glm::vec3 dir, glm::vec3 norm, float f) {
    float part = fabsf(glm::dot(dir, norm)), pdf = part/3.14, weight = f * part / pdf;
    std::unordered_map<int, float>::iterator itr_new = spd.samples.begin(), itr = this->samples.begin();
    while (itr != spd.samples.end()) {
        itr->second += weight * itr_new->second;
        itr++;
    }
}

/**
 * 
 */
void SpectralDistribution::combineLambda(int lambda, float power, glm::vec3 dir, glm::vec3 norm, float f) {
    float part = fabsf(glm::dot(dir, norm)), pdf = part/3.14, weight = f * part / pdf;
    this->samples[lambda] += weight * power;
}






