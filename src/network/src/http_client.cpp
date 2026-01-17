/**
 * HTTP Client implementation
 */

#include "lithium/network/http_client.hpp"
#include <thread>

// libcurl (optional - compile without if not available)
#ifdef LITHIUM_HAS_CURL
#include <curl/curl.h>
#endif

namespace lithium::network {

// ============================================================================
// HTTP Method conversion
// ============================================================================

String http_method_to_string(HttpMethod method) {
    switch (method) {
        case HttpMethod::Get: return "GET"_s;
        case HttpMethod::Post: return "POST"_s;
        case HttpMethod::Put: return "PUT"_s;
        case HttpMethod::Delete: return "DELETE"_s;
        case HttpMethod::Head: return "HEAD"_s;
        case HttpMethod::Options: return "OPTIONS"_s;
        case HttpMethod::Patch: return "PATCH"_s;
    }
    return "GET"_s;
}

// ============================================================================
// HttpHeaders implementation
// ============================================================================

void HttpHeaders::set(const String& name, const String& value) {
    // Remove existing headers with this name
    auto range = m_headers.equal_range(name);
    m_headers.erase(range.first, range.second);
    m_headers.insert({name, value});
}

void HttpHeaders::add(const String& name, const String& value) {
    m_headers.insert({name, value});
}

void HttpHeaders::remove(const String& name) {
    auto range = m_headers.equal_range(name);
    m_headers.erase(range.first, range.second);
}

std::optional<String> HttpHeaders::get(const String& name) const {
    auto it = m_headers.find(name);
    if (it != m_headers.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<String> HttpHeaders::get_all(const String& name) const {
    std::vector<String> values;
    auto range = m_headers.equal_range(name);
    for (auto it = range.first; it != range.second; ++it) {
        values.push_back(it->second);
    }
    return values;
}

bool HttpHeaders::has(const String& name) const {
    return m_headers.count(name) > 0;
}

// ============================================================================
// HttpResponse implementation
// ============================================================================

String HttpResponse::body_as_string() const {
    return String(reinterpret_cast<const char*>(body.data()), body.size());
}

// ============================================================================
// HttpClient implementation
// ============================================================================

#ifdef LITHIUM_HAS_CURL

struct HttpClient::Impl {
    CURL* curl{nullptr};
    HttpHeaders default_headers;
    String user_agent{"Lithium/1.0"};
    u32 timeout_ms{30000};

    Impl() {
        curl = curl_easy_init();
    }

    ~Impl() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }
};

static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buffer = static_cast<std::vector<u8>*>(userdata);
    size_t total = size * nmemb;
    buffer->insert(buffer->end(), ptr, ptr + total);
    return total;
}

static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    auto* response = static_cast<HttpResponse*>(userdata);
    size_t total = size * nitems;

    String header(buffer, total);
    usize colon = header.find(':');
    if (colon != String::npos) {
        String name = header.substr(0, colon);
        String value = header.substr(colon + 1);
        // Trim whitespace
        while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
            value = value.substr(1);
        }
        while (!value.empty() && (value[value.size()-1] == '\r' || value[value.size()-1] == '\n')) {
            value = value.substr(0, value.size() - 1);
        }
        response->headers.add(name, value);
    }

    return total;
}

HttpClient::HttpClient() : m_impl(std::make_unique<Impl>()) {}

HttpClient::~HttpClient() = default;

Result<HttpResponse, String> HttpClient::send(const HttpRequest& request) {
    if (!m_impl->curl) {
        return Result<HttpResponse, String>::error("CURL not initialized"_s);
    }

    CURL* curl = m_impl->curl;
    HttpResponse response;
    response.url = request.url;

    // Reset curl state
    curl_easy_reset(curl);

    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, request.url.data());

    // Set method
    switch (request.method) {
        case HttpMethod::Get:
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
            break;
        case HttpMethod::Post:
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            break;
        case HttpMethod::Put:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            break;
        case HttpMethod::Delete:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;
        case HttpMethod::Head:
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
            break;
        case HttpMethod::Options:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "OPTIONS");
            break;
        case HttpMethod::Patch:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
            break;
    }

    // Set headers
    struct curl_slist* headers = nullptr;
    for (const auto& [name, value] : m_impl->default_headers) {
        std::string header = std::string(name.data()) + ": " + std::string(value.data());
        headers = curl_slist_append(headers, header.c_str());
    }
    for (const auto& [name, value] : request.headers) {
        std::string header = std::string(name.data()) + ": " + std::string(value.data());
        headers = curl_slist_append(headers, header.c_str());
    }
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    // Set body
    if (!request.body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request.body.size());
    }

    // Set callbacks
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);

    // Set timeout
    u32 timeout = request.timeout_ms > 0 ? request.timeout_ms : m_impl->timeout_ms;
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout));

    // Set user agent
    curl_easy_setopt(curl, CURLOPT_USERAGENT, m_impl->user_agent.data());

    // Follow redirects
    if (request.follow_redirects) {
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, static_cast<long>(request.max_redirects));
    }

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    // Clean up headers
    if (headers) {
        curl_slist_free_all(headers);
    }

    if (res != CURLE_OK) {
        return Result<HttpResponse, String>::error(String(curl_easy_strerror(res)));
    }

    // Get response info
    long status_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    response.status_code = static_cast<i32>(status_code);

    double total_time;
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);
    response.time_ms = total_time * 1000.0;

    char* effective_url;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
    if (effective_url) {
        response.url = String(effective_url);
    }

    return Result<HttpResponse, String>::ok(std::move(response));
}

#else // No CURL

struct HttpClient::Impl {
    HttpHeaders default_headers;
    String user_agent{"Lithium/1.0"};
    u32 timeout_ms{30000};
};

HttpClient::HttpClient() : m_impl(std::make_unique<Impl>()) {}

HttpClient::~HttpClient() = default;

Result<HttpResponse, String> HttpClient::send(const HttpRequest& request) {
    // Stub implementation without CURL
    return make_error("HTTP client not available (built without CURL)"_s);
}

#endif // LITHIUM_HAS_CURL

Result<HttpResponse, String> HttpClient::get(const String& url) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = url;
    return send(request);
}

Result<HttpResponse, String> HttpClient::post(const String& url, const std::vector<u8>& body) {
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = url;
    request.body = body;
    return send(request);
}

Result<HttpResponse, String> HttpClient::post_json(const String& url, const String& json) {
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = url;
    request.headers.set("Content-Type"_s, "application/json"_s);
    request.body.assign(json.begin(), json.end());
    return send(request);
}

void HttpClient::send_async(const HttpRequest& request, ResponseCallback callback) {
    std::thread([this, request, callback = std::move(callback)]() {
        auto result = send(request);
        callback(std::move(result));
    }).detach();
}

std::future<Result<HttpResponse, String>> HttpClient::send_async(const HttpRequest& request) {
    return std::async(std::launch::async, [this, request]() {
        return send(request);
    });
}

void HttpClient::set_default_headers(const HttpHeaders& headers) {
    m_impl->default_headers = headers;
}

void HttpClient::set_user_agent(const String& user_agent) {
    m_impl->user_agent = user_agent;
}

void HttpClient::set_timeout(u32 timeout_ms) {
    m_impl->timeout_ms = timeout_ms;
}

} // namespace lithium::network
