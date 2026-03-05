#include <gtest/gtest.h>
#include <algorithm>
#include "tokenizer.hpp"

using namespace inf_search;

class MedicalTokenizerTest : public ::testing::Test {
protected:
    MedicalTokenizer tokenizer;
    bool tokensContain(const std::vector<std::string>& tokens, const std::string& target) {
        return std::find(tokens.begin(), tokens.end(), target) != tokens.end();
    }
};

// Тест 1: Базовая токенизация русских слов
TEST_F(MedicalTokenizerTest, BasicRussianTokenization) {
    std::string text = "Принимать по одной таблетке";
    auto tokens = tokenizer.tokenize(text);
    
    EXPECT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0], "принимать");
    EXPECT_EQ(tokens[1], "по");
    EXPECT_EQ(tokens[2], "одной");
    EXPECT_EQ(tokens[3], "таблетке");
}

// Тест 2: Медицинские сокращения (в/м, в/в)
TEST_F(MedicalTokenizerTest, MedicalAbbreviationsTokenization) {
    std::string text = "Вводить в/м или в/в по показаниям";
    auto tokens = tokenizer.tokenize(text);
    
    EXPECT_GE(tokens.size(), 5);
    EXPECT_TRUE(tokensContain(tokens, "в/м"));
    EXPECT_TRUE(tokensContain(tokens, "в/в"));
    EXPECT_FALSE(tokensContain(tokens, "в"));
    EXPECT_FALSE(tokensContain(tokens, "м"));
}

// Тест 3: Дефисные медицинские термины
TEST_F(MedicalTokenizerTest, HyphenatedMedicalTerms) {
    std::string text = "5-НОК и бета-каротин с омега-3 кислотами";
    auto tokens = tokenizer.tokenize(text);
    
    EXPECT_GE(tokens.size(), 6);
    EXPECT_TRUE(tokensContain(tokens, "5-нок"));
    EXPECT_TRUE(tokensContain(tokens, "бета-каротин"));
    EXPECT_TRUE(tokensContain(tokens, "омега-3"));
    EXPECT_FALSE(tokensContain(tokens, "бета"));     
    EXPECT_FALSE(tokensContain(tokens, "каротин"));  
}

// Тест 4: Витамины и элементы
TEST_F(MedicalTokenizerTest, VitaminsAndElementsCasePreservation) {
    std::string text = "Витамины C, D3, B12, PP, K1, B1";
    auto tokens = tokenizer.tokenize(text);

    EXPECT_TRUE(tokensContain(tokens, "C"));
    EXPECT_TRUE(tokensContain(tokens, "D3"));
    EXPECT_TRUE(tokensContain(tokens, "B12"));
    EXPECT_TRUE(tokensContain(tokens, "PP"));
    EXPECT_TRUE(tokensContain(tokens, "K1"));
    EXPECT_TRUE(tokensContain(tokens, "B1"));
    EXPECT_TRUE(tokensContain(tokens, "витамины"));
}

// Тест 5: Числовые диапазоны с UTF-8 тире
TEST_F(MedicalTokenizerTest, NumericRangesWithUTF8Dash) {
    std::string text = "Температура 15–30 °C, диапазон 10-20 мг";
    auto tokens = tokenizer.tokenize(text);

    EXPECT_FALSE(tokensContain(tokens, "15"));
    EXPECT_FALSE(tokensContain(tokens, "30"));
}

// Тест 6: Десятичные числа и диапазоны
TEST_F(MedicalTokenizerTest, DecimalNumbersAndRanges) {
    std::string text = "pH 7.2-7.4, концентрация 0.9% NaCl";
    auto tokens = tokenizer.tokenize(text);

    EXPECT_TRUE(tokensContain(tokens, "ph"));
    bool foundDecimalRange = false;
    for (const auto& token : tokens) {
        if (token == "7.2-7.4") {
            foundDecimalRange = true;
            break;
        }
    }
    EXPECT_TRUE(foundDecimalRange) << "Не найден токен 7.2-7.4";
}

