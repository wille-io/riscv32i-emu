clear && clang -fpic -std=c99 -g main.c -o main && ./build-test.sh && read && lldb ./main
