#/opt/homebrew/opt/llvm/bin/clang --target=riscv32 -march=rv32i -fuse-ld=lld -nostdlib -c test.c -o test.o
#/opt/homebrew/opt/llvm/bin/clang --target=riscv32 -march=rv32i -fuse-ld=lld -nostdlib test.o -o test
/opt/homebrew/opt/llvm/bin/clang --target=riscv32 -march=rv32i -fuse-ld=lld -nostdlib -g test.c -o test
/opt/homebrew/opt/llvm/bin/llvm-objcopy -O binary test test.bin
/opt/homebrew/opt/llvm/bin/llvm-objdump -S -d test
