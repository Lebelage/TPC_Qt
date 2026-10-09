#pragma once

#include <cmath>
namespace tpc::analytics::models {

constexpr int DEFAULT_MODES_COUNT_ = 10;

inline double DefaultBasisR(int k, double r, double phi, double z)
{
    switch (k)
    {
    case 0:
        return 0.0;
    case 1:
        return -0.5 * r;
    case 2:
        return -r * z;
    case 3:
        return -1.5 * r * z * z + 0.375 * r * r * r;
    case 4:
        return std::cos(phi);
    case 5:
        return std::sin(phi);
    case 6:
        return z * std::cos(phi);
    case 7:
        return z * std::sin(phi);
    case 8:
        return r * std::cos(2.0 * phi);
    case 9:
        return r * std::sin(2.0 * phi);
    default:
        return 0.0;
    }
}

inline double DefaultBasisPhi(int k, double r, double phi, double z)
{
    switch (k)
    {
    case 0:
        return 0.0;
    case 1:
        return 0.0;
    case 2:
        return 0.0;
    case 3:
        return 0.0;
    case 4:
        return -std::sin(phi);
    case 5:
        return std::cos(phi);
    case 6:
        return -z * std::sin(phi);
    case 7:
        return z * std::cos(phi);
    case 8:
        return -r * std::sin(2.0 * phi);
    case 9:
        return r * std::cos(2.0 * phi);
    default:
        return 0.0;
    }
}

inline double DefaultBasisZ(int k, double r, double phi, double z)
{
    switch (k)
    {
    case 0:
        return 1.0;
    case 1:
        return z;
    case 2:
        return z * z - 0.5 * r * r;
    case 3:
        return z * z * z - 1.5 * r * r * z;
    case 4:
        return 0.0;
    case 5:
        return 0.0;
    case 6:
        return r * std::cos(phi);
    case 7:
        return r * std::sin(phi);
    case 8:
        return 0.0;
    case 9:
        return 0.0;
    default:
        return 0.0;
    }
}

} // namespace tpc::analytics::models
