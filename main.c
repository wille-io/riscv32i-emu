#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>


#define MAX_MEM_BLOCKS 10000000
uint32_t mem_ptr = 0;
uint32_t mem_index[MAX_MEM_BLOCKS];
uint8_t mem_block[MAX_MEM_BLOCKS];

int silent = 0;


typedef struct 
{
  uint32_t regs[32];
  uint32_t pc;
} cpu_t;


uint8_t mem_read_8(uint32_t addr)
{
  for (uint32_t i = 0; i < mem_ptr; i++)
  {
    if (mem_index[i] == addr)
    {
      uint8_t val = mem_block[i];
      if (!silent)
        printf("mem_read_8: addr = 0x%"PRIx32", val = %"PRIu32"\n", addr, val);
      return val;
    }
  }

  fprintf(stderr, "!!! mem_read_8: no mem block found for addr 0x%"PRIx32"! aka. 'segmentation' violation\n", addr);
  abort();
  //return 0xFF;
}


uint16_t mem_read_16(uint32_t addr)
{
  for (uint32_t i = 0; i < mem_ptr; i++)
  {
    if (mem_index[i] != addr)
      continue;
    
    if ((i + 1) > mem_ptr)
    {
      fprintf(stderr, "!!! mem_read_16: tried reading past mem_ptr\n");
      abort();
      //return 0xFFFF;
    }

    uint16_t val = mem_block[i] | mem_block[i+1] << 8;
    if (!silent)
      printf("mem_read_16: addr = 0x%"PRIx32", val = %"PRIu16" (%02"PRIx8" %02"PRIx8")\n", addr, val, mem_block[i], mem_block[i+1]);
    return val;
  }

  fprintf(stderr, "!!! mem_read_16: no mem block found for addr 0x%"PRIx32"! aka. 'segmentation' violation\n", addr);
  abort();
  //return 0xFFFF;
}


uint32_t mem_read_32(uint32_t addr)
{
  for (uint32_t i = 0; i < mem_ptr; i++)
  {
    if (mem_index[i] != addr)
      continue;
    
    if ((i + 3) > mem_ptr)
    {
      fprintf(stderr, "!!! mem_read_32: tried reading past mem_ptr\n");
      abort();
      //return 0xFFFFFFFF;
    }

    uint32_t val = mem_block[i] | mem_block[i+1] << 8 | mem_block[i+2] << 16 | mem_block[i+3] << 24;
    if (!silent)
      printf("mem_read_32: addr = 0x%"PRIx32", val = %"PRIu32" (%02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8")\n", addr, val, mem_block[i], mem_block[i+1], mem_block[i+2], mem_block[i+3]);
    return val;
  }

  fprintf(stderr, "!!! mem_read_32: no mem block found for addr 0x%"PRIx32"! aka. 'segmentation' violation\n", addr);
  abort();
  //return 0xFFFFFFFF;
}


uint32_t code_mem_ptr = 0; // initial writes before the program was written are allowed to write everything
uint8_t abort_next = 0;


void mem_write_8(uint32_t addr, uint8_t val)
{
  if (addr < code_mem_ptr)
  {
    fprintf(stderr, "!!! tried to write to code address @ 0x%"PRIx32" (mem_ptr = %"PRIx32") \n", addr, code_mem_ptr);
    //abort();
    abort_next = 1;
  }

  if (mem_ptr >= MAX_MEM_BLOCKS)
  {
    fprintf(stderr, "!!! starved of mem blocks!\n");
    abort();
  }

  // if (addr == 9666)
  //   puts("YAY !!!!!!!!! 9666 !!!!!!!!!");

  // look for existing mem_block for addr
  uint8_t found = 0;
  //puts("searching ...");
  for (uint32_t i = 0; i < mem_ptr; i++)
  {
    if (mem_index[i] == addr)
    {
      //puts("  > FOUND!");
      mem_block[i] = val;
      found = 1;
      break;
    }
  }
  //puts("searching done!");

  if (!found)
  {
    mem_index[mem_ptr] = addr;
    mem_block[mem_ptr] = val;
    mem_ptr++;
  }

  if (silent)
    return;

  printf("mem_write_8: wrote val %"PRIu8" to addr 0x%"PRIx32"\n", val, addr);
}


