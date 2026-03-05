#ifndef STEMMER_H
#define STEMMER_H

#include <string>
#include <vector>
#include <set>
#include <locale>
#include <codecvt>

class RussianStemmer {
public:
    RussianStemmer();
    ~RussianStemmer() = default;
    
    std::string stem(const std::string& word) const;

    bool loadStopWords(const std::string& filename);

    bool isStopWord(const std::string& word) const;
    
private:
    std::wstring utf8_to_wstring(const std::string& str) const;
    std::string wstring_to_utf8(const std::wstring& str) const;
    
    std::wstring stemWord(const std::wstring& word) const;
    std::wstring toLower(const std::wstring& str) const;
    
    bool isVowel(wchar_t ch) const;
    
    size_t findRV(const std::wstring& word) const;
    size_t findR1(const std::wstring& word) const;
    size_t findR2(const std::wstring& word) const;
    
    bool removeEnding(std::wstring& word, const std::vector<std::wstring>& endings, size_t region_start, bool check_preceding_vowel = false, wchar_t vowel1 = L'\0', wchar_t vowel2 = L'\0') const;
    
    bool step1_PerfectiveGerund(std::wstring& word, size_t rv) const;
    bool step1_Adjectival(std::wstring& word, size_t rv) const;
    bool step1_Reflexive(std::wstring& word, size_t rv) const;
    bool step1_Verb(std::wstring& word, size_t rv) const;
    bool step1_Noun(std::wstring& word, size_t rv) const;
    
    void step2_I(std::wstring& word, size_t rv) const;
    void step3_Derivational(std::wstring& word, size_t r2) const;
    bool step4_Superlative(std::wstring& word, size_t rv) const;
    void step4_UndoubleN(std::wstring& word) const;
    void step4_SoftSign(std::wstring& word) const;

    std::set<std::wstring> stopWords;
    std::vector<std::wstring> perfectiveGerund1;
    std::vector<std::wstring> perfectiveGerund2;
    std::vector<std::wstring> adjective;
    std::vector<std::wstring> participle1;
    std::vector<std::wstring> participle2;
    std::vector<std::wstring> reflexive;
    std::vector<std::wstring> verb1;
    std::vector<std::wstring> verb2;
    std::vector<std::wstring> noun;

    std::vector<std::wstring> derivational;
    std::vector<std::wstring> superlative;

    std::wstring vowels;

    void initEndings();
};

#endif