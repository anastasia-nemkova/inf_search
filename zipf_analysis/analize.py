import matplotlib
matplotlib.use('Agg')

import matplotlib.pyplot as plt
import numpy as np
from collections import Counter, defaultdict
import time
from pymongo import MongoClient

try:
    import py_inf_search
    USE_CPP_MODULES = True
    print("Используются C++ модули через py_inf_search.")
except ImportError as e:
    print(f"C++ модуль py_inf_search не найден или не может быть загружен: {e}")
    print("Интеграция с C++ не удалась.")
    exit(1)

def connect_to_db():
    host = 'localhost'
    port = 27017
    database_name = 'inf_search'
    collection_name = 'documents'

    print(f"Подключение к MongoDB: {host}:{port}, DB: {database_name}, Collection: {collection_name}")
    client = MongoClient(
        host=host,
        port=port,
        serverSelectionTimeoutMS=5000 
    )
    db = client[database_name]
    collection = db[collection_name]
    try:
        client.admin.command('ping')
        print("Подключение к MongoDB успешно.")
    except Exception as e:
        print(f"Ошибка подключения к MongoDB: {e}")
        raise
    return collection

def fetch_all_texts_generator(collection):
    print("Извлечение текстов из базы данных...")
    cursor = collection.find({}, {'clean_text': 1, '_id': 0})
    count = 0
    for doc in cursor:
        text = doc.get('clean_text')
        if text:
            yield text
            count += 1
            if count % 10000 == 0:
                print(f"  Обработано {count} документов...")
    print(f"Всего извлечено {count} документов с текстом.")
    return count 

def analyze_and_print_stats(collection, tokenizer, stemmer=None, description=""):
    """
    Выполняет анализ токенов/стемов и печатает статистику.
    """
    print(f"\n--- Начало анализа: {description} ---")
    freq_counter = Counter()
    doc_count = 0
    total_tokens_or_stems = 0
    all_lengths = [] 
    original_tokens_for_stemming_example = defaultdict(set) 
    stem_to_original_map = defaultdict(set) 

    start_time_overall = time.time()
    
    for text in fetch_all_texts_generator(collection):
        doc_count += 1
        if not isinstance(text, str) or not text.strip():
            continue

        start_time_tokenize = time.time()
        tokens = tokenizer.tokenize(text)
        tokenize_time = time.time() - start_time_tokenize
        
        if stemmer:
            valid_tokens = [token for token in tokens if (token.isalpha() or token.isdigit()) and token.strip()]

            start_time_stem = time.time()
            stemmed_tokens = [stemmer.stem(token) for token in valid_tokens]
            stem_time = time.time() - start_time_stem

            freq_counter.update(stemmed_tokens)
            total_tokens_or_stems += len(stemmed_tokens)
            all_lengths.extend([len(st) for st in stemmed_tokens])

            for orig_tok, stemmed_tok in zip(valid_tokens, stemmed_tokens):
                 stem_to_original_map[stemmed_tok].add(orig_tok)
                 original_tokens_for_stemming_example[orig_tok].add(stemmed_tok)
                 
        else:
            valid_tokens = [token for token in tokens if (token.isalpha() or token.isdigit()) and token.strip()]
            
            freq_counter.update(valid_tokens)
            total_tokens_or_stems += len(valid_tokens)
            all_lengths.extend([len(tok) for tok in valid_tokens])

        if doc_count % 10000 == 0:
            print(f"  Проанализировано {doc_count} документов для {description}...")

    overall_time = time.time() - start_time_overall
    print(f"Обработка документов завершена для {description}.")

    if not freq_counter:
        print(f"После токенизации и фильтрации не осталось токенов/стемов для {description}.")
        return

    unique_tokens_or_stems = len(freq_counter)
    total_tokens_docs = doc_count
    
    if all_lengths:
        max_len = max(all_lengths)
        min_len = min(all_lengths)
        avg_len = sum(all_lengths) / len(all_lengths)
    else:
        max_len, min_len, avg_len = 0, 0, 0.0

    sample_keys = list(freq_counter.keys())[:5] if len(freq_counter) >= 5 else list(freq_counter.keys())
    sample_counts = [freq_counter[k] for k in sample_keys]

    print(f"\n--- Статистика {description} ---")
    if stemmer is None: 
        print("Токенизатор:")
        print(f"  Всего токенов (после фильтрации): {total_tokens_or_stems}")
        print(f"  Уникальных токенов: {unique_tokens_or_stems}")
        print(f"  Время выполнения токенизации (всего): {overall_time:.2f} сек")
        if total_tokens_docs > 0:
            print(f"  Скорость токенизации (документов/сек): {total_tokens_docs / overall_time:.2f}")
        print(f"  Средняя длина токена: {avg_len:.2f}")
        print(f"  Самый длинный токен: {max_len}")
        print(f"  Самый короткий токен: {min_len}")
        print(f"  Примеры токенов (токен: частота): {dict(zip(sample_keys, sample_counts))}")
    else: 
        print("\nСтеммер:")
        print(f"  Всего стемов (после фильтрации и стемминга): {total_tokens_or_stems}")
        print(f"  Уникальных стемов: {unique_tokens_or_stems}")
        print(f"  Время выполнения стемминга (всего): {overall_time:.2f} сек") 

        all_unique_original_tokens = set()
        all_unique_stems_from_tokens = set()
        changed_unique_tokens_count = 0
        for orig_tok, stems_set in original_tokens_for_stemming_example.items():
            all_unique_original_tokens.add(orig_tok)
            for stem_val in stems_set:
                all_unique_stems_from_tokens.add(stem_val)
                if orig_tok != stem_val:
                    changed_unique_tokens_count += 1

        total_unique_tokens_before_stemming = len(all_unique_original_tokens)
        percentage_changed = (changed_unique_tokens_count / total_unique_tokens_before_stemming * 100) if total_unique_tokens_before_stemming > 0 else 0.0
        print(f"  Процент уникальных измененных токенов (оценка): {percentage_changed:.2f}%")

        print("  Примеры объединения словоформ (стем: {оригиналы}):")
        examples_printed = 0
        for stem, originals in sorted(stem_to_original_map.items(), key=lambda x: len(x[1]), reverse=True):
            if len(originals) > 1 and examples_printed < 5:
                print(f"    {stem}: {list(originals)[:5]}") 
                examples_printed += 1
        if examples_printed == 0:
             for i, (stem, originals) in enumerate(stem_to_original_map.items()):
                 if i >= 5: break
                 print(f"    {stem}: {list(originals)[:5]}")


def main():
    collection = connect_to_db()
    tokenizer = py_inf_search.MedicalTokenizer()
    stemmer = py_inf_search.RussianStemmer()
    analyze_and_print_stats(collection, tokenizer, stemmer=None, description="Токенизация")
    analyze_and_print_stats(collection, tokenizer, stemmer=stemmer, description="Токенизация + Стемминг")


if __name__ == '__main__':
    main()