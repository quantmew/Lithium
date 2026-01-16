# Network module - HTTP client and resource loading

lithium_add_module(network
    SOURCES
        src/http_client.cpp
        src/resource_loader.cpp
    HEADERS
        include/lithium/network/http_client.hpp
        include/lithium/network/resource_loader.hpp
    PUBLIC_DEPENDENCIES
        lithium_core
)

# Optional CURL integration
if(CURL_FOUND)
    target_link_libraries(lithium_network PRIVATE CURL::libcurl)
    target_compile_definitions(lithium_network PRIVATE LITHIUM_USE_CURL)
endif()
