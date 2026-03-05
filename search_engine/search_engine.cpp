#include "search_engine.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <locale>
#include <chrono>
#include <iomanip>

bool SearchEngine::load_forward_index(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open forward index file: " << filename << std::endl;
        return false;
    }

    uint32_t num_docs;
    file.read(reinterpret_cast<char*>(&num_docs), sizeof(num_docs));

    forward_index_.clear();
    forward_index_.resize(num_docs);

    for (uint32_t i = 0; i < num_docs; ++i) {
        uint32_t doc_id;
        uint32_t url_len;
        file.read(reinterpret_cast<char*>(&doc_id), sizeof(doc_id));
        file.read(reinterpret_cast<char*>(&url_len), sizeof(url_len));

        if (doc_id != i) {
            std::cerr << "Warning: Document ID mismatch in forward index at position " << i << ". Expected " << i << ", got " << doc_id << std::endl;
        }

        std::string url(url_len, '\0');
        file.read(&url[0], url_len);
        uint32_t opt_field_len;
        file.read(reinterpret_cast<char*>(&opt_field_len), sizeof(opt_field_len));
        if (opt_field_len > 0) {
            file.seekg(opt_field_len, std::ios_base::cur);
        }

        if (i < forward_index_.size()) {
            forward_index_[i] = DocumentMetadata(url);
        }
    }

    file.close();
    std::cout << "Loaded " << forward_index_.size() << " documents into forward index." << std::endl;
    return true;
}

bool SearchEngine::load_inverted_index(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open inverted index file: " << filename << std::endl;
        return false;
    }

    uint32_t num_terms;
    file.read(reinterpret_cast<char*>(&num_terms), sizeof(num_terms));

    for (uint32_t i = 0; i < num_terms; ++i) {
        uint32_t term_len;
        file.read(reinterpret_cast<char*>(&term_len), sizeof(term_len));

        std::string term(term_len, '\0');
        file.read(&term[0], term_len);

        uint32_t plist_len;
        file.read(reinterpret_cast<char*>(&plist_len), sizeof(plist_len));

        Vector<uint32_t> posting_list;
        posting_list.resize(plist_len);
        for (uint32_t j = 0; j < plist_len; ++j) {
            uint32_t doc_id;
            file.read(reinterpret_cast<char*>(&doc_id), sizeof(doc_id));
            posting_list[j] = doc_id;
        }

        uint32_t opt_term_data_len;
        file.read(reinterpret_cast<char*>(&opt_term_data_len), sizeof(opt_term_data_len));
        if (opt_term_data_len > 0) {
            file.seekg(opt_term_data_len, std::ios_base::cur);
        }

        inverted_index_[term] = std::move(posting_list);
    }

    file.close();
    std::cout << "Loaded " << inverted_index_.size() << " terms into inverted index." << std::endl;
    return true;
}

bool SearchEngine::load_indexes(const std::string& forward_filename, const std::string& inverted_filename) {
    if (!load_forward_index(forward_filename)) {
        std::cerr << "Failed to load forward index." << std::endl;
        return false;
    }
    if (!load_inverted_index(inverted_filename)) {
        std::cerr << "Failed to load inverted index." << std::endl;
        return false;
    }
    std::cout << "Indexes loaded successfully." << std::endl;
    return true;
}

Vector<uint32_t> SearchEngine::intersect_posting_lists(const Vector<uint32_t>& list1, const Vector<uint32_t>& list2) const {
    Vector<uint32_t> result;
    size_t i = 0, j = 0;

    while (i < list1.size() && j < list2.size()) {
        if (list1[i] == list2[j]) {
            result.push_back(list1[i]);
            i++;
            j++;
        } else if (list1[i] < list2[j]) {
            i++;
        } else {
            j++;
        }
    }
    return result;
}

Vector<uint32_t> SearchEngine::union_posting_lists(const Vector<uint32_t>& list1, const Vector<uint32_t>& list2) const {
    Vector<uint32_t> result;
    size_t i = 0, j = 0;

    while (i < list1.size() && j < list2.size()) {
        if (list1[i] == list2[j]) {
            result.push_back(list1[i]);
            i++;
            j++;
        } else if (list1[i] < list2[j]) {
            result.push_back(list1[i]);
            i++;
        } else {
            result.push_back(list2[j]);
            j++;
        }
    }

    while (i < list1.size()) {
        result.push_back(list1[i]);
        i++;
    }
    while (j < list2.size()) {
        result.push_back(list2[j]);
        j++;
    }
    return result;
}

Vector<uint32_t> SearchEngine::subtract_posting_lists(const Vector<uint32_t>& list1, const Vector<uint32_t>& list2) const {
    Vector<uint32_t> result;
    size_t i = 0, j = 0;

    while (i < list1.size()) {
        if (j >= list2.size() || list1[i] < list2[j]) {
            result.push_back(list1[i]);
            i++;
        } else if (list1[i] == list2[j]) {
            i++;
            j++;
        } else { 
            j++;
        }
    }
    return result;
}


Vector<uint32_t> SearchEngine::search(const std::string& query) {
    auto start_time = std::chrono::high_resolution_clock::now();

    Vector<std::string> tokens = tokenizer_.tokenize(query);
    Vector<uint32_t> results;

    if (tokens.empty()) {
        results = Vector<uint32_t>();
    }
    else if (tokens.size() == 1) {
        const std::string& term = tokens[0];
        auto* ptr = inverted_index_.find(term);
        if (ptr) {
            results = *ptr;
        }
    }
    else if (tokens.size() == 3) {
        std::string op = tokens[1];
        std::transform(op.begin(), op.end(), op.begin(), ::toupper);

        Vector<uint32_t> list1, list2;

        auto* ptr1 = inverted_index_.find(tokens[0]);
        if (ptr1) list1 = *ptr1;

        auto* ptr2 = inverted_index_.find(tokens[2]);
        if (ptr2) list2 = *ptr2;

        if (op == "AND") {
            results = intersect_posting_lists(list1, list2);
        } else if (op == "OR") {
            results = union_posting_lists(list1, list2);
        } else if (op == "NOT") {
            results = subtract_posting_lists(list1, list2);
        } else {
            std::cerr << "Unknown operator: " << op << std::endl;
            results = Vector<uint32_t>();
        }
    }
    else {
        std::cerr << "Unsupported query format: " << query << std::endl;
        results = Vector<uint32_t>();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    double ms = duration.count() / 1000.0;

    std::cout << "Search time: " << std::fixed << std::setprecision(2) << ms << " ms\n";

    return results;
}

DocumentMetadata SearchEngine::get_document_metadata(uint32_t doc_id) const {
    if (doc_id < forward_index_.size()) {
        return forward_index_[doc_id];
    } else {
        std::cerr << "Warning: Requested metadata for non-existent document ID: " << doc_id << std::endl;
        return DocumentMetadata();
    }
}