#ifndef TOKENIZER_HPP
#define TOKENIZER_HPP

#include <string>
#include <vector>
#include <memory>

namespace inf_search {

struct TokenizationStats {
    size_t total_tokens = 0;
    double avg_token_length = 0.0;
    size_t shortest_token = 0;
    size_t longest_token = 0;
    
    void print() const;
};

class MedicalTokenizer {
public:
    MedicalTokenizer();
    ~MedicalTokenizer() = default;
    
    std::vector<std::string> tokenize(const std::string& text);
    std::vector<std::string> tokenize(const std::string& text, TokenizationStats& stats);
    
    bool isMedicalAbbreviation(const std::string& token) const;
    bool isVitaminOrElement(const std::string& token) const;
    bool isNumericRange(const std::string& token) const;
    
private:
    bool isUTF8Punctuation(const std::string& str) const;
    bool shouldKeepUTF8Punctuation(const std::string& str) const;
    bool shouldKeepDotInToken(const std::string& token) const;
    std::string preprocessText(const std::string& text) const;
    std::string normalizeToken(const std::string& token) const;
    std::string cleanToken(const std::string& token) const;
    bool isSpecialMedicalToken(const std::string& token) const;
    bool isHyphenatedMedicalTerm(const std::string& token) const;

    const std::string UTF8_EN_DASH = "\xE2\x80\x93";    // –
    const std::string UTF8_EM_DASH = "\xE2\x80\x94";    // —
    const std::string UTF8_DEGREE  = "\xC2\xB0";        // °
    
    std::vector<std::string> medical_abbreviations_;
    std::vector<std::string> vitamins_elements_;
    std::vector<std::string> special_compounds_;
};
}

#endif