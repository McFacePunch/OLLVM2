rm a.out

clang++ -g -O0 -std=c++11 -I ./build/llvm/include -L ./build/llvm/lib -o test_program standalone.cpp `llvm-config --cxxflags --ldflags --libs core` || exit

clang -emit-llvm -c hello.c -o hello.bc -g -static || exit 

llvm-dis hello.bc -o hello.bc.ll  || exit

./test_program hello.bc.ll -o transform.ll || exit 

clang transform.ll || exit

echo "++++++++++++++++++++++++++++++++++\nBelow is strings+grep output\n"

strings a.out | grep -i hello 
strings a.out | grep -i age 
strings a.out | grep -i greet 


echo "==============================\nBelow is running a.out\n" 
./a.out