void mem_write_8_always_add_memblock(uint32_t addr, uint8_t val)
{
  if (mem_ptr >= MAX_MEM_BLOCKS)
  {
    fprintf(stderr, "!!! starved of mem blocks!\n");
    abort();
  }

  mem_index[mem_ptr] = addr;
  mem_block[mem_ptr] = val;
  mem_ptr++;

  if (silent)
    return;

  printf("mem_write_8_always_add_memblock: wrote val %"PRIu8" to addr 0x%"PRIx32"\n", val, addr);
}


void mem_write_32(uint32_t addr, uint32_t val)
{
  int old_silence = silent;
  silent = 1;
  mem_write_8(addr, val);
  mem_write_8(addr+1, val >> 8); // TODO: skip bits or ... ?
  mem_write_8(addr+2, val >> 16);
  mem_write_8(addr+3, val >> 24);
  silent = old_silence;

  if (silent)
    return;
  printf("mem_write_32: wrote val %"PRIu32" to addr 0x%"PRIx32"\n", val, addr);
}


// void meminit(void *adr, int size)
// {
//   for (int i = 0; i < size; i++)
//     ((char *)adr)[i] = 0;
// }


const char *_r2s[] = { "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0", "a1", "a2", "a7", "s2" };


const char *r2s(uint8_t reg) // register to string
{
  if (reg > 14)
  {
    abort_next = 1;
    fprintf(stderr, "!!! r2s: unknown register %"PRIu8"\n", reg);
    return "???";
  }
  return _r2s[reg];
}


uint32_t stop_after_instructions = 50;
uint32_t inst_count;


