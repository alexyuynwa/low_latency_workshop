set (EXTRA_LIBS pthread)

add_executable(dictionary
    dictionary.cc
    util.hh
    ./spellchecker/tokenizer.hh
    ./v0/dictionary.hh
    ./v1/dictionary.hh
    ./v2/dictionary.hh
    ./v3/dictionary.hh
    ./v4/dictionary.hh
    ./v5/dictionary.hh
    ./string_view.hh
)

target_include_directories(dictionary SYSTEM PRIVATE "ext")

target_link_libraries(dictionary
    PRIVATE
    benchmark::benchmark
    absl::flat_hash_set
    absl::node_hash_set
    ${EXTRA_LIBS}
)
