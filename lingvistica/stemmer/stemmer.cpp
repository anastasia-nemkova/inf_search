#include "stemmer.hpp"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <set>
#include <locale>

RussianStemmer::RussianStemmer() {
    std::locale::global(std::locale("en_US.UTF-8"));
    std::wcout.imbue(std::locale());
    
    vowels = L"аеиоуыэюя";
    initEndings();
}

void RussianStemmer::initEndings() {
    perfectiveGerund1 = {
        L"вшись", L"вши", L"в"
    };
    
    perfectiveGerund2 = {
        L"вшись", L"вши", 
        L"ив", L"ыв"
    };

    adjective = {
        L"ее", L"ие", L"ые", L"ое", L"ими", L"ыми", L"ей", 
        L"ий", L"ый", L"ой", L"ем", L"им", L"ым", L"ом", 
        L"его", L"ого", L"ему", L"ому", L"их", L"ых", 
        L"ую", L"юю", L"ая", L"яя", L"ою", L"ею"
    };

    participle1 = {
        L"ем", L"нн", L"вш", L"ющ", L"щ"
    };

    participle2 = {
        L"ивш", L"ывш", L"ующ"
    };

    reflexive = {
        L"ся", L"сь"
    };

    verb1 = {
        L"нно", L"ешь", L"йте", L"ла", L"на", L"ете", 
        L"ли", L"й", L"л", L"ем", L"н", L"ло", 
        L"но", L"ет", L"ют", L"ны", L"ть"
    };

    verb2 = {
        L"ила", L"ыла", L"ена", L"ейте", L"уйте", L"ите", 
        L"или", L"ыли", L"ей", L"уй", L"ил", L"ыл", 
        L"им", L"ым", L"ен", L"ило", L"ыло", L"ено", 
        L"ят", L"ует", L"уют", L"ит", L"ыт", L"ены", 
        L"ить", L"ыть", L"ишь", L"ешь", L"ую", L"ю",
        L"ете", L"ет", L"ут"
    };

    noun = {
        L"а", L"ев", L"ов", L"ие", L"ье", L"е", L"иями", 
        L"ями", L"ами", L"еи", L"ии", L"и", L"ией", 
        L"ей", L"ой", L"ий", L"й", L"иям", L"ям", 
        L"ием", L"ем", L"ам", L"ом", L"о", L"у", 
        L"ах", L"иях", L"ях", L"ы", L"ь", L"ию", 
        L"ью", L"ю", L"ия", L"ья", L"я"
    };

    derivational = {
        L"ость", L"ост"
    };

    superlative = {
        L"ейше", L"ейш"
    };
}

std::wstring RussianStemmer::utf8_to_wstring(const std::string& str) const {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    try {
        return converter.from_bytes(str);
    } catch (...) {
        return L"";
    }
}

std::string RussianStemmer::wstring_to_utf8(const std::wstring& str) const {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    try {
        return converter.to_bytes(str);
    } catch (...) {
        return "";
    }
}

std::wstring RussianStemmer::toLower(const std::wstring& str) const {
    std::wstring result = str;
    for (size_t i = 0; i < result.length(); ++i) {
        wchar_t ch = result[i];
        if (ch >= L'А' && ch <= L'Я') {
            result[i] = ch + 32;
        }
        else if (ch == L'Ё') {
            result[i] = L'ё';
        }
    }
    return result;
}

bool RussianStemmer::isVowel(wchar_t ch) const {
    if (ch == L'ё') ch = L'е';
    return vowels.find(ch) != std::wstring::npos;
}

size_t RussianStemmer::findRV(const std::wstring& word) const {
    for (size_t i = 0; i < word.length(); ++i) {
        if (isVowel(word[i])) {
            return i + 1;
        }
    }
    return word.length();
}

size_t RussianStemmer::findR1(const std::wstring& word) const {
    size_t i = 0;
    while (i < word.length() && !isVowel(word[i])) {
        i++;
    }
    while (i < word.length() && isVowel(word[i])) {
        i++;
    }
    
    return i;
}

size_t RussianStemmer::findR2(const std::wstring& word) const {
    size_t r1 = findR1(word);
    
    if (r1 >= word.length()) {
        return word.length();
    }
    size_t i = r1;
    while (i < word.length() && !isVowel(word[i])) {
        i++;
    }

    while (i < word.length() && isVowel(word[i])) {
        i++;
    }
    
    return i;
}

bool RussianStemmer::removeEnding(std::wstring& word, const std::vector<std::wstring>& endings, size_t region_start, bool check_preceding_vowel, wchar_t vowel1, wchar_t vowel2) const {
    std::vector<std::wstring> sorted_endings = endings;
    std::sort(sorted_endings.begin(), sorted_endings.end(), [](const std::wstring& a, const std::wstring& b) {return a.length() > b.length();});
    
    for (const auto& ending : sorted_endings) {
        if (word.length() >= ending.length()) {
            size_t pos = word.length() - ending.length();
            if (pos >= region_start && word.substr(pos) == ending) {
                if (check_preceding_vowel && pos > 0) {
                    wchar_t preceding_char = word[pos - 1];
                    if (preceding_char != vowel1 && preceding_char != vowel2) {
                        continue;
                    }
                }
                word = word.substr(0, pos);
                return true;
            }
        }
    }
    return false;
}