// Тест 7: Символ градуса и температура
TEST_F(MedicalTokenizerTest, DegreeSymbolAndTemperature) {
    std::string text = "Хранить при 15–30 °C, не выше 25°C";
    auto tokens = tokenizer.tokenize(text);
    bool foundDegreeCelsius = false;
    for (const auto& token : tokens) {
        if (token == "°C") {
            foundDegreeCelsius = true;
            break;
        }
    }
    EXPECT_TRUE(foundDegreeCelsius) << "Не найден токен °C";

    EXPECT_TRUE(tokensContain(tokens, "25°C"));
}

// Тест 8: Сложные химические соединения
TEST_F(MedicalTokenizerTest, ComplexChemicalCompounds) {
    std::string text = "альфа-линоленовая кислота и бета-каротин";
    auto tokens = tokenizer.tokenize(text);
    
    EXPECT_TRUE(tokensContain(tokens, "альфа-линоленовая"));
    EXPECT_TRUE(tokensContain(tokens, "бета-каротин"));
    EXPECT_TRUE(tokensContain(tokens, "кислота"));
    EXPECT_TRUE(tokensContain(tokens, "и"));
}

// Тест 9: Единицы измерения
TEST_F(MedicalTokenizerTest, MeasurementUnits) {
    std::string text = "Принимать по 100 мг 2 раза в день, 5 мл суспензии";
    auto tokens = tokenizer.tokenize(text);
    
    EXPECT_TRUE(tokensContain(tokens, "мг"));
    EXPECT_TRUE(tokensContain(tokens, "мл"));
    EXPECT_TRUE(tokensContain(tokens, "раза"));
    EXPECT_TRUE(tokensContain(tokens, "день"));
    EXPECT_TRUE(tokensContain(tokens, "суспензии"));
}

// Тест 10: Смешанный русско-английский текст
TEST_F(MedicalTokenizerTest, MixedRussianEnglishText) {
    std::string text = "Vitamin C (аскорбиновая кислота) и Omega-3";
    auto tokens = tokenizer.tokenize(text);
    EXPECT_TRUE(tokensContain(tokens, "vitamin"));
    EXPECT_TRUE(tokensContain(tokens, "C"));
    EXPECT_TRUE(tokensContain(tokens, "аскорбиновая"));
    EXPECT_TRUE(tokensContain(tokens, "кислота"));
    EXPECT_TRUE(tokensContain(tokens, "и"));
}

// Тест 11: Текст с различной пунктуацией
TEST_F(MedicalTokenizerTest, TextWithVariousPunctuation) {
    std::string text = "Принимать: утром и вечером! После еды? Да, обязательно.";
    auto tokens = tokenizer.tokenize(text);
    EXPECT_FALSE(tokensContain(tokens, ":"));
    EXPECT_FALSE(tokensContain(tokens, "!"));
    EXPECT_FALSE(tokensContain(tokens, "?"));
    EXPECT_FALSE(tokensContain(tokens, ","));
    EXPECT_FALSE(tokensContain(tokens, "."));
    EXPECT_TRUE(tokensContain(tokens, "принимать"));
    EXPECT_TRUE(tokensContain(tokens, "утром"));
    EXPECT_TRUE(tokensContain(tokens, "и"));
    EXPECT_TRUE(tokensContain(tokens, "вечером"));
    EXPECT_TRUE(tokensContain(tokens, "после"));
    EXPECT_TRUE(tokensContain(tokens, "еды"));
    EXPECT_TRUE(tokensContain(tokens, "да"));
    EXPECT_TRUE(tokensContain(tokens, "обязательно"));
}

// Тест 12: Процентные значения
TEST_F(MedicalTokenizerTest, PercentageValues) {
    std::string text = "Раствор 0.9% натрия хлорида, 5% глюкоза";
    auto tokens = tokenizer.tokenize(text);

    EXPECT_TRUE(tokensContain(tokens, "5%"));
    EXPECT_TRUE(tokensContain(tokens, "натрия"));
    EXPECT_TRUE(tokensContain(tokens, "хлорида"));
    EXPECT_TRUE(tokensContain(tokens, "глюкоза"));
}

