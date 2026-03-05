#include "indexer.hpp"
#include "../lingvistica/tokenizer/tokenizer.hpp"
#include "../lingvistica/stemmer/stemmer.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <string>

bool load_data_from_txt(const std::string& text_filename, const std::string& url_filename, Vector<InputDocument>& documents, int max_docs = -1) {
    std::ifstream file_text(text_filename);
    std::ifstream file_url(url_filename);

    if (!file_text.is_open()) {
        std::cerr << "Error: Could not open text file " << text_filename << "\n";
        return false;
    }
    if (!file_url.is_open()) {
        std::cerr << "Error: Could not open URL file " << url_filename << "\n";
        file_text.close();
        return false;
    }

    std::string line_text;
    std::string line_url;
    uint32_t current_doc_id = 0;

    while (std::getline(file_text, line_text) && std::getline(file_url, line_url)) {
        if (max_docs >= 0 && current_doc_id >= static_cast<uint32_t>(max_docs)) {
            break;
        }
        InputDocument input_doc;
        input_doc.id = current_doc_id;
        input_doc.url = line_url;
        input_doc.text = line_text;
        documents.push_back(std::move(input_doc));

        current_doc_id++;
    }

    file_text.close();
    file_url.close();

    std::cout << "Loaded " << documents.size() << " documents from text files.\n";
    return true;
}

void BooleanIndexer::build_index(int max_docs, bool use_stemming) {
    std::cout << "Starting Index Building...\n";
    std::cout << "Mode: " << (use_stemming ? "Tokens + Stems" : "Tokens only") << "\n";

    start_time_build_ = std::chrono::high_resolution_clock::now();

    std::string text_filename = "texts/text.txt";
    std::string url_filename = "texts/url.txt";  

    if (!load_data_from_txt(text_filename, url_filename, forward_index_docs_, max_docs)) {
        std::cerr << "Failed to load data from text files. Aborting.\n";
        return;
    }
    num_docs_processed_ = forward_index_docs_.size();
    std::cout << "Initializing tokenizer and stemmer...\n";
    inf_search::MedicalTokenizer tokenizer;
    RussianStemmer stemmer;
    std::cout << "Building index from loaded data...\n";

    for (size_t idx = 0; idx < forward_index_docs_.size(); ++idx) {
        const auto& doc = forward_index_docs_[idx];
        const std::string& text = doc.text;

        std::vector<std::string> tokens = tokenizer.tokenize(text);
        for (const std::string& token : tokens) {
            if (!token.empty()) {
                 std::string processed_term = use_stemming ? stemmer.stem(token) : token;
                 if (!processed_term.empty()) {
                    inverted_index_[processed_term].push_back(doc.id);
                 }
            }
        }

        if ((idx + 1) % 10000 == 0) {
            std::cout << "Processed " << (idx + 1) << " documents...\n";
        }
    }

    std::cout << "Finished processing documents. Total: " << num_docs_processed_ << "\n";

    std::cout << "Finalizing posting lists (sorting and deduplication)...\n";
    Vector<std::string> all_terms;
    Vector<Vector<uint32_t>> all_posting_lists;
    inverted_index_.get_all_pairs(all_terms, all_posting_lists);

    for (auto& plist : all_posting_lists) {
        plist.sort();
        plist.remove_dupl();
    }
    num_unique_terms_ = all_terms.size();

    start_time_write_fwd_ = std::chrono::high_resolution_clock::now();
    write_forward_index_to_disk();
    auto end_time_write_fwd = std::chrono::high_resolution_clock::now();
    auto duration_write_fwd_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_write_fwd - start_time_write_fwd_);
    double duration_write_fwd_sec = duration_write_fwd_ms.count() / 1000.0;
    std::cout << "Forward index written in " << duration_write_fwd_sec << " seconds.\n";

    start_time_write_inv_ = std::chrono::high_resolution_clock::now();

    HashMap<std::string, size_t> term_to_idx;
    for (size_t i = 0; i < all_terms.size(); ++i) {
        term_to_idx[all_terms[i]] = i;
    }

    Vector<std::string> sorted_terms = all_terms;
    sorted_terms.sort();

    std::ofstream inv_file(inverted_index_filename_, std::ios::binary);
    if (!inv_file.is_open()) {
        std::cerr << "Error: Could not open " << inverted_index_filename_ << " for writing!\n";
        return;
    }

    uint32_t num_terms = static_cast<uint32_t>(all_terms.size());
    inv_file.write(reinterpret_cast<const char*>(&num_terms), sizeof(num_terms));

    std::cout << "Writing inverted index with " << num_terms << " terms...\n";
    for (size_t i = 0; i < sorted_terms.size(); ++i) {
        const std::string& term = sorted_terms[i];

        auto* ptr = term_to_idx.find(term);
        if (!ptr) continue;

        const Vector<uint32_t>& plist = all_posting_lists[*ptr];

        uint32_t term_len = static_cast<uint32_t>(term.size());
        uint32_t plist_len = static_cast<uint32_t>(plist.size());
        uint32_t opt_term_data_len = 0;

        inv_file.write(reinterpret_cast<const char*>(&term_len), sizeof(term_len));
        inv_file.write(term.data(), term_len);
        inv_file.write(reinterpret_cast<const char*>(&plist_len), sizeof(plist_len));

        if (plist_len > 0) {
            inv_file.write(reinterpret_cast<const char*>(plist.begin()), 
                           plist_len * sizeof(uint32_t));
        }

        inv_file.write(reinterpret_cast<const char*>(&opt_term_data_len), sizeof(opt_term_data_len));

        if ((i + 1) % 1000 == 0 || i == sorted_terms.size() - 1) {
            std::cout << "Written " << (i + 1) << " / " << sorted_terms.size() << " terms...\n";
        }
    }
    inv_file.close();

    auto end_time_write_inv = std::chrono::high_resolution_clock::now();
    auto duration_write_inv_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_write_inv - start_time_write_inv_);
    double duration_write_inv_sec = duration_write_inv_ms.count() / 1000.0;
    std::cout << "Inverted index written in " << duration_write_inv_sec << " seconds.\n";

    auto end_time_build = std::chrono::high_resolution_clock::now();
    auto duration_build_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_build - start_time_build_);
    double duration_build_total_sec = duration_build_ms.count() / 1000.0;

    std::cout << "Index building completed in " << duration_build_total_sec << " seconds.\n";
}

