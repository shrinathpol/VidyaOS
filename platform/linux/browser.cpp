#include "../../include/browser.h"
#include <curl/curl.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <cctype>
#include <string>

// ── Global state ─────────────────────────────────────────────────────────────
static std::atomic<bool> g_is_fetching{false};
static std::mutex g_result_mutex;
static FetchResult g_last_result;

// ── cURL Write Callback ──────────────────────────────────────────────────────
static size_t curl_write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    std::string* mem = (std::string*)userp;
    mem->append((char*)contents, realsize);
    return realsize;
}

// ── Background Fetch Thread ──────────────────────────────────────────────────
static void fetch_thread_func(std::string url) {
    FetchResult res;
    res.success = false;
    res.http_code = 0;

    CURL* curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res.raw_body);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "VidyaOS/1.0");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        CURLcode res_code = curl_easy_perform(curl);
        if (res_code == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            res.http_code = (int)http_code;
            
            char* ct = nullptr;
            curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
            if (ct) res.content_type = ct;

            char* final_url = nullptr;
            curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &final_url);
            if (final_url) res.final_url = final_url;

            res.success = true;
        } else {
            res.error_message = curl_easy_strerror(res_code);
        }
        curl_easy_cleanup(curl);
    } else {
        res.error_message = "Failed to initialize cURL";
    }

    if (res.success) {
        // Strip HTML if it's text/html
        if (res.content_type.find("text/html") != std::string::npos || res.content_type.empty()) {
            res.lines = html_to_lines(res.raw_body, 80, res.links);
            for (const auto& line : res.lines) {
                res.plain_text += line + "\n";
            }
        } else {
            res.plain_text = "Cannot render non-HTML content (" + res.content_type + ")\nSize: " + std::to_string(res.raw_body.size()) + " bytes.";
            res.lines.push_back(res.plain_text);
        }
    }

    std::lock_guard<std::mutex> lock(g_result_mutex);
    g_last_result = res;
    g_is_fetching = false;
}

void browser_fetch_async(const std::string& url) {
    if (g_is_fetching) return;
    
    std::lock_guard<std::mutex> lock(g_result_mutex);
    g_last_result = FetchResult(); // clear previous
    g_is_fetching = true;
    
    std::thread t(fetch_thread_func, url);
    t.detach();
}

bool browser_is_fetching() {
    return g_is_fetching;
}

const FetchResult& browser_get_result() {
    std::lock_guard<std::mutex> lock(g_result_mutex);
    return g_last_result;
}

std::string browser_normalise_url(const std::string& input) {
    if (input.empty()) return input;
    if (input.find("vidyaos://") == 0) return input;
    if (input.find("file://") == 0) return input;
    if (input.find("http://") != 0 && input.find("https://") != 0) {
        return "https://" + input;
    }
    return input;
}

// ── Basic HTML to Plain Text + Link Extractor ──────────────────────────────
std::vector<std::string> html_to_lines(const std::string& html, int wrap_chars, std::vector<ParsedLink>& links_out) {
    std::vector<std::string> lines;
    std::string current_line;
    bool in_tag = false;
    bool in_script = false;
    bool in_style = false;
    std::string current_tag = "";
    std::string tag_content = ""; // Used to accumulate attributes

    for (size_t i = 0; i < html.size(); ++i) {
        char c = html[i];
        if (c == '<') {
            in_tag = true;
            current_tag = "";
            tag_content = "";
        } else if (c == '>') {
            in_tag = false;
            std::string tag_name = current_tag;
            size_t space = tag_name.find(' ');
            if (space != std::string::npos) {
                tag_name = tag_name.substr(0, space);
            }
            // lower it
            for (char& tc : tag_name) tc = std::tolower(tc);
            
            if (tag_name == "script") in_script = true;
            else if (tag_name == "/script") in_script = false;
            else if (tag_name == "style") in_style = true;
            else if (tag_name == "/style") in_style = false;
            else if (tag_name == "br" || tag_name == "p" || tag_name == "/p" || tag_name == "div" || tag_name == "/div" || tag_name == "li" || tag_name == "h1" || tag_name == "h2" || tag_name == "h3" || tag_name == "h4") {
                if (!current_line.empty()) {
                    lines.push_back(current_line);
                    current_line.clear();
                }
            } else if (tag_name == "a") {
                 // extract href
                 size_t hpos = tag_content.find("href=\"");
                 if (hpos != std::string::npos) {
                     size_t qend = tag_content.find("\"", hpos + 6);
                     if (qend != std::string::npos) {
                         ParsedLink pl;
                         pl.href = tag_content.substr(hpos + 6, qend - (hpos + 6));
                         links_out.push_back(pl); // We'll add text later if needed
                     }
                 }
            }
        } else if (in_tag) {
            if (current_tag.empty() || current_tag.find(' ') == std::string::npos) {
                current_tag += c;
            }
            tag_content += c;
        } else if (!in_script && !in_style) {
            // Replace newlines/tabs with space
            if (c == '\n' || c == '\r' || c == '\t') c = ' ';
            // Avoid double spaces
            if (c == ' ' && (current_line.empty() || current_line.back() == ' ')) continue;
            
            current_line += c;
            if (current_line.size() >= (size_t)wrap_chars) {
                // Find last space
                size_t ls = current_line.find_last_of(" ");
                if (ls != std::string::npos && ls > 0) {
                    lines.push_back(current_line.substr(0, ls));
                    current_line = current_line.substr(ls + 1);
                } else {
                    lines.push_back(current_line);
                    current_line.clear();
                }
            }
        }
    }
    if (!current_line.empty()) lines.push_back(current_line);
    return lines;
}
