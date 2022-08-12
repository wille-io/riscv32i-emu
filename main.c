#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>


#define MAX_MEM_BLOCKS 10000
uint16_t mem_ptr = 0;
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
  for (int i = 0; i < mem_ptr; i++)
  {
    if (mem_index[i] == addr)
    {
      uint8_t val = mem_block[i];
      printf("mem_read_8: addr = 0x%"PRIx32", val = %"PRIu32"\n", addr, val);
      return val;
    }
  }

  fprintf(stderr, "mem_read_8: 'segmentation' violation\n");
  return 0xFF;
}


uint32_t mem_read_32(uint32_t addr)
{
  for (int i = 0; i < mem_ptr; i++)
  {
    if (mem_index[i] != addr)
      continue;
    
    if ((i + 3) > mem_ptr)
    {
      fprintf(stderr, "! mem_read_32: tried reading past mem_ptr\n");
      return 0xFFFFFFFF;
    }

    uint32_t val = mem_block[i] | mem_block[i+3] << 24 | mem_block[i+2] << 16 | mem_block[i+1] << 8;
    if (!silent)
      printf("mem_read_32: addr = 0x%"PRIx32", val = %"PRIu32"\n", addr, val);
    return val;
  }

  fprintf(stderr, "! mem_read_32: no mem_block found for addr 0x%"PRIx32"\n", addr);
  return 0xFFFFFFFF;
}


