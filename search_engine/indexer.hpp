#ifndef INDEXER_HPP
#define INDEXER_HPP

#include "vector.hpp"
#include "hashmap.hpp"
#include <string>
#include <chrono>
#include <fstream> 

struct InputDocument {
    uint32_t id;
    std::string url;
    std::string text;
};

class BooleanIndexer {
private:
    HashMap<std::string, Vector<uint32_t>> inverted_index_;
    Vector<InputDocument> forward_index_docs_;

    size_t num_docs_processed_ = 0;
    size_t num_unique_terms_ = 0;
    std::chrono::high_resolution_clock::time_point start_time_build_;
    std::chrono::high_resolution_clock::time_point start_time_write_inv_;
    std::chrono::high_resolution_clock::time_point start_time_write_fwd_;

    std::string inverted_index_filename_ = "inverted_index.bin";
    std::string forward_index_filename_ = "forward_index.bin";

    void write_forward_index_to_disk();
    void write_inverted_index_to_disk();

public:
    BooleanIndexer() = default;
    ~BooleanIndexer() = default;

    void build_index(int max_docs = -1, bool use_stemming = false);

    void print_statistics(bool use_stemming) const; 
};

#endif