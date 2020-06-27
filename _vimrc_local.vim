let g:ale_linter_aliases = {'h': 'c', 'hpp': 'cpp', 'cc': 'cpp'}

let g:ale_cpp_gcc_options = "-std=c++17 -Wno-c++11-extensions"
let g:ale_cpp_clang_options = "-std=c++17 -Wno-c++11-extensions"

let g:ale_cpp_clangtidy_options = '-Wall -std=c++17 -x c++'
let g:ale_cpp_clangcheck_options = '-- -Wall -std=c++17 -x c++'
