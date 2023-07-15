#include "core_logger.h"

// TODO: Move to build.bat
#define SIM8086_DEBUG

#define sim8086_internal static

#define sim8086_array_count(array) (sizeof(array) / sizeof(array[0]))

#define sim8086_swap(a, b, type) { type temp = a; a = b; b = temp; }

#include <stdlib.h>
#include <string.h>

sim8086_internal uint8_t bitfield_extract(uint8_t value, uint8_t offset, uint8_t count)
{
    const uint8_t mask = (1 << count) - 1;
    const uint8_t result = (value >> offset) & mask;
    return result;
}

sim8086_internal int16_t sign_extend_8_to_16(uint8_t value)
{
    const int16_t result = (value & 0x80) ? (0xFF00 | value) : value;
    return result;
}

typedef enum
{
    register_name_a = 0,
    register_name_c,
    register_name_d,
    register_name_b,
    register_name_sp,
    register_name_bp,
    register_name_si,
    register_name_di,

    register_name_count
} register_name_t;

typedef struct
{
    register_name_t name;
    uint8_t offset;
    uint8_t count;
} register_operand_t;

typedef enum
{
    memory_address_expression_bx_plus_si = 0,
    memory_address_expression_bx_plus_di,
    memory_address_expression_bp_plus_si,
    memory_address_expression_bp_plus_di,
    memory_address_expression_si,
    memory_address_expression_di,
    memory_address_expression_direct_address,
    memory_address_expression_bx,

    memory_address_expression_count
} memory_address_expression_t;

typedef struct
{
    memory_address_expression_t address_expression;
    int32_t displacement; // just 16 bits should do but the compiler will pad it up anyways, so why not just claim the memory
} memory_operand_t;

typedef struct
{
    int32_t value;
} immediate_operand_t;

typedef struct
{
    int32_t value;
} relative_jump_immediate_operand_t;

typedef enum
{
    instruction_operand_kind_register = 0,
    instruction_operand_kind_memory,
    instruction_operand_kind_immediate,
    instruction_operand_kind_relative_jump_immediate,

    instruction_operand_kind_count
} instruction_operand_kind_t;

typedef struct
{
    instruction_operand_kind_t kind;
    union
    {
        register_operand_t reg;
        memory_operand_t mem;
        immediate_operand_t imm;
        relative_jump_immediate_operand_t rel_jump_imm;
    } payload;
} instruction_operand_t;

typedef enum
{
    op_kind_mov = 0,
    op_kind_add,
    op_kind_sub,
    op_kind_cmp,
    op_kind_jz,     // je
    op_kind_jl,     // jnge
    op_kind_jle,    // jng
    op_kind_jb,     // jnae
    op_kind_jbe,    // jna
    op_kind_jp,     // jpe
    op_kind_jo,
    op_kind_js,
    op_kind_jnz,    // jne
    op_kind_jnl,    // jge
    op_kind_jnle,   // jg
    op_kind_jnb,    // jae
    op_kind_jnbe,   // ja
    op_kind_jnp,    // jpo
    op_kind_jno,
    op_kind_jns,
    op_kind_loop,
    op_kind_loopz,  // loope
    op_kind_loopnz, // loopne
    op_kind_jcxz,

    op_kind_count
} op_kind_t;

