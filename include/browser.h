#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  browser.h — Real internet browser backend for VidyaOS
//  Uses libcurl to fetch real HTTP/HTTPS pages and strips HTML to text.
// ─────────────────────────────────────────────────────────────────────────────
#include <string>
#include <vector>
#include <functional>

// ── Page link extracted from HTML ────────────────────────────────────────────
struct ParsedLink {
    std::string href;
    std::string text;
};

// ── Result of a fetch ────────────────────────────────────────────────────────
struct FetchResult {
    bool        success;
    int         http_code;       // 200, 404, etc.
    std::string final_url;       // After redirects
    std::string content_type;
    std::string raw_body;        // Full HTML
    std::string plain_text;      // Stripped text for display
    std::vector<std::string>     lines;  // pre-wrapped display lines
    std::vector<ParsedLink>      links;  // extracted <a href> links
    std::string error_message;
};

// ── Public API ───────────────────────────────────────────────────────────────

// Asynchronous fetch — triggers a background thread.
// Call is_fetching() to poll completion, then get_fetch_result().
void browser_fetch_async(const std::string& url);

// Returns true while a fetch is in progress.
bool browser_is_fetching();

// Returns the last completed fetch result. Safe to call from render thread.
const FetchResult& browser_get_result();

// Returns a normalised URL from user input (adds https:// etc.)
std::string browser_normalise_url(const std::string& input);

// Strip HTML tags and decode entities, return plain text lines.
std::vector<std::string> html_to_lines(const std::string& html, int wrap_chars, std::vector<ParsedLink>& links_out);
