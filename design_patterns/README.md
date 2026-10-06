-   How to use 

    -   design_patterns

        -   Install google benchmark and google test

            $ cd design_patterns
            $ git clone https://github.com/google/benchmark google_benchmark
            $ cd google_benchmark
            $ git clone https://github.com/google/googletest

        -   build

            $ cd design_patterns
            $ mkdir build
            $ cd build
            $ cmake ../
            (Note: To force using clang++ as the compiler, please run: 
            $ cmake -DCMAKE_CXX_COMPILER=clang++ ../ )
            $ make

