#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>


#define MAX_MEM_BLOCKS 10000
uint16_t mem_ptr = 0;
uint32_t mem_index[MAX_MEM_BLOCKS];
uint32_t mem_block[MAX_MEM_BLOCKS];


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
      return mem_block[i];
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
      fprintf(stderr, "mem_read_32: tried reading past mem_ptr\n");
      return 0xFFFFFFFF;
    }

    uint32_t val = mem_block[i] | mem_block[i+3] << 24 | mem_block[i+2] << 16 | mem_block[i+1] << 8;
    printf("mem_read_32: addr = %"PRIu32", val = %"PRIu32"\n", addr, val);
    return val;
  }

  fprintf(stderr, "mem_read_32: no mem_block found for addr %"PRIu32"\n", addr);
  return 0xFFFFFFFF;
}


int silent = 0;


void mem_write_8(uint32_t addr, uint8_t val)
{
  mem_index[mem_ptr] = addr;
  mem_block[mem_ptr] = val;
  mem_ptr++;
  if (silent)
    return;
  printf("mem_write_8: wrote val %"PRIu8" to addr %"PRIu32"\n", val, addr);
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
  printf("mem_write_32: wrote val %"PRIu32" to addr %"PRIu32"\n", val, addr);
}


// void meminit(void *adr, int size)
// {
//   for (int i = 0; i < size; i++)
//     ((char *)adr)[i] = 0;
// }


int main()
{
  puts("go");

  cpu_t cpu;
  //meminit(cpu, sizeof(cpu));
  uint32_t cpu_size = sizeof(cpu_t);
  memset(&cpu, 0, cpu_size);

  printf("cpu size = %"PRIu32"\n", cpu_size);

  FILE *f = fopen("test.bin", "r");

  if (!f)
    return -1;

  fseek(f, 0L, SEEK_END);
  size_t file_size = ftell(f);
  rewind(f);

  uint8_t *binary = (uint8_t *)malloc(file_size);

  int x = fread(binary, 1, file_size, f);

  printf("read %d bytes from binary\n", x);

  silent = 1;
  for (int i = 0; i < x; i++)
    mem_write_8(i, binary[i]);
  silent = 0;

  printf("mem_ptr now %"PRIu32"\n", mem_ptr);


  // go :) - NOTE: must be platform independent at this point:
  cpu.pc = 0;


  while (1)
  {
    printf("MWMWMWMWMWMWMWMWMWMWMWMWMWMWMWMW\n");
    cpu.regs[0] = 0;
    printf("reading instruction... ");
    uint32_t inst = mem_read_32(cpu.pc);

    uint8_t opcode = inst & 0x7F;

    uint8_t funct3 = (inst >> 12) & 0b111;
    uint8_t funct7 = (inst >> 25) & 0b1111111;


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
            uint64_t imm = (((uint32_t)inst) >> 20) & 0b111111111111;

            int32_t imm_val = imm;
            imm_val = (imm_val << (32 - 12) >> (32 - 12)); // sign-extend to 12 bit integer

            uint32_t old = cpu.regs[rd];
            cpu.regs[rd] = cpu.regs[rs1] + imm_val;

            printf("OP: ADDI: imm_val = %"PRIi32", rd = %"PRIu8", rs1 = %"PRIu8", result = %"PRIu32", old = %"PRIu32"\n", imm_val, rd, rs1, cpu.regs[rd], old);
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
          case 0b010:
          {
            // write 32-bit value from reg rs2 into memory at reg rs1 + signed offset
            uint8_t rs1 = (inst >> 15) & 0b11111;
            uint8_t rs2 = (inst >> 20) & 0b11111;
            
            uint32_t _offset = ((inst >> 7) & 0b11111) | (((inst >> 25) & 0b1111111) << 5); 
            int32_t offset = (((int32_t)_offset) << (32 -12)) >> (32 - 12); // sign-extend to 12 bit integer
            
            uint32_t addr = cpu.regs[rs1] + offset;

            mem_write_32(addr, rs2);

            printf("OP: SW: offset = %"PRIi32", _offset = %"PRIu32", rs1 = %"PRIu8", rs2 = %"PRIu8", addr = %"PRIu32"\n", offset, _offset, rs1, rs2, addr);

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

      default:
      {
        printf("unknown opcode 0x%"PRIx8" / %"PRIu8"\n", opcode, opcode);
        return -1;
        //break;
      }
    }

    cpu.pc += 4;
  }


  puts("end");
  return 0;
}