sim8086_internal __forceinline op_kind_t get_op_kind(uint8_t opcode, uint8_t extra_opcode)
{
    switch (opcode)
    {
        case 0b100010:
        case 0b1100011:
        case 0b1011:
        case 0b1010000:
        case 0b1010001:
            return op_kind_mov;

        case 0b000000:
        case 0b0000010:
            return op_kind_add;

        case 0b001010:
        case 0b0010110:
            return op_kind_sub;

        case 0b001110:
        case 0b0011110:
            return op_kind_cmp;

        case 0b100000:
        {
            switch (extra_opcode)
            {
                case 0b000:
                    return op_kind_add;

                case 0b101:
                    return op_kind_sub;

                case 0b111:
                    return op_kind_cmp;

                default:
                    return op_kind_count;
            }
        }

        case 0b01110100:
            return op_kind_jz;
        case 0b01111100:
            return op_kind_jl;
        case 0b01111110:
            return op_kind_jle;
        case 0b01110010:
            return op_kind_jb;
        case 0b01110110:
            return op_kind_jbe;
        case 0b01111010:
            return op_kind_jp;
        case 0b01110000:
            return op_kind_jo;
        case 0b01111000:
            return op_kind_js;
        case 0b01110101:
            return op_kind_jnz;
        case 0b01111101:
            return op_kind_jnl;
        case 0b01111111:
            return op_kind_jnle;
        case 0b01110011:
            return op_kind_jnb;
        case 0b01110111:
            return op_kind_jnbe;
        case 0b01111011:
            return op_kind_jnp;
        case 0b01110001:
            return op_kind_jno;
        case 0b01111001:
            return op_kind_jns;
        case 0b11100010:
            return op_kind_loop;
        case 0b11100001:
            return op_kind_loopz;
        case 0b11100000:
            return op_kind_loopnz;
        case 0b11100011:
            return op_kind_jcxz;

        default:
            return op_kind_count;
    }
}

typedef struct
{
    op_kind_t op_kind;
    uint8_t w;

    // NOTE: operands[0] is the the one which appears first in
    // the disassembly instruction, this is usually the destination operand.
    // For instructions which have only one operand, operand[0] will be used.
    instruction_operand_t operands[2];
} instruction_t;

sim8086_internal instruction_operand_t get_register_operand(uint8_t index, bool is_wide)
{
    instruction_operand_t result = { 0 };
    result.kind = instruction_operand_kind_register;
    result.payload.reg.count = is_wide ? 2 : 1;

    switch (index)
    {
        case 0b000:
        {
            result.payload.reg.name     = register_name_a;
            result.payload.reg.offset   = 0;
        } break;

        case 0b001:
        {
            result.payload.reg.name     = register_name_c;
            result.payload.reg.offset   = 0;
        } break;

        case 0b010:
        {
            result.payload.reg.name     = register_name_d;
            result.payload.reg.offset   = 0;
        } break;

        case 0b011:
        {
            result.payload.reg.name     = register_name_b;
            result.payload.reg.offset   = 0;
        } break;

        case 0b100:
        {
            result.payload.reg.name     = is_wide ? register_name_sp : register_name_a;
            result.payload.reg.offset   = is_wide ? 0 : 1;
        } break;

        case 0b101:
        {
            result.payload.reg.name     = is_wide ? register_name_bp : register_name_c;
            result.payload.reg.offset   = is_wide ? 0 : 1;
        } break;

        case 0b110:
        {
            result.payload.reg.name     = is_wide ? register_name_si : register_name_d;
            result.payload.reg.offset   = is_wide ? 0 : 1;
        } break;

        case 0b111:
        {
            result.payload.reg.name     = is_wide ? register_name_di : register_name_b;
            result.payload.reg.offset   = is_wide ? 0 : 1;
        } break;

        default:
            assert(false);
    }

    return result;
}