void BooleanIndexer::write_forward_index_to_disk() {
    std::ofstream fwd_file(forward_index_filename_, std::ios::binary);
    if (!fwd_file.is_open()) {
        std::cerr << "Error: Could not open " << forward_index_filename_ << " for writing!\n";
        return;
    }

    uint32_t num_docs = static_cast<uint32_t>(forward_index_docs_.size());
    fwd_file.write(reinterpret_cast<const char*>(&num_docs), sizeof(num_docs));

    for (const auto& doc : forward_index_docs_) {
        uint32_t doc_id = doc.id;
        uint32_t url_len = static_cast<uint32_t>(doc.url.size());

        fwd_file.write(reinterpret_cast<const char*>(&doc_id), sizeof(doc_id));
        fwd_file.write(reinterpret_cast<const char*>(&url_len), sizeof(url_len));
        fwd_file.write(doc.url.data(), url_len);

        uint32_t opt_field_len = 0;
        fwd_file.write(reinterpret_cast<const char*>(&opt_field_len), sizeof(opt_field_len));
    }
    fwd_file.close();
}

void BooleanIndexer::print_statistics(bool use_stemming) const {
    auto end_time_build = std::chrono::high_resolution_clock::now();
    auto duration_build_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_build - start_time_build_);
    double duration_build_total_sec = duration_build_ms.count() / 1000.0;

    std::ifstream inv_check(inverted_index_filename_, std::ios::ate | std::ios::binary);
    std::ifstream fwd_check(forward_index_filename_, std::ios::ate | std::ios::binary);
    size_t inv_size = inv_check.tellg();
    size_t fwd_size = fwd_check.tellg();
    inv_check.close();
    fwd_check.close();

    double inv_size_mb = static_cast<double>(inv_size) / (1024 * 1024);
    double fwd_size_mb = static_cast<double>(fwd_size) / (1024 * 1024);

    std::string mode_desc = use_stemming ? "by stems:" : "by tokens:";

    std::cout << "\n--- Indexing Summary " << mode_desc << " ---\n";
    std::cout << "Documents processed: " << num_docs_processed_ << "\n";
    std::cout << "Unique terms: " << num_unique_terms_ << "\n";
    std::cout << "Time: " << duration_build_total_sec << " seconds.\n";
    std::cout << "Volume: " << inv_size_mb << " MB\n";
    std::cout << "Files created: " << inverted_index_filename_ << ", " << forward_index_filename_ << "\n";
    std::cout << "------------------------\n";
}