#include "curl.hpp"
#include <curl/curl.h>

#define CHECK(fcall)                     \
    ({                                   \
        CURLUcode ret = (fcall);         \
        if (ret == CURLUE_OUT_OF_MEMORY) \
            throw std::bad_alloc{};      \
        else if (ret != CURLUE_OK)       \
            throw Url::Exception{ret};   \
        ret;                             \
    })

namespace curl {
static auto curl_url_strerror(int code) noexcept -> const char*
{
    switch (code) {
        case CURLUE_BAD_HANDLE:
            return "An argument that should be a CURLU pointer was passed in as a NULL.";
        case CURLUE_BAD_PARTPOINTER:
            return "A NULL pointer was passed to the 'part' argument of curl_url_get.";
        case CURLUE_MALFORMED_INPUT:
            return "A malformed input was passed to a URL API function.";
        case CURLUE_BAD_PORT_NUMBER:
            return "The port number was not a decimal number between 0 and 65535.";
        case CURLUE_UNSUPPORTED_SCHEME:
            return "This libcurl build doesn't support the given URL scheme.";
        case CURLUE_URLDECODE:
            return "URL decode error, most likely because of rubbish in the input.";
        case CURLUE_OUT_OF_MEMORY:
            return "SHOULD THROW std::bad_alloc instead: A memory function failed.";
        case CURLUE_USER_NOT_ALLOWED:
            return "Credentials was passed in the URL when prohibited.";
        case CURLUE_UNKNOWN_PART:
            return "An unknown part ID was passed to a URL API function.";
        case CURLUE_NO_SCHEME:
            return "There is no scheme part in the URL.";
        case CURLUE_NO_USER:
            return "There is no user part in the URL.";
        case CURLUE_NO_PASSWORD:
            return "There is no password part in the URL.";
        case CURLUE_NO_OPTIONS:
            return "There is no options part in the URL.";
        case CURLUE_NO_HOST:
            return "There is no host part in the URL.";
        case CURLUE_NO_PORT:
            return "There is no port part in the URL.";
        case CURLUE_NO_QUERY:
            return "There is no query part in the URL.";
        case CURLUE_NO_FRAGMENT:
            return "There is no fragment part in the URL.";

        default:
            return "Unknown/unsupported error code.";
    }
}
Url::Exception::Exception(int err_code_arg):
    error_code{err_code_arg},
    curl::Exception{curl_url_strerror(err_code_arg)}
{}

Url::Url():
    url{curl_url()}
{
    if (url == nullptr)
        throw std::bad_alloc{};
}
Url::Url(const char *url_arg):
    Url{}
{
    set_url(url_arg);
}
Url::Url(const Url &other):
    url{curl_url_dup(static_cast<CURLU*>(other.url))}
{
    if (url == nullptr)
        throw std::bad_alloc{};
}
Url::Url(Url &&other) noexcept:
    url{other.url}
{
    other.url = nullptr;
}

Url& Url::operator = (const Url &other)
{
    url = curl_url_dup(static_cast<CURLU*>(other.url));
    if (url == nullptr)
        throw std::bad_alloc{};
    return *this;
}
Url& Url::operator = (Url &&other) noexcept
{
    if (url)
        curl_url_cleanup(static_cast<CURLU*>(url));
    url = other.url;
    other.url = nullptr;
    return *this;
}

void Url::set_url(const char *url_arg)
{
    CHECK(curl_url_set(static_cast<CURLU*>(url), CURLUPART_URL, url_arg, 0));
}
void Url::set_scheme(const char *scheme)
{
    CHECK(curl_url_set(static_cast<CURLU*>(url), CURLUPART_SCHEME, scheme, 0));
}
void Url::set_options(const char *options)
{
    CHECK(curl_url_set(static_cast<CURLU*>(url), CURLUPART_OPTIONS, options, 0));
}

auto Url::get_url() -> fullurl_str
{
    char *fullurl;
    CHECK(curl_url_get(static_cast<CURLU*>(url), CURLUPART_URL, &fullurl, 0));
    return fullurl_str(fullurl, &curl_free);
}

Url::~Url()
{
    if (url)
        curl_url_cleanup(static_cast<CURLU*>(url));
}
} /* namespace curl */
