#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <unordered_map>
#include <functional>
#include <future>

namespace lithium::network {

// ============================================================================
// HTTP Types
// ============================================================================

enum class HttpMethod {
    Get,
    Post,
    Put,
    Delete,
    Head,
    Options,
    Patch
};

[[nodiscard]] String http_method_to_string(HttpMethod method);

// ============================================================================
// HTTP Headers
// ============================================================================

class HttpHeaders {
public:
    void set(const String& name, const String& value);
    void add(const String& name, const String& value);
    void remove(const String& name);

    [[nodiscard]] std::optional<String> get(const String& name) const;
    [[nodiscard]] std::vector<String> get_all(const String& name) const;
    [[nodiscard]] bool has(const String& name) const;

    [[nodiscard]] auto begin() const { return m_headers.begin(); }
    [[nodiscard]] auto end() const { return m_headers.end(); }

private:
    std::unordered_multimap<String, String> m_headers;
};

// ============================================================================
// HTTP Request
// ============================================================================

struct HttpRequest {
    HttpMethod method{HttpMethod::Get};
    String url;
    HttpHeaders headers;
    std::vector<u8> body;

    // Timeout in milliseconds (0 = no timeout)
    u32 timeout_ms{30000};

    // Follow redirects
    bool follow_redirects{true};
    u32 max_redirects{10};
};

// ============================================================================
// HTTP Response
// ============================================================================

struct HttpResponse {
    i32 status_code{0};
    String status_text;
    HttpHeaders headers;
    std::vector<u8> body;

    // Response URL (may differ from request URL after redirects)
    String url;

    // Timing
    f64 time_ms{0};

    [[nodiscard]] bool is_success() const { return status_code >= 200 && status_code < 300; }
    [[nodiscard]] bool is_redirect() const { return status_code >= 300 && status_code < 400; }
    [[nodiscard]] bool is_client_error() const { return status_code >= 400 && status_code < 500; }
    [[nodiscard]] bool is_server_error() const { return status_code >= 500; }

    [[nodiscard]] String body_as_string() const;
};

// ============================================================================
// HTTP Client
// ============================================================================

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Synchronous requests
    [[nodiscard]] Result<HttpResponse, String> send(const HttpRequest& request);

    // Convenience methods
    [[nodiscard]] Result<HttpResponse, String> get(const String& url);
    [[nodiscard]] Result<HttpResponse, String> post(const String& url, const std::vector<u8>& body);
    [[nodiscard]] Result<HttpResponse, String> post_json(const String& url, const String& json);

    // Asynchronous requests
    using ResponseCallback = std::function<void(Result<HttpResponse, String>)>;
    void send_async(const HttpRequest& request, ResponseCallback callback);
    [[nodiscard]] std::future<Result<HttpResponse, String>> send_async(const HttpRequest& request);

    // Configuration
    void set_default_headers(const HttpHeaders& headers);
    void set_user_agent(const String& user_agent);
    void set_timeout(u32 timeout_ms);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace lithium::network