sim8086_internal instruction_operand_t get_memory_operand(uint8_t index, uint8_t mod, uint8_t **instruction_ptr)
{
    instruction_operand_t result;
    result.kind = instruction_operand_kind_memory;

    switch (index)
    {
        case 0b000:
            result.payload.mem.address_expression = memory_address_expression_bx_plus_si;
            break;
        case 0b001:
            result.payload.mem.address_expression = memory_address_expression_bx_plus_di;
            break;
        case 0b010:
            result.payload.mem.address_expression = memory_address_expression_bp_plus_si;
            break;
        case 0b011: 
            result.payload.mem.address_expression = memory_address_expression_bp_plus_di;
            break;
        case 0b100:
            result.payload.mem.address_expression = memory_address_expression_si;
            break;
        case 0b101:               
            result.payload.mem.address_expression = memory_address_expression_di;
            break;
        case 0b110:
            result.payload.mem.address_expression = memory_address_expression_direct_address;
            break;
        case 0b111:
            result.payload.mem.address_expression = memory_address_expression_bx;
            break;
        default:
            assert(false);
    }

    uint8_t displacement_size = 0;
    const uint8_t *displacement = *instruction_ptr;
    if ((mod == 0b10) || (mod == 0b00 && index == 0b110) /*special case (direct address)*/)
    {
        displacement_size = 2;
        result.payload.mem.displacement = (int32_t)(*((const uint16_t *)displacement));
    }
    else
    {
        displacement_size = 1;
        result.payload.mem.displacement = (int32_t)(sign_extend_8_to_16(*displacement));
    }
    *instruction_ptr = *instruction_ptr + displacement_size;

    return result;
}

sim8086_internal instruction_operand_t get_immediate_operand(bool should_sign_extend, bool is_wide, uint8_t **instruction_ptr)
{
    instruction_operand_t result;
    result.kind = instruction_operand_kind_immediate;

    const uint8_t *immediate_data = *instruction_ptr;
    uint8_t immediate_size = 0;
    if (!should_sign_extend && !is_wide)
    {
        immediate_size = 1;
        result.payload.imm.value = (int32_t)(*immediate_data);
    }
    else if (!should_sign_extend && is_wide)
    {
        immediate_size = 2;
        result.payload.imm.value = (int32_t)(*((const uint16_t *)immediate_data));
    }
    else if (should_sign_extend && is_wide)
    {
        immediate_size = 1;
        result.payload.imm.value = (int32_t)(sign_extend_8_to_16(*immediate_data));
    }
    else
    {
        // NOTE: This case shouldn't be possible.
        assert(false);
    }

    *instruction_ptr = *instruction_ptr + immediate_size;

    return result;
};

sim8086_internal instruction_operand_t get_relative_jump_immediate_operand(uint8_t **instruction_ptr)
{
    instruction_operand_t result;
    result.kind = instruction_operand_kind_relative_jump_immediate;

    const uint8_t relative_immediate = **instruction_ptr;
    // NOTE: NASM will add a -2 by itself to the displacement so add 2 to counter that.
    result.payload.rel_jump_imm.value = (int32_t)(sign_extend_8_to_16(relative_immediate)) + 2;
    *instruction_ptr = *instruction_ptr + 1;
    return result;
};

sim8086_internal uint8_t print_signed_constant(int32_t constant, char *dst)
{
    assert(constant <= INT16_MAX && constant >= INT16_MIN);

    uint8_t result = 0;
    if (constant > 0)
        dst[result++] = '+';

    const uint8_t chars_written = (uint8_t)sprintf(dst, "%d", constant);
    result += chars_written;

    return result;
}

sim8086_internal void print_register_operand(register_operand_t op)
{
    switch (op.name)
    {
        case register_name_sp:
            printf("sp");
            return;
        
        case register_name_bp:
            printf("bp");
            return;

        case register_name_si:
            printf("si");
            return;

        case register_name_di:
            printf("di");
            return;

        default:
            break;
    }

    uint8_t len = 0;
    char name[8];
    switch (op.name)
    {
        case register_name_a:
            name[len++] = 'a';
            break;

        case register_name_b:
            name[len++] = 'b';
            break;

        case register_name_c:
            name[len++] = 'c';
            break;

        case register_name_d:
            name[len++] = 'd';
            break;

        default:
            assert(false);
    }

    if (op.offset == 1)
        name[len++] = 'h';
    else
        name[len++] = (op.count == 2) ? 'x' : 'l';

    assert(len == 2);
    name[len] = '\0';

    printf("%s", name);
}

