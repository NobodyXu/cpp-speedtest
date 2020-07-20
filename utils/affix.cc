#include "affix.hpp"
#include <cstring>

namespace speedtest::utils {
bool has_suffix(std::string_view str, std::string_view suffix) noexcept
{
    auto str_sz = str.size();
    auto suffix_sz = suffix.size();

    if (str_sz < suffix_sz)
        return false;

    return !std::memcmp(str.data() + (str_sz - suffix_sz), suffix.data(), suffix_sz);
}
} /* namespace speedtest::utils */
