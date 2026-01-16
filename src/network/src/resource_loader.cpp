/**
 * Resource Loader implementation
 */

#include "lithium/network/resource_loader.hpp"
#include <algorithm>

namespace lithium::network {

// ============================================================================
// Resource implementation
// ============================================================================

String Resource::data_as_string() const {
    return String(reinterpret_cast<const char*>(data.data()), data.size());
}

// ============================================================================
// ResourceLoader implementation
// ============================================================================

ResourceLoader::ResourceLoader() = default;

ResourceLoader::~ResourceLoader() = default;

Result<Resource, String> ResourceLoader::load(const String& url, ResourceType type) {
    // Resolve relative URL
    String resolved_url = resolve_url(url);

    // Check cache first
    if (m_cache_enabled) {
        auto cached = get_cached(resolved_url);
        if (cached) {
            return Result<Resource, String>::ok(std::move(*cached));
        }
    }

    // Load from network
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolved_url;

    // Set appropriate Accept header based on resource type
    switch (type) {
        case ResourceType::Document:
            request.headers.set("Accept"_s, "text/html,application/xhtml+xml"_s);
            break;
        case ResourceType::Stylesheet:
            request.headers.set("Accept"_s, "text/css"_s);
            break;
        case ResourceType::Script:
            request.headers.set("Accept"_s, "application/javascript,text/javascript"_s);
            break;
        case ResourceType::Image:
            request.headers.set("Accept"_s, "image/*"_s);
            break;
        case ResourceType::Font:
            request.headers.set("Accept"_s, "font/*,application/font-woff"_s);
            break;
        default:
            request.headers.set("Accept"_s, "*/*"_s);
            break;
    }

    ++m_pending_count;
    auto result = m_client.send(request);
    --m_pending_count;

    if (!result.is_ok()) {
        return Result<Resource, String>::error(result.error());
    }

    auto& response = result.value();

    if (!response.is_success()) {
        return Result<Resource, String>::error(
            "HTTP error: "_s + String(std::to_string(response.status_code)));
    }

    // Create resource
    Resource resource;
    resource.url = resolved_url;
    resource.type = type;
    resource.mime_type = detect_mime_type(resolved_url, response);
    resource.data = std::move(response.body);
    resource.load_time_ms = response.time_ms;

    // Extract charset from Content-Type
    if (auto content_type = response.headers.get("Content-Type"_s)) {
        auto pos = content_type->find("charset="_s);
        if (pos != String::npos) {
            resource.charset = content_type->substr(pos + 8);
        }
    }

    // Extract cache headers
    if (auto etag = response.headers.get("ETag"_s)) {
        resource.etag = *etag;
    }

    // Cache the resource
    if (m_cache_enabled) {
        cache_resource(resource);
    }

    return Result<Resource, String>::ok(std::move(resource));
}

void ResourceLoader::load_async(const String& url, ResourceType type, LoadCallback callback) {
    std::thread([this, url, type, callback = std::move(callback)]() {
        auto result = load(url, type);
        callback(std::move(result));
    }).detach();
}

void ResourceLoader::set_base_url(const String& base_url) {
    m_base_url = base_url;
}

String ResourceLoader::resolve_url(const String& url) const {
    // Already absolute URL
    if (url.find("://"_s) != String::npos) {
        return url;
    }

    if (m_base_url.empty()) {
        return url;
    }

    // Parse base URL
    auto base = parse_url(m_base_url);
    if (!base) {
        return url;
    }

    // Handle protocol-relative URLs
    if (url.size() >= 2 && url[0] == '/' && url[1] == '/') {
        return base->scheme + ":" + url;
    }

    // Handle absolute path
    if (!url.empty() && url[0] == '/') {
        base->path = url;
        base->query = ""_s;
        base->fragment = ""_s;
        return build_url(*base);
    }

    // Handle relative path
    usize last_slash = base->path.rfind('/');
    if (last_slash != String::npos) {
        base->path = base->path.substr(0, last_slash + 1) + url;
    } else {
        base->path = "/" + url;
    }

    // Normalize path (handle . and ..)
    // (simplified - full implementation would handle all edge cases)

    return build_url(*base);
}

void ResourceLoader::clear_cache() {
    m_cache.clear();
    m_current_cache_size = 0;
}

std::optional<Resource> ResourceLoader::get_cached(const String& url) const {
    auto it = m_cache.find(url);
    if (it != m_cache.end()) {
        Resource cached = it->second;
        cached.from_cache = true;
        return cached;
    }
    return std::nullopt;
}

std::optional<ResourceLoader::ParsedUrl> ResourceLoader::parse_url(const String& url) {
    ParsedUrl result;

    // Find scheme
    usize scheme_end = url.find("://"_s);
    if (scheme_end == String::npos) {
        return std::nullopt;
    }
    result.scheme = url.substr(0, scheme_end);

    // Find host and path
    usize host_start = scheme_end + 3;
    usize path_start = url.find('/', host_start);
    if (path_start == String::npos) {
        result.host = url.substr(host_start);
        result.path = "/"_s;
    } else {
        result.host = url.substr(host_start, path_start - host_start);
        result.path = url.substr(path_start);
    }

    // Extract port from host
    usize port_pos = result.host.find(':');
    if (port_pos != String::npos) {
        String port_str = result.host.substr(port_pos + 1);
        result.port = static_cast<u16>(std::stoi(std::string(port_str.data(), port_str.size())));
        result.host = result.host.substr(0, port_pos);
    } else {
        result.port = (result.scheme == "https"_s) ? 443 : 80;
    }

    // Extract query and fragment
    usize query_pos = result.path.find('?');
    if (query_pos != String::npos) {
        usize frag_pos = result.path.find('#', query_pos);
        if (frag_pos != String::npos) {
            result.fragment = result.path.substr(frag_pos + 1);
            result.query = result.path.substr(query_pos + 1, frag_pos - query_pos - 1);
        } else {
            result.query = result.path.substr(query_pos + 1);
        }
        result.path = result.path.substr(0, query_pos);
    } else {
        usize frag_pos = result.path.find('#');
        if (frag_pos != String::npos) {
            result.fragment = result.path.substr(frag_pos + 1);
            result.path = result.path.substr(0, frag_pos);
        }
    }

    return result;
}

String ResourceLoader::build_url(const ParsedUrl& parsed) {
    String url = parsed.scheme + "://" + parsed.host;

    bool default_port = (parsed.scheme == "http"_s && parsed.port == 80) ||
                       (parsed.scheme == "https"_s && parsed.port == 443);
    if (!default_port && parsed.port != 0) {
        url += ":" + String(std::to_string(parsed.port));
    }

    url += parsed.path;

    if (!parsed.query.empty()) {
        url += "?" + parsed.query;
    }

    if (!parsed.fragment.empty()) {
        url += "#" + parsed.fragment;
    }

    return url;
}

String ResourceLoader::detect_mime_type(const String& url, const HttpResponse& response) {
    // Check Content-Type header first
    if (auto content_type = response.headers.get("Content-Type"_s)) {
        // Extract MIME type (before charset)
        usize semicolon = content_type->find(';');
        if (semicolon != String::npos) {
            return content_type->substr(0, semicolon);
        }
        return *content_type;
    }

    // Guess from URL extension
    usize dot = url.rfind('.');
    if (dot != String::npos) {
        String ext = url.substr(dot + 1);

        if (ext == "html"_s || ext == "htm"_s) return "text/html"_s;
        if (ext == "css"_s) return "text/css"_s;
        if (ext == "js"_s) return "application/javascript"_s;
        if (ext == "json"_s) return "application/json"_s;
        if (ext == "png"_s) return "image/png"_s;
        if (ext == "jpg"_s || ext == "jpeg"_s) return "image/jpeg"_s;
        if (ext == "gif"_s) return "image/gif"_s;
        if (ext == "svg"_s) return "image/svg+xml"_s;
        if (ext == "woff"_s) return "font/woff"_s;
        if (ext == "woff2"_s) return "font/woff2"_s;
        if (ext == "ttf"_s) return "font/ttf"_s;
    }

    return "application/octet-stream"_s;
}

ResourceType ResourceLoader::mime_to_resource_type(const String& mime_type) {
    if (mime_type.find("text/html"_s) != String::npos) {
        return ResourceType::Document;
    }
    if (mime_type.find("text/css"_s) != String::npos) {
        return ResourceType::Stylesheet;
    }
    if (mime_type.find("javascript"_s) != String::npos) {
        return ResourceType::Script;
    }
    if (mime_type.find("image/"_s) != String::npos) {
        return ResourceType::Image;
    }
    if (mime_type.find("font/"_s) != String::npos) {
        return ResourceType::Font;
    }
    if (mime_type.find("video/"_s) != String::npos ||
        mime_type.find("audio/"_s) != String::npos) {
        return ResourceType::Media;
    }
    return ResourceType::Other;
}

void ResourceLoader::cache_resource(const Resource& resource) {
    evict_cache_if_needed();

    m_cache[resource.url] = resource;
    m_current_cache_size += resource.data.size();
}

void ResourceLoader::evict_cache_if_needed() {
    while (m_current_cache_size > m_max_cache_size && !m_cache.empty()) {
        // Simple LRU: just remove first item
        // (Full implementation would track access times)
        auto it = m_cache.begin();
        m_current_cache_size -= it->second.data.size();
        m_cache.erase(it);
    }
}

} // namespace lithium::network