sim8086_internal void print_memory_operand(memory_operand_t op)
{
    uint8_t len = 0;
    char expression[32];
    {
        expression[len++] = '[';

        {
            const char *table[] =
            {
                "bx+si",
                "bx+di",
                "bp+si",
                "bp+di",
                "si",
                "di",
                "",
                "bx"
            };

            const uint8_t chars_written = (uint8_t)sprintf(expression+len, "%s", table[op.address_expression]);
            len += chars_written;
        }

        {
            const bool has_direct_address = (op.address_expression == memory_address_expression_direct_address);
            const bool has_disp = !has_direct_address && (op.displacement != 0);

            if (has_direct_address)
            {
                const uint8_t chars_written = (uint8_t)sprintf(expression+len, "%d", op.displacement);
                // sprintf will also write the null character, but we will overwrite it because
                // we still need to add more stuff to the string. Also note that `chars_written`
                // will not include the null character.
                len += chars_written;
            }
            else if (has_disp)
            {
                const uint8_t chars_written = print_signed_constant(op.displacement, expression+len);
                len += chars_written;
            }
        }

        expression[len++] = ']';
        
        assert(len < sim8086_array_count(expression));
        expression[len] = '\0';
    }

    printf("%s", expression);
}

sim8086_internal void print_immediate(immediate_operand_t op, const char *size_expression)
{
    uint8_t len = 0;
    char expression[16];
    {
        if (size_expression)
        {
            const uint8_t chars_written = (uint8_t)sprintf(expression+len, "%s ", size_expression);
            len += chars_written;
        }

        {
            const uint8_t chars_written = (uint8_t)sprintf(expression+len, "%d", op.value);
            len += chars_written;
        }

        assert(len < sim8086_array_count(expression));
        expression[len] = '\0';
    }

    printf("%s", expression);
}

sim8086_internal void print_relative_jump_immediate(relative_jump_immediate_operand_t op)
{
    uint8_t len = 0;
    char expression[8];
    {
        expression[len++] = '$';

        const uint8_t chars_written = print_signed_constant(op.value, expression+len);
        len += chars_written;

        assert(len < sim8086_array_count(expression));
        expression[len] = '\0';
    }

    printf("%s", expression);
}

sim8086_internal void print_instruction_operand(instruction_operand_t op, const char *size_expression)
{
    switch (op.kind)
    {
        case instruction_operand_kind_register:
            print_register_operand(op.payload.reg);
            break;

        case instruction_operand_kind_memory:
            print_memory_operand(op.payload.mem);
            break;

        case instruction_operand_kind_immediate:
            print_immediate(op.payload.imm, size_expression);
            break;

        case instruction_operand_kind_relative_jump_immediate:
            print_relative_jump_immediate(op.payload.rel_jump_imm);
            break;

        default:
            assert(false);
            break;
    }
}

