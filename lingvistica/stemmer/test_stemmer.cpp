#include "stemmer.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <iostream>

class RussianStemmerTest : public ::testing::Test {
protected:
    RussianStemmer stemmer;
    
    void SetUp() override {
        stemmer.loadStopWords("stop_words.txt");
    }
    
    std::string stem(const std::string& word) {
        return stemmer.stem(word);
    }
};

// ТЕСТ 1: Обычные русские слова
TEST_F(RussianStemmerTest, TestBasicRussianWords) {
    EXPECT_EQ(stem("бегающий"), "бега");
    EXPECT_EQ(stem("бегающая"), "бега");
    EXPECT_EQ(stem("бегающее"), "бега");
}

// ТЕСТ 2: Глаголы
TEST_F(RussianStemmerTest, TestVerbs) {
    EXPECT_EQ(stem("бегать"), "бега");
    EXPECT_EQ(stem("бегаю"), "бега");
    EXPECT_EQ(stem("бегает"), "бега");
    EXPECT_EQ(stem("бегали"), "бега");
}

// ТЕСТ 3: Существительные
TEST_F(RussianStemmerTest, TestNouns) {
    EXPECT_EQ(stem("дом"), "дом");
    EXPECT_EQ(stem("дома"), "дом");
    EXPECT_EQ(stem("дому"), "дом");
    EXPECT_EQ(stem("домом"), "дом");
    EXPECT_EQ(stem("домами"), "дом");
}

// ТЕСТ 4: Прилагательные
TEST_F(RussianStemmerTest, TestAdjectives) {
    EXPECT_EQ(stem("красивый"), "красив");
    EXPECT_EQ(stem("красивая"), "красив");
    EXPECT_EQ(stem("красивое"), "красив");
    EXPECT_EQ(stem("красивыми"), "красив");
    EXPECT_EQ(stem("красивейший"), "красив");
}

// ТЕСТ 5: Причастия и деепричастия
TEST_F(RussianStemmerTest, TestParticiplesAndGerunds) {
    EXPECT_EQ(stem("бегающий"), "бега");
    EXPECT_EQ(stem("бегавший"), "бега");
    EXPECT_EQ(stem("убежав"), "убежа");
    EXPECT_EQ(stem("сделанный"), "сдела");
}

// ТЕСТ 6: Медицинские термины
TEST_F(RussianStemmerTest, TestMedicalTermsRussian) {
    EXPECT_EQ(stem("кардиомиопатия"), "кардиомиопат");
    EXPECT_EQ(stem("кардиомиопатии"), "кардиомиопат");
    EXPECT_EQ(stem("кардиомиопатий"), "кардиомиопат");
    EXPECT_EQ(stem("гипертензия"), "гипертенз");
    EXPECT_EQ(stem("гипертензии"), "гипертенз");
    EXPECT_EQ(stem("артериосклероз"), "артериосклероз");
    EXPECT_EQ(stem("остеохондроз"), "остеохондроз");
    EXPECT_EQ(stem("головокружение"), "головокружен");
    EXPECT_EQ(stem("головокружения"), "головокружен");
}

// ТЕСТ 7: Медицинские термины
TEST_F(RussianStemmerTest, TestMedicalTermsLatinInRussian) {
    EXPECT_EQ(stem("COVID-19"), "COVID-19");
    EXPECT_EQ(stem("SARS-CoV-2"), "SARS-CoV-2");
    EXPECT_EQ(stem("MRI"), "MRI");
    EXPECT_EQ(stem("CT"), "CT");
    EXPECT_EQ(stem("ВИЧ-инфекция"), "вич-инфекц");
    EXPECT_EQ(stem("СПИД-ассоциированный"), "спид-ассоциирова");
}

// ТЕСТ 8: Английские слова 
TEST_F(RussianStemmerTest, TestEnglishWords) {
    EXPECT_EQ(stem("computer"), "computer");
    EXPECT_EQ(stem("programming"), "programming");
    EXPECT_EQ(stem("algorithm"), "algorithm");
    EXPECT_EQ(stem("database"), "database");
    EXPECT_EQ(stem("software"), "software");
    EXPECT_EQ(stem("development"), "development");
}

// ТЕСТ 9: Сложные медицинские названия
TEST_F(RussianStemmerTest, TestComplexMedicalNames) {
    EXPECT_EQ(stem("ибупрофен"), "ибупроф");
    EXPECT_EQ(stem("парацетамол"), "парацетамол");
    EXPECT_EQ(stem("амитриптилин"), "амитриптилин");
    EXPECT_EQ(stem("ацетилсалициловая"), "ацетилсалицилов");
    EXPECT_EQ(stem("метилпреднизолон"), "метилпреднизолон");
    EXPECT_EQ(stem("респираторная"), "респираторн");
}

// ТЕСТ 10: Слова с буквой "ё"
TEST_F(RussianStemmerTest, TestLetterYo) {
    EXPECT_EQ(stem("ёлка"), "елк");
    EXPECT_EQ(stem("ёлочка"), "елочк");
    EXPECT_EQ(stem("пёстрый"), "пестр");
    EXPECT_EQ(stem("лёгочный"), "легочн");
    EXPECT_EQ(stem("сердцёбиение"), "сердцебиен");
}

// ТЕСТ 11: Граничные случаи
TEST_F(RussianStemmerTest, TestEdgeCases) {
    EXPECT_EQ(stem(""), "");
    EXPECT_EQ(stem("я"), "я");
    EXPECT_EQ(stem("он"), "он");
    EXPECT_EQ(stem("мы"), "мы");
    EXPECT_EQ(stem("врч"), "врч");
    EXPECT_EQ(stem("кгб"), "кгб");
}

// ТЕСТ 12: Медицинские аббревиатуры
TEST_F(RussianStemmerTest, TestMedicalAbbreviations) {
    EXPECT_EQ(stem("ВИЧ"), "ВИЧ");
    EXPECT_EQ(stem("ОРИ"), "ОРИ");
    EXPECT_EQ(stem("HIV"), "HIV");
    EXPECT_EQ(stem("AIDS"), "AIDS");
    EXPECT_EQ(stem("PCR"), "PCR");
    EXPECT_EQ(stem("DNA"), "DNA");
    EXPECT_EQ(stem("RNA"), "RNA");
}

// ТЕСТ 13: Профессиональные медицинские термины
TEST_F(RussianStemmerTest, TestProfessionalMedicalTerms) {
    EXPECT_EQ(stem("аппендэктомия"), "аппендэктом");
    EXPECT_EQ(stem("лапароскопия"), "лапароскоп");
    EXPECT_EQ(stem("рентгенография"), "рентгенограф");
    EXPECT_EQ(stem("ультразвуковое"), "ультразвуков");
    EXPECT_EQ(stem("антибиотикотерапия"), "антибиотикотерап");
    EXPECT_EQ(stem("антикоагулянты"), "антикоагулянт");
}

// ТЕСТ 14: Комплексные предложения с медицинскими терминами
TEST_F(RussianStemmerTest, TestMedicalSentences) {
    EXPECT_EQ(stem("кардиологическое"), "кардиологическ");
    EXPECT_EQ(stem("обследование"), "обследован");
    EXPECT_EQ(stem("проведенное"), "проведен");
    EXPECT_EQ(stem("вчера"), "вчер");
    EXPECT_EQ(stem("выявило"), "выяв");
    EXPECT_EQ(stem("признаки"), "признак");
    EXPECT_EQ(stem("ишемической"), "ишемическ");
    EXPECT_EQ(stem("болезни"), "болезн");
    EXPECT_EQ(stem("сердца"), "сердц");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}