void mem_write_8(uint32_t addr, uint8_t val)
{
  mem_index[mem_ptr] = addr;
  mem_block[mem_ptr] = val;
  mem_ptr++;
  if (silent)
    return;
  printf("mem_write_8: wrote val %"PRIu8" to addr 0x%"PRIx32"\n", val, addr);
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


int main()
{
  //puts("go");

  cpu_t cpu;
  //meminit(cpu, sizeof(cpu));
  uint32_t cpu_size = sizeof(cpu_t);
  memset(&cpu, 0, cpu_size);

  //printf("cpu size = %"PRIu32"\n", cpu_size);

  FILE *f = fopen("test.bin", "r");

  if (!f)
    return -1;

  fseek(f, 0L, SEEK_END);
  size_t file_size = ftell(f);
  rewind(f);

  uint8_t *binary = (uint8_t *)malloc(file_size);

  int x = fread(binary, 1, file_size, f);

  //printf("read %d bytes from binary\n", x);

  silent = 1;
  for (int i = 0; i < x; i++)
    mem_write_8(i, binary[i]);
  silent = 0;

  //printf("mem_ptr now %"PRIu32"\n", mem_ptr);


  // go :) - NOTE: must be platform independent at this point:
  cpu.pc = 0;
  cpu.regs[2] = 0x10000; // set stack pointer to some smaller value than 0-1 ;)
  mem_write_8(cpu.regs[2], 0); // tell our software memory system to know the stack!    TODO: really initialize?


  while (1)
  {
    printf("PC = 0x%"PRIx32"  MWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMW\n", cpu.pc);
    cpu.regs[0] = 0;    
    //printf("reading instruction... ");

    silent = 1;
    uint32_t inst = mem_read_32(cpu.pc);
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

    //printf("instruction: "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(inst));


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
          case 0b0010011: // ADDI, etc.
          {
            switch (funct3)
            {
              case 0b000: // ADDI
              {
                // add the values of reg rs1 + imm to reg rd  

                uint8_t rd = (inst >> 7) & 0x1F;
                uint8_t rs1 = (inst >> 15) & 0x1F;
                
                uint32_t _imm = (inst >> 20) & 0b111111111111;
                int32_t imm = (((int32_t)_imm) << (32 -12)) >> (32 - 12); // sign-extend to 12 bit integer

                uint32_t old = cpu.regs[rd];
                cpu.regs[rd] = cpu.regs[rs1] + imm;

                printf("OP: ADDI: _imm = %"PRIu32", imm = %"PRIi32", rd = %"PRIu8", rs1 = %"PRIu8", x[rd](result) = %"PRIu32", x[rd](old) = %"PRIu32", x[rs1] = %"PRIu32"\n", _imm, imm, rd, rs1, cpu.regs[rd], old, cpu.regs[rs1]);
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

          case 0b0100011: // SW, etc.
          {
            switch (funct3)
            {
              case 0b000: // SB
              case 0b010: // SW
              {
                // write n-bit value from reg rs2 into memory at reg rs1 + signed offset
                uint8_t rs1 = (inst >> 15) & 0b11111;
                uint8_t rs2 = (inst >> 20) & 0b11111;
                
                uint32_t _offset = ((inst >> 7) & 0b11111) | (((inst >> 25) & 0b1111111) << 5); 
                int32_t offset = (((int32_t)_offset) << (32 -12)) >> (32 - 12); // sign-extend to 12 bit integer
                
                uint32_t addr = cpu.regs[rs1] + offset;

                if (funct3 == 0b000) // SB
                  mem_write_8(addr, cpu.regs[rs2]);
                else
                  mem_write_32(addr, cpu.regs[rs2]);

                printf("OP: SW/SB: offset = %"PRIi32", _offset = %"PRIu32", rs1 = %"PRIu8", rs2 = %"PRIu8", reg[rs1] = %"PRIu32", reg[rs2] = %"PRIu32", addr = %"PRIu32"\n", offset, _offset, rs1, rs2, cpu.regs[rs1],cpu.regs[rs2], addr);

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

              //   printf("OP: SW: offset = %"PRIi32", _offset = %"PRIu32", rs1 = %"PRIu8", rs2 = %"PRIu8", reg[rs1] = %"PRIu32", reg[rs2] = %"PRIu32", addr = %"PRIu32"\n", offset, _offset, rs1, rs2, cpu.regs[rs1],cpu.regs[rs2], addr);

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
            uint32_t _imm = (inst >> 12) & 0b11111111111111111111;
            int32_t imm = (((int32_t)_imm) << 0) >> 0;

            cpu.regs[rd] = imm;
            printf("OP: LUI: rd = %"PRIu8", imm = %"PRIi32", _imm = %"PRIu32"\n", rd, imm, _imm);
            break;
          }


          case 0b0000011: // LW, etc.
          {
            switch (funct3)
            {
              case 0b010: // LW
              {
                // put 32 bit sign extended value of memory address on reg rs1 into reg rd

                uint8_t rd = (inst >> 7) & 0b11111;
                uint8_t rs1 = (inst >> 15) & 0b11111;

                uint32_t _offset = (inst >> 20) & 0b111111111111;
                int32_t offset = (((int32_t)_offset) << (32 - 12)) >> (32 - 12); // s-e 2 12

                uint32_t addr = cpu.regs[rs1];
                uint32_t _val = mem_read_32(addr);
                int32_t val = ((((int32_t)_val) << 0) >> 0) + offset; // s-e 2 0

                cpu.regs[rd] = val;
                printf("OP: LW: rd = %"PRIu8", rs1 = %"PRIu8", addr = %"PRIu32", _offset = %"PRIu32", offset = %"PRIi32", _val = %"PRIu32", val = %"PRIi32"\n", rd, rs1, addr, _offset, offset, _val, val);
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
                if (rd != 0) // instruction was RET! tries to write next address to 0-register, unnecessary
                {
                  uint32_t next_address = cpu.pc + 4;
                  cpu.regs[rd] = next_address;
                }

                cpu.pc = ((cpu.regs[rs1] + offset) & ~(uint32_t)1) - 4; // 0b01111111111111111111111111111110; // ?????????????

                printf("OP: JALR (RET): rd = %"PRIu8", rs1 = %"PRIu8", reg[rs1] = 0x%"PRIx32", _offset = %"PRIu32", offset = %"PRIi32", next_address & reg[rd] = 0x%"PRIx32", pc = 0x%"PRIx32"\n", 
                rd, rs1, cpu.regs[rs1], _offset, offset, next_address, cpu.pc);

                continue; // !!!!!!!!!!!!!!!
              }


              default:
              {
                printf("unknown opcode 0x%"PRIx8" / %"PRIu8"\n", opcode, opcode);
                return -1;
                //break;
              }
            }

            break;
          }


          default:
          {
            printf("unknown opcode 0x%"PRIx8" / %"PRIu8"\n", opcode, opcode);
            return -1;
            //break;
          }
        }

        break;
      }
    }

    cpu.pc += 4;
  }


  puts("end");
  return 0;
}