bool RussianStemmer::step1_PerfectiveGerund(std::wstring& word, size_t rv) const {
    if (removeEnding(word, perfectiveGerund1, rv, true, L'а', L'я')) {
        return true;
    }

    if (removeEnding(word, perfectiveGerund2, rv, false, 0, 0)) {
        return true;
    }
    
    return false;
}

bool RussianStemmer::step1_Reflexive(std::wstring& word, size_t rv) const {
    return removeEnding(word, reflexive, rv, false, 0, 0);
}

bool RussianStemmer::step1_Adjectival(std::wstring& word, size_t rv) const {
    std::wstring temp = word;
    for (const auto& participle : participle1) {
        if (temp.length() >= participle.length() + 2 && temp.length() >= rv) {
            size_t pos = temp.length() - participle.length() - 2;
            if (pos >= rv && temp.substr(pos, participle.length()) == participle) {
                wchar_t before = temp[pos - 1];
                if (before == L'а' || before == L'я') {
                    if (pos >= 2) {
                         std::wstring temp2 = temp.substr(0, pos) + temp.substr(pos + 2);
                         if (removeEnding(temp2, adjective, rv, false, 0, 0)) {
                             word = temp2;
                             return true;
                         }
                    }
                }
            }
        }
    }

    for (const auto& participle : participle2) {
        if (temp.length() >= participle.length() && temp.length() >= rv) {
            size_t pos = temp.length() - participle.length();
            if (pos >= rv && temp.substr(pos) == participle) {
                std::wstring temp2 = temp.substr(0, pos);
                if (removeEnding(temp2, adjective, rv, false, 0, 0)) {
                    word = temp2;
                    return true;
                }
            }
        }
    }

    if (temp.length() >= rv) {
        if (removeEnding(word, adjective, rv, false, 0, 0)) {
            return true;
        }
    }

    return false;
}

bool RussianStemmer::step1_Verb(std::wstring& word, size_t rv) const {
    if (removeEnding(word, verb1, rv, true, L'а', L'я')) {
        return true;
    }

    if (removeEnding(word, verb2, rv, false, 0, 0)) {
        return true;
    }
    
    return false;
}

bool RussianStemmer::step1_Noun(std::wstring& word, size_t rv) const {
    return removeEnding(word, noun, rv, false, 0, 0);
}

void RussianStemmer::step2_I(std::wstring& word, size_t rv) const {
    if (word.length() > rv && word.back() == L'и') {
        word.pop_back();
    }
}

void RussianStemmer::step3_Derivational(std::wstring& word, size_t r2) const {
    removeEnding(word, derivational, r2, false, 0, 0);
}

bool RussianStemmer::step4_Superlative(std::wstring& word, size_t rv) const {
    return removeEnding(word, superlative, rv, false, 0, 0);
}

void RussianStemmer::step4_UndoubleN(std::wstring& word) const {
    if (word.length() >= 2 && word[word.length() - 1] == L'н' && word[word.length() - 2] == L'н') {
        word.pop_back();
    }
}

void RussianStemmer::step4_SoftSign(std::wstring& word) const {
    if (!word.empty() && word.back() == L'ь') {
        word.pop_back();
    }
}

std::wstring RussianStemmer::stemWord(const std::wstring& word) const {
    if (word.length() < 4) {
        return word;
    }

    std::wstring result = toLower(word);
    std::replace(result.begin(), result.end(), L'ё', L'е');

    size_t rv = findRV(result);
    size_t r1 = findR1(result);
    size_t r2 = findR2(result);

    if (step1_PerfectiveGerund(result, rv)) {
    } else {
        step1_Reflexive(result, rv);
        if (!step1_Adjectival(result, rv)) {
            if (!step1_Verb(result, rv)) {
                step1_Noun(result, rv);
            }
        }
    }
    step2_I(result, rv);
    step3_Derivational(result, r2);

    if (step4_Superlative(result, rv)) {
        step4_UndoubleN(result);
    } else {
        step4_UndoubleN(result);
        step4_SoftSign(result);
    }

    bool hasVowel = false;
    for (wchar_t ch : result) {
        if (isVowel(ch)) {
            hasVowel = true;
            break;
        }
    }
    
    if (!hasVowel) {
        return word;
    }
    
    return result;
}

std::string RussianStemmer::stem(const std::string& word) const {
    if (word.empty()) {
        return word;
    }
    
    std::wstring wword = utf8_to_wstring(word);
    if (wword.empty()) {
        return word;
    }
    
    std::wstring stemmed = stemWord(wword);
    
    return wstring_to_utf8(stemmed);
}

bool RussianStemmer::loadStopWords(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        
        if (start != std::string::npos && end != std::string::npos) {
            std::string word = line.substr(start, end - start + 1);
            if (!word.empty()) {
                std::wstring wword = utf8_to_wstring(word);
                if (!wword.empty()) {
                    stopWords.insert(toLower(wword));
                }
            }
        }
    }
    
    file.close();
    return true;
}

bool RussianStemmer::isStopWord(const std::string& word) const {
    std::wstring wword = utf8_to_wstring(word);
    if (wword.empty()) {
        return false;
    }
    return stopWords.find(toLower(wword)) != stopWords.end();
}