// Тест 13: Пустой текст и пробелы
TEST_F(MedicalTokenizerTest, EmptyTextAndWhitespace) {
    std::string empty_text = "";
    auto empty_tokens = tokenizer.tokenize(empty_text);
    EXPECT_EQ(empty_tokens.size(), 0);
    
    std::string spaces_text = "   \t\n  ";
    auto spaces_tokens = tokenizer.tokenize(spaces_text);
    EXPECT_EQ(spaces_tokens.size(), 0);
    
    std::string punctuation_text = "!!! ??? ... ,,, ;;";
    auto punct_tokens = tokenizer.tokenize(punctuation_text);
    EXPECT_EQ(punct_tokens.size(), 0);
}

// Тест 14: Составные медицинские термины с префиксами
TEST_F(MedicalTokenizerTest, CompoundMedicalTermsWithPrefixes) {
    std::string text = "орто-фенилфенол и мета-ксиленол изо-пропил";
    auto tokens = tokenizer.tokenize(text);
    
    EXPECT_TRUE(tokensContain(tokens, "орто-фенилфенол"));
    EXPECT_TRUE(tokensContain(tokens, "мета-ксиленол"));
    EXPECT_TRUE(tokensContain(tokens, "изо-пропил"));
    EXPECT_TRUE(tokensContain(tokens, "и"));
}

// Тест 15: Реальные примеры из медицинских текстов
TEST_F(MedicalTokenizerTest, RealMedicalTextExamples) {
    std::string text = 
        "Каопектат®: суспензия 15 мл содержит 750 мг аттапульгита. "
        "Применять в/м по 100 мг витамина C и D3. "
        "Хранить при 15–30 °C. pH 7.2-7.4, концентрация";
    
    auto tokens = tokenizer.tokenize(text);
    EXPECT_TRUE(tokensContain(tokens, "каопектат®"));
    EXPECT_TRUE(tokensContain(tokens, "мл"));
    EXPECT_TRUE(tokensContain(tokens, "мг"));
    EXPECT_TRUE(tokensContain(tokens, "аттапульгита"));
    EXPECT_TRUE(tokensContain(tokens, "в/м"));
    EXPECT_TRUE(tokensContain(tokens, "витамина"));
    EXPECT_TRUE(tokensContain(tokens, "C"));
    EXPECT_TRUE(tokensContain(tokens, "D3"));
    EXPECT_TRUE(tokensContain(tokens, "°C"));
    EXPECT_TRUE(tokensContain(tokens, "ph"));

    bool foundRange = false;
    for (const auto& token : tokens) {
        if (token == "7.2-7.4") {
            foundRange = true;
            break;
        }
    }
    EXPECT_TRUE(foundRange) << "Не найден токен 7.2-7.4";
    EXPECT_FALSE(tokensContain(tokens, "7"));
    EXPECT_FALSE(tokensContain(tokens, "2-7"));
    EXPECT_FALSE(tokensContain(tokens, "4"));
    EXPECT_FALSE(tokensContain(tokens, "0"));
    EXPECT_FALSE(tokensContain(tokens, "9"));
}

// Тест 16: Граничные случаи с дефисами
TEST_F(MedicalTokenizerTest, EdgeCasesWithHyphens) {
    std::string text = "пре- и постоперационный период, анти-В-клеточный";
    auto tokens = tokenizer.tokenize(text);
    EXPECT_TRUE(tokensContain(tokens, "и"));
    EXPECT_TRUE(tokensContain(tokens, "постоперационный"));
    EXPECT_TRUE(tokensContain(tokens, "период"));
}

// Тест 17: Таблетки и формы выпуска
TEST_F(MedicalTokenizerTest, TabletsAndReleaseForms) {
    std::string text = "Таблетки №10, капсулы 100 мг, ампулы 2 мл";
    auto tokens = tokenizer.tokenize(text);
    
    EXPECT_TRUE(tokensContain(tokens, "таблетки"));
    EXPECT_TRUE(tokensContain(tokens, "капсулы"));
    EXPECT_TRUE(tokensContain(tokens, "мг"));
    EXPECT_TRUE(tokensContain(tokens, "ампулы"));

    EXPECT_TRUE(tokensContain(tokens, "мл"));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}