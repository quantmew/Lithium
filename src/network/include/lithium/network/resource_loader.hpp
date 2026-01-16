#pragma once

#include "http_client.hpp"
#include <memory>
#include <unordered_map>

namespace lithium::network {

// ============================================================================
// Resource Types
// ============================================================================

enum class ResourceType {
    Document,
    Stylesheet,
    Script,
    Image,
    Font,
    Media,
    Other
};

// ============================================================================
// Loaded Resource
// ============================================================================

struct Resource {
    String url;
    ResourceType type;
    String mime_type;
    String charset;
    std::vector<u8> data;

    // Timing
    f64 load_time_ms{0};

    // Cache info
    bool from_cache{false};
    std::optional<String> etag;
    std::optional<i64> max_age;

    [[nodiscard]] String data_as_string() const;
};

// ============================================================================
// Resource Loader
// ============================================================================

class ResourceLoader {
public:
    ResourceLoader();
    ~ResourceLoader();

    // Load a resource
    [[nodiscard]] Result<Resource, String> load(const String& url, ResourceType type = ResourceType::Other);

    // Async loading
    using LoadCallback = std::function<void(Result<Resource, String>)>;
    void load_async(const String& url, ResourceType type, LoadCallback callback);

    // Base URL for relative URLs
    void set_base_url(const String& base_url);
    [[nodiscard]] const String& base_url() const { return m_base_url; }

    // Resolve relative URL
    [[nodiscard]] String resolve_url(const String& url) const;

    // Cache control
    void enable_cache(bool enabled) { m_cache_enabled = enabled; }
    void set_max_cache_size(usize bytes) { m_max_cache_size = bytes; }
    void clear_cache();

    // Get cached resource (if available)
    [[nodiscard]] std::optional<Resource> get_cached(const String& url) const;

    // Pending request count
    [[nodiscard]] usize pending_count() const { return m_pending_count; }

private:
    // URL parsing
    struct ParsedUrl {
        String scheme;
        String host;
        u16 port{0};
        String path;
        String query;
        String fragment;
    };

    [[nodiscard]] static std::optional<ParsedUrl> parse_url(const String& url);
    [[nodiscard]] static String build_url(const ParsedUrl& parsed);

    // MIME type detection
    [[nodiscard]] static String detect_mime_type(const String& url, const HttpResponse& response);
    [[nodiscard]] static ResourceType mime_to_resource_type(const String& mime_type);

    // Cache
    void cache_resource(const Resource& resource);
    void evict_cache_if_needed();

    HttpClient m_client;
    String m_base_url;

    // Cache
    bool m_cache_enabled{true};
    usize m_max_cache_size{50 * 1024 * 1024};  // 50MB default
    usize m_current_cache_size{0};
    std::unordered_map<String, Resource> m_cache;

    // Pending requests
    usize m_pending_count{0};
};

} // namespace lithium::network
