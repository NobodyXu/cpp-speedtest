#ifndef  __cpp_speedtest_curl_curl_HPP__
# define __cpp_speedtest_curl_curl_HPP__

# include <cstddef>
# include <cstdio>
# include <stdexcept>
# include <memory>
# include <string>

namespace curl {
class Exception: public std::runtime_error {
public:
    Exception(const char *what_arg);
    Exception(const Exception&) = default;
};

class handle_t;
class Url;

/**
 * Must be defined as a static global variable in main
 */
class curl_t {
    FILE *debug_stream;

public:
    curl_t(FILE *debug_stream_arg);
    ~curl_t();

    /**
     * @param encoding "" for enable all, NULL for disable all (including auto decompression).
     * @param url must exist until perform() returns.
     * @exception curl::Exception and curl::handle_t::Exception.
     */
    handle_t create_conn(const Url &url, const char *useragent = NULL, const char *encoding = "");
};

class handle_t {
public:
    /**
     * If return value is less than @param size, then it will singal an err cond to libcurl.
     * This will cause the transfer to get aborted and the libcurl function used will return CURLE_WRITE_ERROR.
	 * 
     * @param buffer not null-terminated
     */
    using download_callback_t = std::size_t (*)(char *buffer, std::size_t size, void *data);

private:
    void *curl_easy;

    download_callback_t download_callback;
    void *data;

protected:
    handle_t(void *p);

public:
    friend handle_t curl_t::create_conn(const Url &url, const char *useragent, const char *encoding);

    class Exception: public curl::Exception {
    public:
        const int error_code;

        Exception(int err_code_arg);
        Exception(const Exception&) = default;
    };

    /**
     * After calling copy constructor, you must call request_* again before calling perform.
     */
    handle_t(const handle_t&);
    handle_t(handle_t &&other) = delete;

    handle_t& operator = (const handle_t&) = delete;
    handle_t& operator = (handle_t&&) = delete;

    /**
     * @param url must exist until perform() returns.
     */
    void set_url(const Url &url);

    void request_get(download_callback_t callback, void *data);
    /**
     * The data pointed to is NOT copied by the library: as a consequence, it must be preserved by 
     * the calling application until the associated transfer finishes. 
     */
    void request_post(const void *data, std::size_t len);

    void perform();

    std::size_t getinfo_sizeof_uploaded();
    std::size_t getinfo_sizeof_response_header();
    std::size_t getinfo_sizeof_response_body();

    ~handle_t();
};

class Url {
    void *url;

public:
    friend void handle_t::set_url(const Url &url);

    class Exception: public curl::Exception {
    public:
        const int error_code;

        Exception(int err_code_arg);
        Exception(const Exception&) = default;
    };

    Url();
    Url(const char *url_arg);
    Url(const Url&);
    /**
     * After other is moved, it can only be destroyed or reassigned.
     */
    Url(Url &&other) noexcept;

    Url& operator = (const Url&);
    /**
     * After other is moved, it can only be destroyed or reassigned.
     */
    Url& operator = (Url &&other) noexcept;

    void set_url(const char *url_arg);
    void set_scheme(const char *scheme);
    void set_options(const char *options);

    using fullurl_str = std::unique_ptr<char, void (*)(void*)>;
    auto get_url() -> fullurl_str;

    ~Url();
};

} /* namespace curl */

#endif
