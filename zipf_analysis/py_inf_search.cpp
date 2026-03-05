#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "../lingvistica/tokenizer/tokenizer.hpp"
#include "../lingvistica/stemmer/stemmer.hpp"

namespace py = pybind11;

PYBIND11_MODULE(py_inf_search, m) {
    py::class_<inf_search::MedicalTokenizer>(m, "MedicalTokenizer")
        .def(py::init<>())
        .def("tokenize", py::overload_cast<const std::string&>(&inf_search::MedicalTokenizer::tokenize));

    py::class_<RussianStemmer>(m, "RussianStemmer")
        .def(py::init<>())
        .def("stem", &RussianStemmer::stem);
}