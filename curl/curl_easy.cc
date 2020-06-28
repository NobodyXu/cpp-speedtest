#include "curl.hpp"
#include <curl/curl.h>

#include <utility>

#define CHECK(expr) check_easy((expr), #expr)

namespace curl {
static void check_easy(CURLcode code, const char *expr)
{
    if (code == CURLE_OUT_OF_MEMORY)
        throw std::bad_alloc{};
    else if (code == CURLE_BAD_FUNCTION_ARGUMENT)
        throw std::invalid_argument{expr};
    else if (code != CURLE_OK)
        throw handle_t::Exception{code};
}

Exception::Exception(const char *what_arg):
    std::runtime_error{what_arg}
{}
handle_t::Exception::Exception(int err_code_arg):
    error_code{err_code_arg}, 
    curl::Exception{curl_easy_strerror(static_cast<CURLcode>(err_code_arg))}
{}

curl_t::curl_t(FILE *debug_stream_arg):
    debug_stream{debug_stream_arg}
{
    CHECK(curl_global_init(CURL_GLOBAL_ALL));
}
curl_t::~curl_t()
{
    curl_global_cleanup();
}

handle_t::handle_t(void *p):
    curl_easy{p}
{}
handle_t::handle_t(const handle_t &other):
    curl_easy{curl_easy_duphandle(other.curl_easy)},
    download_callback{other.download_callback},
    data{other.data}
{
    if (!curl_easy)
        throw curl::Exception("curl_easy_duphandle failed");
}
handle_t curl_t::create_conn(const Url &url, const char *useragent, const char *encoding)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        throw curl::Exception("curl_easy_init failed");

    if (debug_stream) {
        curl_easy_setopt(curl, CURLOPT_STDERR, debug_stream);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }

    // Enable TCP_fastopen
    curl_easy_setopt(curl, CURLOPT_TCP_FASTOPEN, 1L);
    // Enable TCP_keepalive
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

    // Fail on HTTP 4xx errors
    CHECK(curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L));

    handle_t handle{curl};
    handle.set(url, useragent, encoding);
    return handle;
}
void handle_t::set(const Url &url, const char *useragent, const char *encoding)
{
    // Set url
    CHECK(curl_easy_setopt(curl_easy, CURLOPT_CURLU, url.url));
    // Set useragent
    CHECK(curl_easy_setopt(curl_easy, CURLOPT_USERAGENT, useragent));
    // Set encoding
    CHECK(curl_easy_setopt(curl_easy, CURLOPT_ACCEPT_ENCODING, encoding));
}

void handle_t::request_get(download_callback_t callback, void *data_arg)
{
    this->data = data_arg;
    this->download_callback = callback;

    auto write_callback = [](char *buffer, std::size_t _, std::size_t size, void *arg) noexcept {
        handle_t *handle = static_cast<handle_t*>(arg);
        return handle->download_callback(buffer, size, handle->data);
    };

    CHECK(curl_easy_setopt(curl_easy, CURLOPT_HTTPGET, 1L));
    curl_easy_setopt(curl_easy, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE);

    curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, write_callback);
}
void handle_t::request_post(const void *data, std::size_t len)
{
    CHECK(curl_easy_setopt(curl_easy, CURLOPT_POSTFIELDSIZE_LARGE, len));
    curl_easy_setopt(curl_easy, CURLOPT_POSTFIELDS, data);
}

void handle_t::perform()
{
    CHECK(curl_easy_perform(curl_easy));
}

std::size_t handle_t::getinfo_sizeof_uploaded()
{
    curl_off_t ul;
    CHECK(curl_easy_getinfo(curl_easy, CURLINFO_SIZE_UPLOAD_T, &ul));
    return ul;
}
std::size_t handle_t::getinfo_sizeof_response_header()
{
    long size;
    CHECK(curl_easy_getinfo(curl_easy, CURLINFO_HEADER_SIZE, &size));
    return size;
}
std::size_t handle_t::getinfo_sizeof_response_body()
{
    curl_off_t dl;
    CHECK(curl_easy_getinfo(curl_easy, CURLINFO_SIZE_DOWNLOAD_T, &dl));
    return dl;
}

handle_t::~handle_t()
{
    if (curl_easy)
        curl_easy_cleanup(curl_easy);
}

std::string handle_t::readall()
{
    std::string response;

    auto callback = [](char *buffer, std::size_t size, void *data) {
        std::string &response = *static_cast<std::string*>(data);
        response.append(buffer, buffer + size - 1);
        return size;
    };

    request_get(callback, &response);
    perform();

    return response;
}
void handle_t::establish_connection_only()
{
    CHECK(curl_easy_setopt(curl_easy, CURLOPT_HTTPGET, 1L));
    curl_easy_setopt(curl_easy, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE);

    curl_easy_setopt(curl_easy, CURLOPT_NOBODY, 1);
    perform();
    curl_easy_setopt(curl_easy, CURLOPT_NOBODY, 0);
}
} /* namespace curl */
