#ifndef SEARCH_ENGINE_HPP
#define SEARCH_ENGINE_HPP

#include "vector.hpp" 
#include "hashmap.hpp"
#include "../lingvistica/tokenizer/tokenizer.hpp"
#include "../lingvistica/stemmer/stemmer.hpp"
#include <string>
#include <utility>

struct DocumentMetadata {
    std::string url;
    DocumentMetadata() = default;
    DocumentMetadata(const std::string& u) : url(u) {}
};

class SearchEngine {
private:
    inf_search::MedicalTokenizer tokenizer_;
    Vector<DocumentMetadata> forward_index_;
    HashMap<std::string, Vector<uint32_t>> inverted_index_;

    bool load_forward_index(const std::string& filename);
    bool load_inverted_index(const std::string& filename);

    Vector<uint32_t> intersect_posting_lists(const Vector<uint32_t>& list1, const Vector<uint32_t>& list2) const;
    Vector<uint32_t> union_posting_lists(const Vector<uint32_t>& list1, const Vector<uint32_t>& list2) const;
    Vector<uint32_t> subtract_posting_lists(const Vector<uint32_t>& list1, const Vector<uint32_t>& list2) const;

public:
    SearchEngine() = default;
    ~SearchEngine() = default;

    bool load_indexes(const std::string& forward_filename, const std::string& inverted_filename);

    Vector<uint32_t> search(const std::string& query);
    DocumentMetadata get_document_metadata(uint32_t doc_id) const;
};

#endif