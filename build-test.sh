#/opt/homebrew/opt/llvm/bin/clang --target=riscv32 -march=rv32i -fuse-ld=lld -nostdlib -c test.c -o test.o
#/opt/homebrew/opt/llvm/bin/clang --target=riscv32 -march=rv32i -fuse-ld=lld -nostdlib test.o -o test
/opt/homebrew/opt/llvm/bin/clang --target=riscv32 -march=rv32i -fuse-ld=lld -nostdlib -o0 -g test.c -o test.elf
/opt/homebrew/opt/llvm/bin/llvm-objcopy -O binary -j .text test.elf test.bin
#/opt/homebrew/opt/llvm/bin/llvm-objdump -S -d -Mno-aliases test

#/opt/homebrew/opt/llvm/bin/llvm-objcopy -I binary -B riscv32 --rename-section=.data=.text,code test.bin test.bin.elf
#/opt/homebrew/opt/llvm/bin/llvm-objcopy -I binary -B riscv32 test.bin test.bin.elf
#/opt/homebrew/opt/llvm/bin/llvm-objcopy -I binary -B rv32i -j .text test.bin test.bin.elf
/opt/homebrew/opt/llvm/bin/llvm-objdump -d test.elf
#/opt/homebrew/opt/llvm/bin/llvm-objdump -S -D test.elf > test.elf.disam
