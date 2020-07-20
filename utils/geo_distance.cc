#include "geo_distance.hpp"
#include <cmath>

namespace speedtest::utils {
static const auto pi = std::acos(-1.0);

static double to_radian(float degrees) noexcept
{
    return (degrees * pi) / 180;
}

static double pow2(double base) noexcept
{
    return base * base;
}

double geo_distance(float lat1, float lon1, float lat2, float lon2) noexcept
{
    static constexpr const auto radius = 6371.0; // km

    auto dlat = to_radian(lat2 - lat1);
    auto dlon = to_radian(lon2 - lon1);

    auto psh_dlat = pow2(std::sin(dlat / 2));
    auto psh_dlon = pow2(std::sin(dlon / 2));

    auto a = psh_dlat + psh_dlon * std::cos(to_radian(lat1)) * std::cos(to_radian(lat2));
    auto c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

    return radius * c;
}
} /* namespace speedtest::utils */
