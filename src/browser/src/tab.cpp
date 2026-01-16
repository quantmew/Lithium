/**
 * Browser Tab implementation
 */

#include "lithium/browser/engine.hpp"
#include "lithium/dom/document.hpp"

namespace lithium::browser {

// ============================================================================
// Tab class
// ============================================================================

class Tab {
public:
    Tab() = default;

    // Tab ID
    [[nodiscard]] u32 id() const { return m_id; }

    // Title
    [[nodiscard]] const String& title() const { return m_title; }
    void set_title(const String& title) {
        m_title = title;
        if (m_on_title_changed) {
            m_on_title_changed(title);
        }
    }

    // URL
    [[nodiscard]] const String& url() const { return m_url; }

    // Loading state
    [[nodiscard]] bool is_loading() const { return m_is_loading; }

    // Load a URL
    void load(const String& url) {
        m_url = url;
        m_is_loading = true;

        if (m_on_load_started) {
            m_on_load_started(url);
        }

        // Actual loading would be done by the engine
    }

    // Stop loading
    void stop() {
        m_is_loading = false;
    }

    // Reload
    void reload() {
        load(m_url);
    }

    // Navigation
    void go_back() {
        if (can_go_back()) {
            --m_history_index;
            load(m_history[m_history_index]);
        }
    }

    void go_forward() {
        if (can_go_forward()) {
            ++m_history_index;
            load(m_history[m_history_index]);
        }
    }

    [[nodiscard]] bool can_go_back() const {
        return m_history_index > 0;
    }

    [[nodiscard]] bool can_go_forward() const {
        return m_history_index < static_cast<i32>(m_history.size()) - 1;
    }

    // Document access
    [[nodiscard]] dom::Document* document() const { return m_document.get(); }

    // Callbacks
    using TitleChangedCallback = std::function<void(const String&)>;
    using LoadStartedCallback = std::function<void(const String&)>;
    using LoadFinishedCallback = std::function<void(const String&, bool)>;

    void set_title_changed_callback(TitleChangedCallback cb) { m_on_title_changed = std::move(cb); }
    void set_load_started_callback(LoadStartedCallback cb) { m_on_load_started = std::move(cb); }
    void set_load_finished_callback(LoadFinishedCallback cb) { m_on_load_finished = std::move(cb); }

private:
    static u32 s_next_id;

    u32 m_id{s_next_id++};
    String m_title{"New Tab"};
    String m_url{"about:blank"};
    bool m_is_loading{false};

    RefPtr<dom::Document> m_document;

    std::vector<String> m_history;
    i32 m_history_index{-1};

    TitleChangedCallback m_on_title_changed;
    LoadStartedCallback m_on_load_started;
    LoadFinishedCallback m_on_load_finished;
};

u32 Tab::s_next_id = 1;

// ============================================================================
// TabManager class
// ============================================================================

class TabManager {
public:
    TabManager() = default;

    // Create new tab
    Tab* create_tab() {
        m_tabs.push_back(std::make_unique<Tab>());
        return m_tabs.back().get();
    }

    // Close tab
    void close_tab(u32 id) {
        m_tabs.erase(
            std::remove_if(m_tabs.begin(), m_tabs.end(),
                [id](const auto& tab) { return tab->id() == id; }),
            m_tabs.end());
    }

    // Get tab by ID
    Tab* get_tab(u32 id) {
        for (auto& tab : m_tabs) {
            if (tab->id() == id) {
                return tab.get();
            }
        }
        return nullptr;
    }

    // Get active tab
    Tab* active_tab() {
        if (m_active_tab_id == 0 && !m_tabs.empty()) {
            m_active_tab_id = m_tabs.front()->id();
        }
        return get_tab(m_active_tab_id);
    }

    // Set active tab
    void set_active_tab(u32 id) {
        m_active_tab_id = id;
    }

    // Get all tabs
    [[nodiscard]] const std::vector<std::unique_ptr<Tab>>& tabs() const {
        return m_tabs;
    }

    // Tab count
    [[nodiscard]] usize count() const {
        return m_tabs.size();
    }

private:
    std::vector<std::unique_ptr<Tab>> m_tabs;
    u32 m_active_tab_id{0};
};

} // namespace lithium::browser