int main(int argc, char **argv)
{
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <binary file>\n", argv[0]);
    return -1;
  }
  fprintf(stderr, "binary file: %s\n", argv[1]);

  cpu_t cpu;
  //meminit(cpu, sizeof(cpu));
  uint32_t cpu_size = sizeof(cpu_t);
  memset(&cpu, 0, cpu_size);


  //printf("cpu size = %"PRIu32"\n", cpu_size);

  FILE *f = fopen(argv[1], "r");

  if (!f)
    return -1;


  fseek(f, 0L, SEEK_END);
  size_t file_size = ftell(f);
  rewind(f);


  uint8_t *binary = (uint8_t *)malloc(file_size);

  printf("reading %zu bytes from binary...\n", file_size);

  int x = fread(binary, 1, file_size, f);

  printf("read %d bytes from binary\n", x);

  puts("injecting binary...");

  silent = 1;
  for (uint32_t i = 0; i < x; i++)
    mem_write_8_always_add_memblock(i, binary[i]);

  puts("injecting binary done");

  free(binary);
  
  // for (uint32_t i = 0; i < x; i+=4)
  //   printf("0x%02"PRIx32" = %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n", i, mem_read_8(i), mem_read_8(i+1), mem_read_8(i+2), mem_read_8(i+3));
  silent = 0;


  code_mem_ptr = mem_ptr;

  //printf("mem_ptr now %"PRIu32"\n", mem_ptr);

  
  // go :) - NOTE: must be platform independent at this point:
  cpu.pc = 0;
  //cpu.regs[2] = 0x10000; // set stack pointer to some smaller value than 0-1 ;)
  mem_write_8_always_add_memblock(cpu.regs[2], 0); // = return address, which is known and is 0x0  (tell our software memory system to know the stack!)


  puts("executing!");


  while (1)
  {
    if (abort_next)
    {
      fprintf(stderr, "aborting due to previous error!\n");
      abort();
    }

    // if (inst_count > stop_after_instructions)
    // {
    //   printf("# reached stop_after_instructions\n");
    //   break;
    // }

    inst_count++;

    cpu.regs[0] = 0;
    //printf("reading instruction... ");

    silent = 1;
    const uint32_t inst = mem_read_32(cpu.pc);
    silent = 0;


#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c" // thx to William Whyte on stackoverflow
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 


    printf("\n[PC = 0x%"PRIx32", inst = %02"PRIx8" %02"PRIx8" %02"PRIx8" %02"PRIx8" ("BYTE_TO_BINARY_PATTERN""BYTE_TO_BINARY_PATTERN""BYTE_TO_BINARY_PATTERN""BYTE_TO_BINARY_PATTERN")] MWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMW\n", 
    cpu.pc, (uint8_t)inst, (uint8_t)(inst >> 8), (uint8_t)(inst >> 16),(uint8_t)(inst >> 24), BYTE_TO_BINARY((uint8_t)inst>>24), BYTE_TO_BINARY(inst>>16), BYTE_TO_BINARY(inst>>8), BYTE_TO_BINARY(inst));
        


    uint8_t opcode = inst & 0x7F;

    uint8_t funct3 = (inst >> 12) & 0b111;
    uint8_t funct7 = (inst >> 25) & 0b1111111;


    switch (inst)
    {
      // case 0b00000000001000000000000001110011: // uret (wtf)
      // case 0b00010000001000000000000001110011: // sret (wtf)
      // case 0b00110000001000000000000001110011: // mret (wtf)
      // {
      //   puts("some RET!!!");
      //   break;
      // }

      default:
      {
        //printf("# not a full instruction: 0x%"PRIx32"\n", inst);

        switch (opcode)
        {
          case 0b0010011: // ADDI, SRAI, SRLI, SLLI, ANDI, XORI, ORI, etc.
          {
            switch (funct3)
            {
              case 0b000: // ADDI (LI)
              {
                // add the values of reg rs1 + imm to reg rd  

                uint8_t rd = (inst >> 7) & 0x1F;
                uint8_t rs1 = (inst >> 15) & 0x1F;
                
                uint32_t _imm = (inst >> 20) & 0b111111111111;
                int32_t imm = (((int32_t)_imm) << (32 -12)) >> (32 - 12); // sign-extend to 12 bit integer

                uint32_t old = cpu.regs[rd];
                cpu.regs[rd] = cpu.regs[rs1] + imm;

                printf("OP: ADDI: _imm = %"PRIu32", imm = %"PRIi32", rd = %s, rs1 = %s, x[rd](result) = %"PRIu32", x[rd](old) = %"PRIu32", x[rs1] = %"PRIu32"\n", _imm, imm, r2s(rd), r2s(rs1), cpu.regs[rd], old, cpu.regs[rs1]);
                break;
              }


              case 0b101: // SRAI *and* SRLI
              {
                uint8_t rd = (inst >> 7) & 0x1F;
                uint8_t rs1 = (inst >> 15) & 0x1F;
                
                uint32_t imm = (inst >> 20) & 0b11111;

                uint32_t old = cpu.regs[rd];

                if (inst & 0b01000000000000000000000000000000) // xD SRAI
                {
                  cpu.regs[rd] = cpu.regs[rs1] >> ((int32_t)imm);
                  printf("OP: SRAI: imm = %"PRIi32", rd = %s, rs1 = %s, x[rd](result) = %"PRIu32", x[rd](old) = %"PRIu32", x[rs1] = %"PRIu32"\n", imm, r2s(rd), r2s(rs1), cpu.regs[rd], old, cpu.regs[rs1]);
                }
                else if (inst & 0) // SRLI
                {
                  cpu.regs[rd] = cpu.regs[rs1] >> imm;
                  printf("OP: SRLI: imm = %"PRIi32", rd = %s, rs1 = %s, x[rd](result) = %"PRIu32", x[rd](old) = %"PRIu32", x[rs1] = %"PRIu32"\n", imm, r2s(rd), r2s(rs1), cpu.regs[rd], old, cpu.regs[rs1]);
                }

                break;
              }


              case 0b001: // SLLI
              {
                uint8_t rd = (inst >> 7) & 0x1F;
                uint8_t rs1 = (inst >> 15) & 0x1F;
                
                uint32_t imm = (inst >> 20) & 0b11111;

                uint32_t old = cpu.regs[rd];

                cpu.regs[rd] = cpu.regs[rs1] >> imm;
                printf("OP: SLLI: imm = %"PRIi32", rd = %s, rs1 = %s, x[rd](result) = %"PRIu32", x[rd](old) = %"PRIu32", x[rs1] = %"PRIu32"\n", imm, r2s(rd), r2s(rs1), cpu.regs[rd], old, cpu.regs[rs1]);

                break;
              }


              case 0b111: // ANDI
              {
                // bitwise and the values of reg rs1 and imm to reg rd  

                uint8_t rd = (inst >> 7) & 0x1F;
                uint8_t rs1 = (inst >> 15) & 0x1F;
                
                uint32_t _imm = (inst >> 20) & 0b111111111111;
                int32_t imm = (((int32_t)_imm) << (32 -12)) >> (32 - 12); // sign-extend to 12 bit integer

                uint32_t old = cpu.regs[rd];
                cpu.regs[rd] = cpu.regs[rs1] & imm;

                printf("OP: ANDI: _imm = %"PRIu32", imm = %"PRIi32", rd = %s, rs1 = %s, x[rd](result) = %"PRIu32", x[rd](old) = %"PRIu32", x[rs1] = %"PRIu32"\n", _imm, imm, r2s(rd), r2s(rs1), cpu.regs[rd], old, cpu.regs[rs1]);
                break;
              }


              case 0b100: // XORI
              {
                uint8_t rd = (inst >> 7) & 0x1F;
                uint8_t rs1 = (inst >> 15) & 0x1F;
                
                uint32_t _imm = (inst >> 20) & 0b111111111111;
                int32_t imm = (((int32_t)_imm) << (32 -12)) >> (32 - 12); // sign-extend to 12 bit integer

                uint32_t old = cpu.regs[rd];
                cpu.regs[rd] = cpu.regs[rs1] ^ imm;

                printf("OP: ANDI: _imm = %"PRIu32", imm = %"PRIi32", rd = %s, rs1 = %s, x[rd](result) = %"PRIu32", x[rd](old) = %"PRIu32", x[rs1] = %"PRIu32"\n", _imm, imm, r2s(rd), r2s(rs1), cpu.regs[rd], old, cpu.regs[rs1]);
                break;
              }


              case 0b110: // ORI
              {
                uint8_t rd = (inst >> 7) & 0x1F;
                uint8_t rs1 = (inst >> 15) & 0x1F;
                
                uint32_t _imm = (inst >> 20) & 0b111111111111;
                int32_t imm = (((int32_t)_imm) << (32 -12)) >> (32 - 12); // sign-extend to 12 bit integer

                uint32_t old = cpu.regs[rd];
                cpu.regs[rd] = cpu.regs[rs1] | imm;

                printf("OP: ORI: _imm = %"PRIu32", imm = %"PRIi32", rd = %s, rs1 = %s, x[rd](result) = %"PRIu32", x[rd](old) = %"PRIu32", x[rs1] = %"PRIu32"\n", _imm, imm, r2s(rd), r2s(rs1), cpu.regs[rd], old, cpu.regs[rs1]);
                break;
              }


              default:
              {
                printf("! unknown funct3 %"PRIu8" for opcode %"PRIu8"\n", funct3, opcode);
                //return -1;
                break;
              }
            }

            break;
          }

          case 0b0100011: // SB, SW, etc.
          {
            switch (funct3)
            {
              case 0b000: // SB (n = 8-bit)
              case 0b010: // SW (n = 32-bit)
              {
                // write n-bit value from reg rs2 into memory at reg rs1 + signed offset
                uint8_t rs1 = (inst >> 15) & 0b11111;
                uint8_t rs2 = (inst >> 20) & 0b11111;
                
                uint32_t _offset = ((inst >> 7) & 0b11111) | (((inst >> 25) & 0b1111111) << 5); 
                int32_t offset = (((int32_t)_offset) << (32 -12)) >> (32 - 12); // sign-extend to 12 bit integer
                
                uint32_t addr = ((int32_t)cpu.regs[rs1]) + offset;

                if (funct3 == 0b000) // SB
                  mem_write_8(addr, cpu.regs[rs2]);
                else // SW
                  mem_write_32(addr, cpu.regs[rs2]);

                printf("OP: SW/SB: offset = %"PRIi32", _offset = %"PRIu32", rs1 = %s, rs2 = %s, reg[rs1] = %"PRIu32", reg[rs2] = %"PRIu32", addr = %"PRIx32"\n", offset, _offset, r2s(rs1), r2s(rs2), cpu.regs[rs1],cpu.regs[rs2], addr);

                break;
              }

              // case 0b010:
              // {
              //   // write 32-bit value from reg rs2 into memory at reg rs1 + signed offset
              //   uint8_t rs1 = (inst >> 15) & 0b11111;
              //   uint8_t rs2 = (inst >> 20) & 0b11111;
                
              //   uint32_t _offset = ((inst >> 7) & 0b11111) | (((inst >> 25) & 0b1111111) << 5); 
              //   int32_t offset = (((int32_t)_offset) << (32 -12)) >> (32 - 12); // sign-extend to 12 bit integer
                
              //   uint32_t addr = cpu.regs[rs1] + offset;

              //   mem_write_32(addr, cpu.regs[rs2]);

              //   printf("OP: SW: offset = %"PRIi32", _offset = %"PRIu32", rs1 = %"PRIu8", rs2 = %"PRIu8", reg[rs1] = %"PRIu32", reg[rs2] = %"PRIu32", addr = %"PRIu32"\n", offset, _offset, r2s(rs1), r2s(rs2), cpu.regs[rs1],cpu.regs[rs2], addr);

              //   break;
              // }

              default:
              {
                printf("! unknown funct3 %"PRIu8" for opcode %"PRIu8"\n", funct3, opcode);
                //return -1;
                break;
              }
            }

            break;
          }

          case 0b0110111: // LUI
          {
            // put 12 bit sign extended imm into reg rd

            uint8_t rd = (inst >> 7) & 0b11111;
            uint32_t _imm = (inst & 0b11111111111111111111000000000000);
            int32_t imm = (((int32_t)_imm) << 0) >> 0;

            cpu.regs[rd] = imm;
            printf("OP: LUI: rd = %s, imm = %"PRIi32", _imm = %"PRIu32"\n", r2s(rd), imm, _imm);
            break;
          }


          case 0b0000011: // LW, LB, LBU, LHU, etc.
          {
            switch (funct3)
            {
              case 0b010: // LW
              case 0b000: // LB
              case 0b100: // LBU
              case 0b101: // LHU
              {
                // put 32 bit sign extended value of memory address on reg rs1 into reg rd

                uint8_t rd = (inst >> 7) & 0b11111;
                uint8_t rs1 = (inst >> 15) & 0b11111;

                uint32_t _offset = (inst >> 20) & 0b111111111111;

                int32_t offset;
                //if (funct3 == 0b010 || funct3 == 0b100) // LW or LB
                  offset = (((int32_t)_offset) << (32 - 12)) >> (32 - 12); // s-e 2 12
                // else // LBU
                //   offset =  ... // zero-extend

                uint32_t addr = (((int32_t)cpu.regs[rs1]) + offset);
                uint32_t _val;
                
                if (funct3 == 0b010 || funct3 == 0b100) // LB / LBU
                  _val = mem_read_8(addr);
                else if (funct3 == 0b101) // LHU
                  _val = mem_read_16(addr);
                else if (funct3 == 0b010) // LW
                  _val = mem_read_32(addr);
                
                if (funct3 == 0b010 || funct3 == 0b000) // sign-extend for LB & LW
                  cpu.regs[rd] = ((((int32_t)_val) << 0) >> 0); // s-e 2 0
                else // zero-extend for LBU
                  cpu.regs[rd] = ((_val << 0) >> 0); // z-e 2 0
                
                printf("OP: LW/LB: rd = %s, rs1 = %s, reg[rs1] = %"PRIu32", addr = %"PRIx32", _offset = %"PRIu32", offset = %"PRIi32", _val = %"PRIu32", reg[rd] = %"PRIi32" (unsigned = %"PRIu32")\n", r2s(rd), r2s(rs1), cpu.regs[rs1], addr, _offset, offset, _val, cpu.regs[rd], cpu.regs[rd]);
                break;
              }

              default:
              {
                printf("! unknown funct3 %"PRIu8" for opcode %"PRIu8"\n", funct3, opcode);
                //return -1;
                break;
              }
            }

            break;
          }


          case 0b1100111: // JALR (RET), etc.
          {
            switch (funct3)
            {
              case 0b000: // JALR (RET)
              {
                uint8_t rd = (inst >> 7) & 0b11111;
                uint8_t rs1 = (inst >> 15) & 0b11111;

                uint32_t _offset = (inst >> 20) & 0b111111111111;
                int32_t offset = (((int32_t)_offset) << (32 - 12)) >> (32 - 12); // s-e 2 12

                uint32_t next_address = 0;
                if (rd != 0) // instruction is RET if rd == 0! tries to write next address to 0-register, unnecessary
                {
                  uint32_t next_address = cpu.pc + 4;
                  cpu.regs[rd] = next_address;
                }                
                
                uint32_t rs1_val = cpu.regs[rs1];
                uint32_t new_pc = ((rs1_val + offset) & ~(uint32_t)1);
                
                if (rd == 0 && offset == 0 && new_pc == 0 && rs1_val == 0) // RET, with no 'valid' return address
                {
                  printf("# program exited with code: TODO\n");
                  //printf("# program exited with code: %"PRIi32"\n", cpu.regs[10]); ???
                  return 0;
                }

                cpu.pc = new_pc;

                printf("OP: JALR (RET): rd = %s, rs1 = %s, reg[rs1] = 0x%"PRIx32", _offset = %"PRIu32", offset = %"PRIi32", next_address & reg[rd] = 0x%"PRIx32", pc = 0x%"PRIx32"\n", 
                r2s(rd), r2s(rs1), cpu.regs[rs1], _offset, offset, next_address, cpu.pc);

                continue; // !!!!!!!!!!!!!!! (so pc will NOT be +4'd)
              }


              default:
              {
                printf("! unknown funct3 %"PRIu8" for opcode %"PRIu8"\n", funct3, opcode);
                return -1;
                //break;
              }
            }

            break;
          }


          case 0b1100011: // BLT, BNE, BEQ, BGE, BGEU, BLTU, etc.
          {
            switch (funct3)
            {
              case 0b001: // BNE
              case 0b100: // BLT
              case 0b000: // BEQ
              case 0b101: // BGE
              case 0b110: // BLTU
              case 0b111: // BGEU
              {
                // add sign extended offset to pc if rs1 is smaller than rs2

                uint8_t rs1 = (inst >> 15) & 0b11111;
                uint8_t rs2 = (inst >> 20) & 0b11111;

                uint32_t rs1_val = cpu.regs[rs1];
                uint32_t rs2_val = cpu.regs[rs2];

                int8_t branch = 0;

                switch (funct3)
                {
                  case 0b001: // BNE
                  {
                    if (rs1_val != rs2_val)
                      branch = 1;

                    printf("OP: BNE: rs1 = %s, rs2 = %s, reg[rs1] = %"PRIu32", reg[rs2] = %"PRIu32", branching? %i\n", 
                      r2s(rs1), r2s(rs2), rs1_val, rs2_val, branch);
                    
                    break;
                  }

                  case 0b100: // BLT
                  {
                    if ((int32_t)rs1_val < (int32_t)rs2_val)
                      branch = 1;

                    printf("OP: BLT: rs1 = %s, rs2 = %s, reg[rs1] = %"PRIu32", reg[rs2] = %"PRIu32", branching? %i\n", 
                      r2s(rs1), r2s(rs2), rs1_val, rs2_val, branch);
                    
                    break;
                  }

                  case 0b000: // BEQ
                  {
                    if (rs1_val == rs2_val)
                      branch = 1;

                    printf("OP: BEQ: rs1 = %s, rs2 = %s, reg[rs1] = %"PRIu32", reg[rs2] = %"PRIu32", branching? %i\n", 
                      r2s(rs1), r2s(rs2), rs1_val, rs2_val, branch);
                    
                    break;
                  }

                  case 0b101: // BGE
                  {
                    if ((int32_t)rs1_val > (int32_t)rs2_val)
                      branch = 1;

                    printf("OP: BGE: rs1 = %s, rs2 = %s, reg[rs1] = %"PRIu32", reg[rs2] = %"PRIu32", branching? %i\n", 
                      r2s(rs1), r2s(rs2), rs1_val, rs2_val, branch);
                    
                    break;
                  }

                  case 0b111: // BGEU
                  {
                    if (rs1_val > rs2_val)
                      branch = 1;

                    printf("OP: BGEU: rs1 = %s, rs2 = %s, reg[rs1] = %"PRIu32", reg[rs2] = %"PRIu32", branching? %i\n", 
                      r2s(rs1), r2s(rs2), rs1_val, rs2_val, branch);
                    
                    break;
                  }

                  case 0b110: // BLTU
                  {
                    if (rs1_val < rs2_val)
                      branch = 1;

                    printf("OP: BLTU: rs1 = %s, rs2 = %s, reg[rs1] = %"PRIu32", reg[rs2] = %"PRIu32", branching? %i\n", 
                      r2s(rs1), r2s(rs2), rs1_val, rs2_val, branch);
                    
                    break;
                  }

                  default:
                  {
                    fprintf(stderr, "dev forgot to add case here...\n");
                    return -1;
                  }
                }
                
                if (!branch)
                  break;
                
                // imm[20|10:1|11|19:12]   o_o'
                // offset[12|10:5] ... offset[4:1|11]   o.o
                uint32_t _offset = ((inst >> 31) & 1) << 12 | ((inst >> 7) & 1) << 11 | ((inst >> 25) & 0b111111) << 5 | ((inst >> 8) & 0b1111) << 1;
                int32_t offset = (((int32_t)_offset) << (32 - 13)) >> (32 - 13); // sign-extend to 13 (!) bit integer

                cpu.pc += offset;

                continue; // !!!!!!!!!!!!!!! (so pc will NOT be +4'd)
              }


              
              {
                // add sign extended offset to pc if rs1 and rs2 are not equal

                uint8_t rs1 = (inst >> 15) & 0b11111;
                uint8_t rs2 = (inst >> 20) & 0b11111;

                uint32_t rs1_val = cpu.regs[rs1];
                uint32_t rs2_val = cpu.regs[rs2];

                if (rs1_val == rs2_val)
                {
                  printf("OP: BNE: rs1 = %s, rs2 = %s, reg[rs1] = %"PRIu32", reg[rs2] = %"PRIu32", branching? NO\n", r2s(rs1), r2s(rs2), rs1_val, rs2_val);
                  break;
                }
                
                // offset[12|10:5] ... offset[4:1|11]   o.o
                uint32_t _offset = ((inst >> 31) & 1) << 12 | ((inst >> 7) & 1) << 11 | ((inst >> 25) & 0b111111) << 5 | ((inst >> 8) & 0b1111) << 1;
                int32_t offset = (((int32_t)_offset) << (32 -12)) >> (32 - 12); // sign-extend to 12 bit integer

                cpu.pc += offset;

                printf("OP: BNE: rs1 = %s, rs2 = %s, reg[rs1] = %"PRIu32", reg[rs2] = %"PRIu32", _offset = %"PRIu32", offset = %"PRIi32", branching? YES\n", r2s(rs1), r2s(rs2), rs1_val, rs2_val, _offset, offset);
                continue; // !!!!!!!!!!!!!!! (so pc will NOT be +4'd)
              }


              
              {
                // add sign extended offset to pc if rs1 and rs2 are equal

                uint8_t rs1 = (inst >> 15) & 0b11111;
                uint8_t rs2 = (inst >> 20) & 0b11111;

                uint32_t rs1_val = cpu.regs[rs1];
                uint32_t rs2_val = cpu.regs[rs2];

                if (rs1_val != rs2_val)
                {
                  printf("OP: BEQ: rs1 = %s, rs2 = %s, reg[rs1] = %"PRIu32", reg[rs2] = %"PRIu32", branching? NO\n", r2s(rs1), r2s(rs2), rs1_val, rs2_val);
                  break;
                }
                
                // offset[12|10:5] ... offset[4:1|11]   o.o
                uint32_t _offset = ((inst >> 31) & 1) << 12 | ((inst >> 7) & 1) << 11 | ((inst >> 25) & 0b111111) << 5 | ((inst >> 8) & 0b1111) << 1;
                int32_t offset = (((int32_t)_offset) << (32 -12)) >> (32 - 12); // sign-extend to 12 bit integer

                cpu.pc += offset;

                printf("OP: BEQ: rs1 = %s, rs2 = %s, reg[rs1] = %"PRIu32", reg[rs2] = %"PRIu32", _offset = %"PRIu32", offset = %"PRIi32", branching? YES\n", r2s(rs1), r2s(rs2), rs1_val, rs2_val, _offset, offset);
                continue; // !!!!!!!!!!!!!!! (so pc will NOT be +4'd)
              }


              
              {

              }


              default:
              {
                printf("! unknown funct3 %"PRIu8" for opcode %"PRIu8"\n", funct3, opcode);
                return -1;
                //break;
              }
            }

            break;
          }


          case 0b0010111: // AUIPC
          {
            uint8_t rd = (inst >> 7) & 0b11111;
            uint32_t _imm = (inst & 0b11111111111111111111000000000000);
            int32_t imm = (((int32_t)_imm) << 0 >> 0);
            
            uint32_t pc = (((int32_t)cpu.pc) + imm);
            cpu.regs[rd] = pc;

            printf("OP: AUIPC: rd = %s, imm = %"PRIi32", _imm = %"PRIu32", reg[rd] = %"PRIu32"\n", r2s(rd), imm, _imm, pc);
            break;
          }


          case 0b1101111: // JAL (J)
          {
            uint8_t rd = (inst >> 7) & 0b11111;
            
            // imm[20|10:1|11|19:12]   o_o'
            uint32_t _imm = ((inst >> 31) & 1) << 20 | ((inst >> 12) & 0b11111111) << 12 | ((inst >> 20) & 1) << 11 | ((inst >> 21) & 0b1111111111) << 1;
            int32_t imm = (((int32_t)_imm) << (32-16) >> (32-16));

            cpu.regs[rd] = cpu.pc + 4; // write return address into reg rd
            cpu.pc = ((int32_t)cpu.pc) + imm;

            printf("OP: JAL: rd = %s, _imm = %"PRIu32", imm = %"PRIi32", pc(new) = 0x%"PRIx32", reg[rd] = %"PRIx32"\n", 
            r2s(rd), _imm, imm, cpu.pc, cpu.regs[rd]);

            continue; // !!!!!!!!!!!!!!! (so pc will NOT be +4'd)
          }


          case 0b0110011: // ADD, SUB, etc.
          {
            switch (funct3)
            {
              case 0b000: // ADD *and* SUB
              {
                uint8_t rd = (inst >> 7) & 0b11111;
                uint8_t rs1 = (inst >> 15) & 0b11111;
                uint8_t rs2 = (inst >> 20) & 0b11111;

                if ((inst >> 30) & 1) // xD SUB
                {
                  cpu.regs[rd] = cpu.regs[rs1] - cpu.regs[rs2];

                  printf("OP: SUB: rd = %s, rs1 = %s, reg[rs1] = %"PRIu32", rs2 = %s, reg[rs2] = %"PRIu32", reg[rd] = %"PRIx32"\n", 
                    r2s(rd), r2s(rs1), cpu.regs[rs1], r2s(rs2), cpu.regs[rs2], cpu.regs[rd]);
                }
                else if (!((inst >> 30) & 1)) // ADD
                {
                  cpu.regs[rd] = cpu.regs[rs1] + cpu.regs[rs2];

                  printf("OP: ADD: rd = %s, rs1 = %s, reg[rs1] = %"PRIu32", rs2 = %s, reg[rs2] = %"PRIu32", reg[rd] = %"PRIx32"\n", 
                    r2s(rd), r2s(rs1), cpu.regs[rs1], r2s(rs2), cpu.regs[rs2], cpu.regs[rd]);
                }
                else 
                {
                  printf("! unknown function for funct3 %"PRIx8" for opcode %"PRIx8" (instruction: %"PRIx32")\n", funct3, opcode, inst);
                  return -1;
                }

                break;
              }

              default:
              {
                printf("! unknown funct3 %"PRIu8" for opcode %"PRIu8"\n", funct3, opcode);
                return -1;
                //break;
              }
            }

            break;
          }


          default:
          {
            printf("unknown opcode 0x%"PRIx8" / %"PRIu8" / "BYTE_TO_BINARY_PATTERN"\n", opcode, opcode, BYTE_TO_BINARY(((uint8_t)((opcode >> 2) & 0b11111))));
            return -1;
            //break;
          }
        }

        break;
      }
    }

    cpu.pc += 4;
  }


  // silent = 1;
  // for (uint32_t i = 0; i < x; i+=4)
  //   printf("0x%02"PRIx32" = %02"PRIx32" %02"PRIx32" %02"PRIx32" %02"PRIx32"\n", i, mem_read_8(i), mem_read_8(i+1), mem_read_8(i+2), mem_read_8(i+3));
  // silent = 0;


  puts("end");
  return 0;
}