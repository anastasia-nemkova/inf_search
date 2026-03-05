from setuptools import setup, Extension
from pybind11.setup_helpers import Pybind11Extension, build_ext
from pybind11 import get_cmake_dir
import os

tokenizer_cpp_path = "../lingvistica/tokenizer/tokenizer.cpp"
stemmer_cpp_path = "../lingvistica/stemmer/stemmer.cpp"

ext_modules = [
    Pybind11Extension(
        "py_inf_search",
        ["py_inf_search.cpp", tokenizer_cpp_path, stemmer_cpp_path],
        include_dirs=[
            "../lingvistica/tokenizer",
            "../lingvistica/stemmer",
        ],
        language='c++',
        cxx_std=14,
    ),
]

setup(
    name="py_inf_search",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
    python_requires=">=3.6",
)