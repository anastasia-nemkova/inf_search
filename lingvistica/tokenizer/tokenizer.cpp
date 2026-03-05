#include "tokenizer.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstring>

namespace inf_search {

static inline std::string trim(const std::string& str) {
    if (str.empty()) return str;
    size_t start = 0, end = str.size() - 1;
    while (start <= end && std::isspace(static_cast<unsigned char>(str[start]))) ++start;
    while (end > start && std::isspace(static_cast<unsigned char>(str[end]))) --end;
    return str.substr(start, end - start + 1);
}

bool MedicalTokenizer::shouldKeepUTF8Punctuation(const std::string& str) const {
    return str == "/" || str == "-" || str == "%" || str == "+" ||
           str == UTF8_EN_DASH || str == UTF8_EM_DASH || str == UTF8_DEGREE;
}

MedicalTokenizer::MedicalTokenizer() {
    medical_abbreviations_ = {
        "в/м", "в/в", "п/к", "в/б", "в/кап", "мг", "мл", "г", "кг",
        "ед", "мкг", "ммоль", "нмоль", "моль", "л", "см", "мм",
        "таб", "кап", "амп", "фл", "уп", "шт"
    };
    vitamins_elements_ = {
        "A", "B", "C", "D", "E", "K", "PP", "H", "P", "U", "F",
        "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9", "B10", "B11", "B12",
        "D2", "D3", "D4", "D5",
        "K1", "K2", "K3", "K4", "K5"
    };
    special_compounds_ = {
        "альфа-", "бета-", "гамма-", "дельта-", "омега-",
        "орто-", "мета-", "пара-", "цис-", "транс-",
        "изо-", "нео-", "псевдо-"
    };
}

std::string MedicalTokenizer::cleanToken(const std::string& token) const {
    if (token.empty()) return token;
    size_t len = token.length();
    size_t start = 0, end = len - 1;

    while (start <= end && std::ispunct(static_cast<unsigned char>(token[start]))) {
        std::string c = token.substr(start, 1);
        if (!shouldKeepUTF8Punctuation(c)) ++start;
        else break;
    }
    if (start > end) return "";

    while (end >= start && std::ispunct(static_cast<unsigned char>(token[end]))) {
        std::string c = token.substr(end, 1);
        if (!shouldKeepUTF8Punctuation(c)) --end;
        else break;
    }

    return token.substr(start, end - start + 1);
}

static inline int utf8_char_len(unsigned char c) {
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

std::string toLowerUTF8(const std::string& str) {
    std::string result;
    result.reserve(str.length());

    for (size_t i = 0; i < str.length(); ) {
        unsigned char uc = static_cast<unsigned char>(str[i]);

        if (uc < 128) {
            if (uc >= 'A' && uc <= 'Z') {
                result += static_cast<char>(uc + ('a' - 'A'));
            } else {
                result += str[i];
            }
            ++i;
        }
        else if ((uc & 0xE0) == 0xC0 && i + 1 < str.length()) {
            unsigned char uc2 = static_cast<unsigned char>(str[i + 1]);
            if ((uc2 & 0xC0) == 0x80) {
                unsigned int codepoint = ((uc & 0x1F) << 6) | (uc2 & 0x3F);
                if (codepoint >= 0x0410 && codepoint <= 0x042F) {
                    codepoint += 0x20;
                    result += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                }
                else if (codepoint == 0x0401) {
                    result += '\xD0';
                    result += '\xB9';
                }
                else {
                    result += str[i];
                    result += str[i + 1];
                }
                i += 2;
            } else {
                result += str[i];
                ++i;
            }
        }
        else if ((uc & 0xF0) == 0xE0 && i + 2 < str.length()) {
            result += str[i];
            result += str[i + 1];
            result += str[i + 2];
            i += 3;
        }
        else if ((uc & 0xF8) == 0xF0 && i + 3 < str.length()) {
            result += str[i];
            result += str[i + 1];
            result += str[i + 2];
            result += str[i + 3];
            i += 4;
        }
        else {
            result += str[i];
            ++i;
        }
    }

    return result;
}

std::string MedicalTokenizer::normalizeToken(const std::string& token) const {
    std::string cleaned = cleanToken(token);
    if (cleaned.empty()) return cleaned;

    size_t pos = cleaned.find(UTF8_DEGREE);
    if (pos != std::string::npos && pos + UTF8_DEGREE.length() < cleaned.length()) {
        char next = cleaned[pos + UTF8_DEGREE.length()];
        if (next == 'c' || next == 'C') {
            return cleaned.substr(0, pos + UTF8_DEGREE.length()) + 'C';
        }
    }

    if (isVitaminOrElement(cleaned)) {
        return cleaned;
    }

    return toLowerUTF8(cleaned);
}

bool MedicalTokenizer::isMedicalAbbreviation(const std::string& token) const {
    std::string lower = toLowerUTF8(cleanToken(token));
    return std::find(medical_abbreviations_.begin(), medical_abbreviations_.end(), lower) != medical_abbreviations_.end();
}

bool MedicalTokenizer::isVitaminOrElement(const std::string& token) const {
    std::string cleaned = cleanToken(token);
    return std::find(vitamins_elements_.begin(), vitamins_elements_.end(), cleaned) != vitamins_elements_.end();
}

bool MedicalTokenizer::isSpecialMedicalToken(const std::string& token) const {
    std::string lower = toLowerUTF8(cleanToken(token));
    for (const auto& prefix : special_compounds_) {
        if (lower.compare(0, prefix.length(), prefix) == 0) {
            return true;
        }
    }
    return false;
}

bool MedicalTokenizer::isHyphenatedMedicalTerm(const std::string& token) const {
    size_t dash_pos = token.find('-');
    if (dash_pos == std::string::npos || dash_pos == 0 || dash_pos == token.length() - 1)
        return false;

    std::string left = token.substr(0, dash_pos);
    std::string right = token.substr(dash_pos + 1);

    bool left_ok = !left.empty() && (
        std::all_of(left.begin(), left.end(), ::isdigit) ||
        (left.length() == 1 && std::isalpha(static_cast<unsigned char>(left[0])))
    );

    bool right_ok = !right.empty() && std::all_of(right.begin(), right.end(), [](unsigned char c) {
        return std::isalpha(c) || (c & 0x80);
    });

    return left_ok && right_ok;
}

std::string MedicalTokenizer::preprocessText(const std::string& text) const {
    if (text.empty()) return text;

    std::string result;
    result.reserve(text.size() + 16);

    for (size_t i = 0; i < text.size(); ) {
        unsigned char c = text[i];

        if (c < 128) {
            if (c == '-' || c == '/' || c == '%' || c == '+' || c == '.') {
                result += static_cast<char>(c);
            } else if (std::isspace(c)) {
                result += ' ';
            } else if (std::ispunct(c)) {
                result += ' ';
            } else {
                result += static_cast<char>(c);
            }
            ++i;
        } else {
            int len = utf8_char_len(c);
            if (i + len <= text.size()) {
                result.append(&text[i], len);
                i += len;
            } else {
                result += static_cast<char>(c);
                ++i;
            }
        }
    }

    std::string cleaned;
    cleaned.reserve(result.size());
    bool last_space = false;
    for (char c : result) {
        if (c == ' ') {
            if (!last_space) {
                cleaned += ' ';
                last_space = true;
            }
        } else {
            cleaned += c;
            last_space = false;
        }
    }

    return cleaned;
}

static bool isDecimalRange(const std::string& s) {
    size_t dot1 = s.find('.');
    size_t dash = s.find('-', dot1 + 1);
    size_t dot2 = s.find('.', dash + 1);
    if (dot1 == std::string::npos || dash == std::string::npos || dot2 == std::string::npos) return false;

    for (size_t i = 0; i < s.size(); ++i) {
        if (i == dot1 || i == dash || i == dot2) continue;
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    return true;
}

static bool isPercent(const std::string& s) {
    if (s.back() != '%') return false;
    return std::all_of(s.begin(), s.end() - 1, ::isdigit);
}

static bool isNumDashNum(const std::string& s) {
    size_t dash = s.find('-');
    if (dash == 0 || dash == s.size() - 1 || dash == std::string::npos) return false;
    return std::all_of(s.begin(), s.begin() + dash, ::isdigit) &&
           std::all_of(s.begin() + dash + 1, s.end(), ::isdigit);
}

static bool isFloat(const std::string& s) {
    size_t dot = s.find('.');
    if (dot == 0 || dot == s.size() - 1 || dot == std::string::npos) return false;
    return std::all_of(s.begin(), s.begin() + dot, ::isdigit) &&
           std::all_of(s.begin() + dot + 1, s.end(), ::isdigit);
}

static bool isTemperature(const std::string& s) {
    size_t deg = s.find("°C");
    if (deg == std::string::npos || deg == 0) return false;
    std::string num_part = s.substr(0, deg);
    return isFloat(num_part) || std::all_of(num_part.begin(), num_part.end(), ::isdigit);
}

static bool isTempRange(const std::string& s) {
    size_t deg = s.find("°C");
    if (deg == std::string::npos || deg <= 2) return false;
    std::string part = s.substr(0, deg);
    size_t dash = part.find('-');
    return dash != 0 && dash != part.size() - 1 && dash != std::string::npos &&
           std::all_of(part.begin(), part.begin() + dash, ::isdigit) &&
           std::all_of(part.begin() + dash + 1, part.end(), ::isdigit);
}

std::vector<std::string> MedicalTokenizer::tokenize(const std::string& text, TokenizationStats& stats) {
    std::vector<std::string> tokens;
    tokens.reserve(128); 

    std::string processed = preprocessText(text);
    std::istringstream iss(processed);
    std::string raw_token;

    while (iss >> raw_token) {
        if (raw_token.empty()) continue;

        std::string cleaned = cleanToken(raw_token);
        if (cleaned.empty()) continue;

        if (isSpecialMedicalToken(cleaned) ||
            isMedicalAbbreviation(cleaned) ||
            isVitaminOrElement(cleaned)) {
            tokens.push_back(normalizeToken(cleaned));
        }
        else if (isDecimalRange(cleaned) ||
                 isPercent(cleaned) ||
                 isNumDashNum(cleaned) ||
                 isFloat(cleaned) ||
                 isTemperature(cleaned) ||
                 isTempRange(cleaned)) {
            tokens.push_back(cleaned);
        }
        else if (cleaned.find('/') != std::string::npos) {
            tokens.push_back(normalizeToken(cleaned));
        }
        else if (isHyphenatedMedicalTerm(cleaned)) {
            tokens.push_back(normalizeToken(cleaned));
        }
        else {
            bool is_word = std::all_of(cleaned.begin(), cleaned.end(), [](unsigned char c) {
                return std::isalpha(c) || (c & 0x80);
            });
            if (is_word) {
                tokens.push_back(normalizeToken(cleaned));
            }
        }
    }

    stats.total_tokens = tokens.size();
    if (stats.total_tokens > 0) {
        size_t total_len = 0;
        stats.shortest_token = tokens[0].length();
        stats.longest_token = 0;
        for (const auto& t : tokens) {
            size_t len = t.length();
            total_len += len;
            if (len < stats.shortest_token) stats.shortest_token = len;
            if (len > stats.longest_token) stats.longest_token = len;
        }
        stats.avg_token_length = static_cast<double>(total_len) / stats.total_tokens;
    } else {
        stats.shortest_token = 0;
        stats.avg_token_length = 0.0;
    }

    return tokens;
}

std::vector<std::string> MedicalTokenizer::tokenize(const std::string& text) {
    TokenizationStats dummy;
    return tokenize(text, dummy);
}

void TokenizationStats::print() const {
    std::cout << "Total tokens: " << total_tokens << "\n";
    std::cout << "Average length: " << std::fixed << std::setprecision(2) << avg_token_length << "\n";
    std::cout << "Shortest: " << shortest_token << ", Longest: " << longest_token << "\n";
}

}