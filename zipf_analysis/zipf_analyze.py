import matplotlib
matplotlib.use('Agg')

import matplotlib.pyplot as plt
import numpy as np
from collections import Counter
from pymongo import MongoClient

try:
    import py_inf_search
    USE_CPP_MODULES = True
except ImportError as e:
    exit(1)


def connect_to_db():
    host = 'localhost'
    port = 27017
    database_name = 'inf_search'
    collection_name = 'documents'

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
    cursor = collection.find({}, {'clean_text': 1, '_id': 0})
    count = 0
    for doc in cursor:
        text = doc.get('clean_text')
        if text:
            yield text
            count += 1
            if count % 5000 == 0:
                print(f"  Обработано {count} документов...")
    print(f"Всего извлечено {count} документов с текстом.")

def analyze_zipf(collection, tokenizer, stemmer=None, description=""):

    print(f"Токенизация и подсчет частот {description}...")
    freq_counter = Counter()
    doc_count = 0

    for text in fetch_all_texts_generator(collection):
        doc_count += 1
        if not isinstance(text, str):
            continue
        
        tokens = tokenizer.tokenize(text)
        if stemmer:
            valid_tokens = [token for token in tokens if token.isalpha() or token.isdigit()]
            stemmed_tokens = [stemmer.stem(token) for token in valid_tokens if token.strip()]
            freq_counter.update(stemmed_tokens)
        else:
            valid_tokens = [token for token in tokens if token.isalpha() or token.isdigit()]
            freq_counter.update(valid_tokens)
        if doc_count % 5000 == 0:
            print(f"  Проанализировано {doc_count} документов для {description}...")

    if not freq_counter:
        print(f"После токенизации и фильтрации не осталось токенов для {description}.")
        return [], [], 0

    print(f"Количество уникальных токенов {description}: {len(freq_counter)}")

    sorted_items = freq_counter.most_common()
    sorted_freqs = [item[1] for item in sorted_items]
    ranks = list(range(1, len(sorted_freqs) + 1))

    total_tokens = sum(sorted_freqs)

    return ranks, sorted_freqs, total_tokens

def calculate_zipf_r_squared(ranks, freqs):
    """
    Рассчитывает R^2 для соответствия данным Закона Ципфа (f = c/r^s, s=1).
    Использует логарифмический масштаб: log(f) = log(c) - s*log(r).
    """
    if not ranks or not freqs or len(ranks) != len(freqs):
        print("Недостаточно данных для расчета R^2.")
        return float('nan')

    log_ranks = []
    log_freqs_observed = []
    for r, f in zip(ranks, freqs):
        if r > 0 and f > 0:
             log_ranks.append(np.log(r))
             log_freqs_observed.append(np.log(f))
    
    if len(log_ranks) < 2:
        print("Недостаточно положительных значений для расчета R^2.")
        return float('nan')

    log_ranks = np.array(log_ranks)
    log_freqs_observed = np.array(log_freqs_observed)

    log_c = np.mean(log_freqs_observed + log_ranks)
    log_freqs_theoretical = log_c - log_ranks

    ss_res = np.sum((log_freqs_observed - log_freqs_theoretical) ** 2)
    ss_tot = np.sum((log_freqs_observed - np.mean(log_freqs_observed)) ** 2)

    if ss_tot == 0:
        if ss_res == 0:
            r_squared = 1.0
        else:
            r_squared = 0.0
    else:
        r_squared = 1 - (ss_res / ss_tot)

    return r_squared

def plot_single_zipf(ranks, freqs, title, output_file):

    r_squared = calculate_zipf_r_squared(ranks, freqs)
    print(f" Коэффициент аппроксимации: {r_squared:.4f}")

    fig, ax = plt.subplots(figsize=(10, 6))

    if ranks and freqs:
        ax.loglog(ranks, freqs, marker='o', linestyle='', markersize=3, label='Частоты корпуса', alpha=0.7)
        if freqs:
             c_theory = freqs[0]
             s_theory = 1.0
             theoretical_freqs = [c_theory / (r ** s_theory) for r in ranks]
             ax.loglog(ranks, theoretical_freqs, linestyle='--', color='red', label='Теоретический Закон Ципфа', alpha=0.8)

    ax.set_xlabel('Ранг (лог. шкала)')
    ax.set_ylabel('Частота (лог. шкала)')
    ax.set_title(title)
    ax.legend()
    ax.grid(True, which="both", ls="-", alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')

def main():
    collection = connect_to_db()
    tokenizer = py_inf_search.MedicalTokenizer()
    stemmer = py_inf_search.RussianStemmer()


    ranks_no_stem, freqs_no_stem, total_no_stem = analyze_zipf(
        collection, tokenizer, stemmer=None, description="Without Stemming"
    )

    if not ranks_no_stem:
         print("Не удалось получить данные для построения графика без стемминга.")
    else:

        plot_single_zipf(ranks_no_stem, freqs_no_stem, "Распределение терминов (без стемминга)", "zipf_law_without_stemming.png")



    ranks_stem, freqs_stem, total_stem = analyze_zipf(
        collection, tokenizer, stemmer=stemmer, description="With Stemming"
    )

    if not ranks_stem:
         print("Не удалось получить данные для построения графика со стеммингом.")
    else:
        plot_single_zipf(ranks_stem, freqs_stem, "Распределение терминов (со стеммингом)", "zipf_law_with_stemming.png")

    
if __name__ == '__main__':
    main()