int main(int argc, char **argv)
{
    const char *in_file_path;
    if (argc == 1)
    {
        in_file_path = "tests/listing_0037_single_register_mov";
    }
    else
    {
        in_file_path = argv[1];
    }

    uint32_t assembled_code_size = 0;
    uint8_t *assembled_code = NULL;
    {
        FILE *file = fopen(in_file_path, "rb");
        if (!file)
        {
            core_logger_log(core_logger_level_fatal, "Could not read file: %s", in_file_path);
            return -1;
        }

        fseek(file, 0, SEEK_END);
        assembled_code_size = ftell(file);

        assembled_code = (uint8_t *)malloc(assembled_code_size);

        fseek(file, 0, SEEK_SET);
        fread(assembled_code, 1, assembled_code_size, file);

        fclose(file);
    }
    assert(assembled_code != NULL);
    assert(assembled_code_size != 0);

    uint8_t *instruction_ptr = assembled_code;
    printf("bits 16\n\n");

    const char *op_mnemonic_table[] =
    {
        "mov",
        "add",
        "sub",
        "cmp",
        "jz",
        "jl",
        "jle",
        "jb",
        "jbe",
        "jp",
        "jo",
        "js",
        "jnz",
        "jnl",
        "jnle",
        "jnb",
        "jnbe",
        "jnp",
        "jno",
        "jns",
        "loop",
        "loopz",
        "loopnz",
        "jcxz"
    };
    assert(sim8086_array_count(op_mnemonic_table) == op_kind_count);

    while (instruction_ptr != assembled_code + assembled_code_size)
    {
        instruction_t decoded_instruction = { 0 };

        enum { MinOPCodeBitCount = 4 };
        enum { MaxOPCodeBitCount = 8 };

        const uint8_t opcode_byte = *instruction_ptr++;

        bool op_code_found = false;
        for (uint8_t opcode_bit_count = MaxOPCodeBitCount; opcode_bit_count >= MinOPCodeBitCount; --opcode_bit_count)
        {
            const uint8_t opcode = bitfield_extract(opcode_byte, 8-opcode_bit_count, opcode_bit_count);
            decoded_instruction.op_kind = get_op_kind(opcode, 0xFF);

            if (decoded_instruction.op_kind == op_kind_count)
                continue;

            op_code_found = true;

            switch (opcode)
            {
                case 0b100010:   // mov, Register/memory to/from register
                case 0b000000:  // add, Register/memroy to/from register
                case 0b001010:  // sub, Register/memory to/from register
                case 0b001110:  // cmp, Register/memory to/from register
                {
                    decoded_instruction.w           = bitfield_extract(opcode_byte, 0, 1);
                    const uint8_t d                 = bitfield_extract(opcode_byte, 1, 1);

                    const uint8_t mod_reg_r_m_byte = *instruction_ptr++;
                    const uint8_t mod = bitfield_extract(mod_reg_r_m_byte, 6, 2);
                    const uint8_t reg = bitfield_extract(mod_reg_r_m_byte, 3, 3);
                    const uint8_t r_m = bitfield_extract(mod_reg_r_m_byte, 0, 3);

                    const bool is_wide = (decoded_instruction.w == 0b1);

                    decoded_instruction.operands[1] = get_register_operand(reg, is_wide);

                    if (mod == 0b11)
                        decoded_instruction.operands[0] = get_register_operand(r_m, is_wide);
                    else
                        decoded_instruction.operands[0] = get_memory_operand(r_m, mod, &instruction_ptr);

                    if (d == 0b1)
                        sim8086_swap(decoded_instruction.operands[0], decoded_instruction.operands[1], instruction_operand_t);
                } break;

                case 0b1100011: // mov, Immediate to register/memory
                case 0b100000:  // add or sub or cmp, Immediate to register/memory
                {
                    const uint8_t mod_extra_opcode_r_m_byte = *instruction_ptr++;
                    const uint8_t extra_opcode = bitfield_extract(mod_extra_opcode_r_m_byte, 3, 3);
                    decoded_instruction.op_kind = get_op_kind(opcode, extra_opcode);

                    decoded_instruction.w = bitfield_extract(opcode_byte, 0, 1);
                    const bool is_wide = (decoded_instruction.w == 0b1);

                    const uint8_t s = bitfield_extract(opcode_byte, 1, 1);
                    const bool should_sign_extend = (decoded_instruction.op_kind == op_kind_mov) ? false : (s == 0b1);

                    decoded_instruction.operands[1] = get_immediate_operand(should_sign_extend, is_wide, &instruction_ptr);

                    const uint8_t mod = bitfield_extract(mod_extra_opcode_r_m_byte, 6, 2);
                    const uint8_t r_m = bitfield_extract(mod_extra_opcode_r_m_byte, 0, 3);
                    if (mod == 0b11)
                    {
                        assert(decoded_instruction.op_kind != op_kind_mov && "The mov instruction usually doesn't take this path. There is a separate opcode for this for mov.");
                        decoded_instruction.operands[0] = get_register_operand(r_m, is_wide);
                    }
                    else
                    {
                        decoded_instruction.operands[0] = get_memory_operand(r_m, mod, &instruction_ptr);
                    }
                } break;

                case 0b1011: // mov, Immediate to register
                {
                    decoded_instruction.w = bitfield_extract(opcode_byte, 3, 1);
                    const bool is_wide = (decoded_instruction.w == 0b1);

                    decoded_instruction.operands[1] = get_immediate_operand(false, is_wide, &instruction_ptr);

                    const uint8_t reg = bitfield_extract(opcode_byte, 0, 3);
                    decoded_instruction.operands[0] = get_register_operand(reg, is_wide);
                } break;

                case 0b1010000: // mov, Memory to accumulator
                case 0b1010001: // mov, Accumulator to memory
                {
                    // NOTE: Assume Memory to accumulator..
                    decoded_instruction.w = bitfield_extract(opcode_byte, 0, 1);

                    decoded_instruction.operands[1] = get_register_operand(0b000, decoded_instruction.w == 0b1);
                    decoded_instruction.operands[0] = get_memory_operand(0b110, 0b00, &instruction_ptr);

                    // NOTE: Swap if it is Accumulator to memory
                    if (opcode == 0b1010001)
                        sim8086_swap(decoded_instruction.operands[0], decoded_instruction.operands[1], instruction_operand_t);
                } break;

                case 0b0000010: // add, Immediate to accumulator
                case 0b0010110: // sub, Immediate to accumulator
                case 0b0011110: // cmp, Immediate to accumulator
                {
                    decoded_instruction.w = bitfield_extract(opcode_byte, 0, 1);
                    const bool is_wide = decoded_instruction.w == 0b1;

                    decoded_instruction.operands[1] = get_immediate_operand(false, is_wide, &instruction_ptr);
                    decoded_instruction.operands[0] = get_register_operand(0b000, is_wide);
                } break;

                case 0b01110100: // jz/je
                case 0b01111100: // jl/jnge
                case 0b01111110: // jle/jng
                case 0b01110010: // jb/jnae
                case 0b01110110: // jbe/jna
                case 0b01111010: // jp/jpe
                case 0b01110000: // jo
                case 0b01111000: // js
                case 0b01110101: // jnz/jne
                case 0b01111101: // jnl/jge
                case 0b01111111: // jnle/jg
                case 0b01110011: // jnb/jae
                case 0b01110111: // jnbe/ja
                case 0b01111011: // jnp/jpo
                case 0b01110001: // jno
                case 0b01111001: // jns
                case 0b11100010: // loop
                case 0b11100001: // loopz/loope
                case 0b11100000: // loopnz/loopne
                case 0b11100011: // jcxz
                {
                    decoded_instruction.operands[0] = get_relative_jump_immediate_operand(&instruction_ptr);
                } break;

                default:
                    assert(false);
                    continue;
            }
            
            break;
        }

        if (!op_code_found)
        {
            core_logger_log(core_logger_level_fatal, "Unknown opcode");
            return -1;
        }

        printf("%s ", op_mnemonic_table[decoded_instruction.op_kind]);

        const char *size_expression = NULL;
        print_instruction_operand(decoded_instruction.operands[0], size_expression);

        printf(", ");

        const bool should_print_size_expression =
            ((decoded_instruction.operands[0].kind == instruction_operand_kind_memory) && (decoded_instruction.operands[1].kind == instruction_operand_kind_immediate)) ||
            ((decoded_instruction.operands[0].kind == instruction_operand_kind_immediate) && (decoded_instruction.operands[1].kind == instruction_operand_kind_memory));

        if (should_print_size_expression)
            size_expression = (decoded_instruction.w == 0b0) ? "byte" : "word";

        // TODO: Sometimes we only have a single operand.
        print_instruction_operand(decoded_instruction.operands[1], size_expression);

        printf("\n");
    }

    return 0;
}