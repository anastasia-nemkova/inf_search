# Информационный поиск

## Реализованные лабораторные работы:

01. Добыча корпуса документов
02. Поисковый робот
03. Токенизация
04. Стемминг
05. Закон Ципфа
06. Булев индекс
07. Булев поиск

## Пошаговая сборка

Все команды выполняются относительно корневой директории проекта

### 1. Установка зависимостей

`pip install -r requirements.txt`

### 2. Запуск Docker (MongoDB + Crawler)

`docker-compose up -d --build`

Поднимает контейнеры:
* mongodb — БД для хранения сырых страниц
* mongo-express — веб-интерфейс MongoDB
* crawler — запускает search_robot/main.py
* crawler_stats — собирает статистику

Результат:
MongoDB готова принимать данные
Кроулер парсит сайты и сохраняет в БД

_Просмотр MongoBD: http://localhost:8081_

### 3. Экспорт данных из MongoDB в текстовые файлы

Создаем папку дл файлов

`mkdir -p texts`

Запускаем экспорт

`python utils/export_data.py --output_dir texts`

* Подключается к mongodb://localhost:27017
* Извлекает text и url из всех документов
* Сохраняет:
    `texts/text.txt` — одна строка = один документ
    `texts/url.txt` — соответствующие URL

Результат:
Подготовлены чистые тексты для токенизации и индексации

### 4. Тестирование токенизатора

```
mkdir -p lingvistica/tokenizer/build
cd lingvistica/tokenizer/build && cmake .. && make && cd ../../..
lingvistica/tokenizer/build/tokenizer_tests
```

### 5. Тестирование стеммера

```
mkdir -p lingvistica/stemmer/build
cd lingvistica/stemmer/build && cmake .. && make && cd ../../..
lingvistica/stemmer/build/stemmer_tests
```

### 6. Zipf-анализ

`python zipf_analyze/zipf_analyze.py`

* Токенизирует текст
* Считает частотность слов
* Строит график: ранг vs частота
* Считает коэффициент аппроксимации

### 7. Индексация

`cd search_engine && mkdir -p build && cd build && cmake .. && make && cd ../../`

Запуск индексации

`search_engine/build/indexer --index`

`--stem` для индексации по основам

* Читает `texts/text.txt` и `texts/url.txt`
* Применяет MedicalTokenizer
* При `--stem`: дополнительно применяет RussianStemmer
* Строит:
    `forward_index.bin` — id → url
    `inverted_index.bin` — term → список id

Результат:
Индексы готовы к поиску

### 8. Запуск поискового движка

`search_engine/build/indexer --search`

* Загружает оба индекса
* Запускает интерактивный режим

Поддерживаемые запросы

| Тип | Пример | Описание |
|-----|--------|---------|
| Одиночный термин | `альфа-интерферон` | Поиск по одному слову |
| Логическое И (`AND`) | `глюкоза AND 5.5` | Документы, где есть оба термина |
| Логическое ИЛИ (`OR`) | `в/в OR в/м` | Любой из двуз терминов |
| Исключение (`NOT`) | `антибиотик NOT пенициллин` | Без второго термина |

