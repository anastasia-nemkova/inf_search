#include "indexer.hpp"
#include "search_engine.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>

int main(int argc, char* argv[]) {
    std::string mode = "index";
    bool use_stemming = false;
    int max_docs = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--stem") {
            use_stemming = true;
        } else if (arg == "--index") {
            mode = "index";
        } else if (arg == "--search") {
            mode = "search";
        } else {
            try {
                max_docs = std::stoi(arg);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Invalid argument '" << arg << "', skipping.\n";
            }
        }
    }

    if (mode == "index") {
        BooleanIndexer indexer;
        indexer.build_index(max_docs, use_stemming);
        indexer.print_statistics(use_stemming);
    } else if (mode == "search") {
        SearchEngine engine;
        if (!engine.load_indexes("forward_index.bin", "inverted_index.bin")) {
             std::cerr << "Failed to load indexes for searching.\n";
             return -1;
        }

        std::cout << "\nSearch Engine loaded. Enter queries (type 'exit' to quit):\n";
        std::cout << "Supported queries: 'single_term', 'term1 AND term2', 'term1 OR term2', 'term1 NOT term2'\n";
        std::string query;
        while (true) {
            std::cout << "> ";
            std::getline(std::cin, query);
            if (query == "exit") break;

            query.erase(0, query.find_first_not_of(" \t"));
            query.erase(query.find_last_not_of(" \t") + 1);

            if (query.empty()) continue;
            Vector<uint32_t> results = engine.search(query); 
            std::cout << "Found " << results.size() << " document(s):\n";
            for (size_t i = 0; i < results.size(); ++i) {
                uint32_t doc_id = results[i];
                DocumentMetadata meta = engine.get_document_metadata(doc_id);
                std::cout << "  " << i + 1 << ". ID: " << doc_id << ", URL: " << meta.url << std::endl;
            }
            if (results.size() == 0) {
                std::cout << "  No documents matched the query.\n";
            }
        }
    } else {
        std::cerr << "Unknown mode. Use --index or --search.\n";
        return -1;
    }

    return 0;
}