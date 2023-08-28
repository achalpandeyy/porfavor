#include "core/logger/logger.h"

#define LOG_INFO(fmt, ...) core_logger_log(core_logger_level_info, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) core_logger_log(core_logger_level_debug, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) core_logger_log(core_logger_level_warning, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) core_logger_log(core_logger_level_fatal, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) core_logger_log(core_logger_level_error, fmt, ##__VA_ARGS__)

#include <stdlib.h>
#include <string.h>

static uint8_t bitfield_extract(uint8_t value, uint8_t offset, uint8_t count)
{
    const uint8_t mask = (1 << count) - 1;
    const uint8_t result = (value >> offset) & mask;
    return result;
}

static int16_t sign_extend_8_to_16(uint8_t value)
{
    const int16_t result = (value & 0x80) ? (0xFF00 | value) : value;
    return result;
}

typedef enum register_name_t register_name_t;
enum register_name_t
{
    register_name_a = 0,
    register_name_b,
    register_name_c,
    register_name_d,
    register_name_sp,
    register_name_bp,
    register_name_si,
    register_name_di,
    
    register_name_count
};

typedef struct register_operand_t register_operand_t;
struct register_operand_t
{
    register_name_t name;
    uint8_t offset;
    uint8_t count;
};

typedef enum memory_address_expression_t memory_address_expression_t;
enum memory_address_expression_t
{
    memory_address_expression_bx_plus_si = 0,
    memory_address_expression_bx_plus_di,
    memory_address_expression_bp_plus_si,
    memory_address_expression_bp_plus_di,
    memory_address_expression_si,
    memory_address_expression_di,
    memory_address_expression_bp,
    memory_address_expression_bx,
    
    memory_address_expression_count
};

typedef struct memory_operand_t memory_operand_t;
struct memory_operand_t
{
    memory_address_expression_t address_expression;
    int32_t displacement; // just 16 bits should do but the compiler will pad it up anyways, so why not just claim the memory
};

typedef struct immediate_operand_t immediate_operand_t;
struct immediate_operand_t
{
    int32_t value;
};

typedef struct relative_jump_immediate_operand_t relative_jump_immediate_operand_t;
struct relative_jump_immediate_operand_t
{
    int32_t value;
};

typedef enum instruction_operand_type_t instruction_operand_type_t;
enum instruction_operand_type_t
{
    instruction_operand_type_register = 0,
    instruction_operand_type_memory,
    instruction_operand_type_immediate,
    instruction_operand_type_relative_jump_immediate,
    
    instruction_operand_type_count
};

typedef struct instruction_operand_t instruction_operand_t;
struct instruction_operand_t
{
    instruction_operand_type_t type;
    union
    {
        register_operand_t reg;
        memory_operand_t mem;
        immediate_operand_t imm;
        relative_jump_immediate_operand_t rel_jump_imm;
    } payload;
};

typedef enum op_type_t op_type_t;
enum op_type_t
{
    op_type_mov = 0,
    op_type_add,
    op_type_sub,
    op_type_cmp,
    op_type_add_sub_cmp,
    op_type_jz,     // je
    op_type_jl,     // jnge
    op_type_jle,    // jng
    op_type_jb,     // jnae
    op_type_jbe,    // jna
    op_type_jp,     // jpe
    op_type_jo,
    op_type_js,
    op_type_jnz,    // jne
    op_type_jnl,    // jge
    op_type_jnle,   // jg
    op_type_jnb,    // jae
    op_type_jnbe,   // ja
    op_type_jnp,    // jpo
    op_type_jno,
    op_type_jns,
    op_type_loop,
    op_type_loopz,  // loope
    op_type_loopnz, // loopne
    op_type_jcxz,
    
    op_type_count
};

static __forceinline op_type_t get_op_type(uint8_t opcode, uint8_t extra_opcode, uint32_t bits_to_match)
{
    switch (bits_to_match)
    {
        case 8:
        {
            switch (opcode)
            {
                case 0x74: // 01110100
                return op_type_jz;
                case 0x7C: // 01111100
                return op_type_jl;
                case 0x7E: // 01111110
                return op_type_jle;
                case 0x72: // 01110010
                return op_type_jb;
                case 0x76: // 01110110
                return op_type_jbe;
                case 0x7A: // 01111010
                return op_type_jp;
                case 0x70: // 01110000
                return op_type_jo;
                case 0x78: // 01111000
                return op_type_js;
                case 0x75: // 01110101
                return op_type_jnz;
                case 0x7D: // 01111101
                return op_type_jnl;
                case 0x7F: // 01111111
                return op_type_jnle;
                case 0x73: // 01110011
                return op_type_jnb;
                case 0x77: // 01110111
                return op_type_jnbe;
                case 0x7B: // 01111011
                return op_type_jnp;
                case 0x71: // 01110001
                return op_type_jno;
                case 0x79: // 01111001
                return op_type_jns;
                case 0xE2: // 11100010
                return op_type_loop;
                case 0xE1: // 11100001
                return op_type_loopz;
                case 0xE0: // 11100000
                return op_type_loopnz;
                case 0xE3: // 11100011
                return op_type_jcxz;
            }
        } break;
        
        case 7:
        {
            switch (opcode)
            {
                case 0x63: // 1100011
                case 0x50: // 1010000
                case 0x51: // 1010001
                return op_type_mov;
                
                case 0x02: // 0000010
                return op_type_add;
                
                case 0x16: // 0010110
                return op_type_sub;
                
                case 0x1E: // 0011110
                return op_type_cmp;
            }
        } break;
        
        case 6:
        {
            switch (opcode)
            {
                case 0x22: // 100010
                return op_type_mov;
                
                case 0x0: // 000000
                return op_type_add;
                
                case 0x0A: // 001010
                return op_type_sub;
                
                case 0x0E: // 001110
                return op_type_cmp;
                
                case 0x20: // 100000
                {
                    switch (extra_opcode)
                    {
                        case 0x0: // 000
                        return op_type_add;
                        
                        case 0x5: // 101
                        return op_type_sub;
                        
                        case 0x7: // 111
                        return op_type_cmp;
                        
                        // NOTE(achal): Sometimes you do not have the extra_opcode yet to figure out _exactly_
                        // which opcode it is supposed to be so just return this special enum value. I think,
                        // eventually I will make it just op_type_arithmetic because more arithmetic instructions
                        // follow this extra opcode pattern.
                        default:
                        return op_type_add_sub_cmp;
                    }
                }
            }
        } break;
        
        case 5:
        {
        } break;
        
        case 4:
        {
            case 0x11: // 1011
            return op_type_mov;
        } break;
    }
    
    return op_type_count;
}

typedef struct instruction_t instruction_t;
struct instruction_t
{
    op_type_t op_type;
    uint8_t w;
    
    // NOTE: operands[0] is the the one which appears first in
    // the disassembly instruction, this is usually the destination operand.
    // For instructions which have only one operand, operand[0] will be used.
    instruction_operand_t operands[2];
};

static instruction_operand_t get_register_operand(uint8_t index, bool is_wide)
{
    instruction_operand_t result = { 0 };
    result.type = instruction_operand_type_register;
    result.payload.reg.count = is_wide ? 2 : 1;
    
    switch (index)
    {
        case 0x0:
        {
            result.payload.reg.name     = register_name_a;
            result.payload.reg.offset   = 0;
        } break;
        
        case 0x1:
        {
            result.payload.reg.name     = register_name_c;
            result.payload.reg.offset   = 0;
        } break;
        
        case 0x2:
        {
            result.payload.reg.name     = register_name_d;
            result.payload.reg.offset   = 0;
        } break;
        
        case 0x3:
        {
            result.payload.reg.name     = register_name_b;
            result.payload.reg.offset   = 0;
        } break;
        
        case 0x4:
        {
            result.payload.reg.name     = is_wide ? register_name_sp : register_name_a;
            result.payload.reg.offset   = is_wide ? 0 : 1;
        } break;
        
        case 0x5:
        {
            result.payload.reg.name     = is_wide ? register_name_bp : register_name_c;
            result.payload.reg.offset   = is_wide ? 0 : 1;
        } break;
        
        case 0x6:
        {
            result.payload.reg.name     = is_wide ? register_name_si : register_name_d;
            result.payload.reg.offset   = is_wide ? 0 : 1;
        } break;
        
        case 0x7:
        {
            result.payload.reg.name     = is_wide ? register_name_di : register_name_b;
            result.payload.reg.offset   = is_wide ? 0 : 1;
        } break;
        
        default:
        assert(false);
    }
    
    return result;
}

static instruction_operand_t get_memory_operand(uint8_t r_m, uint8_t mod, uint8_t **instruction_ptr)
{
    instruction_operand_t result = { 0 };
    result.type = instruction_operand_type_memory;
    
    assert(mod != 0x3);
    
    switch (r_m)
    {
        case 0x0:
        result.payload.mem.address_expression = memory_address_expression_bx_plus_si;
        break;
        case 0x1:
        result.payload.mem.address_expression = memory_address_expression_bx_plus_di;
        break;
        case 0x2:
        result.payload.mem.address_expression = memory_address_expression_bp_plus_si;
        break;
        case 0x3: 
        result.payload.mem.address_expression = memory_address_expression_bp_plus_di;
        break;
        case 0x4:
        result.payload.mem.address_expression = memory_address_expression_si;
        break;
        case 0x5:               
        result.payload.mem.address_expression = memory_address_expression_di;
        break;
        case 0x6:
        result.payload.mem.address_expression = memory_address_expression_bp;
        break;
        case 0x7:
        result.payload.mem.address_expression = memory_address_expression_bx;
        break;
        default:
        assert(false);
    }
    
    const bool is_direct_address_case = (mod == 0b00 && r_m == 0b110);
    if (is_direct_address_case)
        // NOTE(achal): We use this condition later to detect if the instruction had direct address.
        result.payload.mem.address_expression = memory_address_expression_count;
    
    uint8_t displacement_size = 0;
    const uint8_t *displacement = *instruction_ptr;
    if ((mod == 0x2) || is_direct_address_case)
    {
        displacement_size = 2;
        result.payload.mem.displacement = (int32_t)(*((const uint16_t *)displacement));
    }
    else if (mod == 0x1)
    {
        displacement_size = 1;
        result.payload.mem.displacement = (int32_t)(sign_extend_8_to_16(*displacement));
    }
    
    *instruction_ptr = *instruction_ptr + displacement_size;
    
    return result;
}

static instruction_operand_t get_immediate_operand(bool should_sign_extend, bool is_wide, uint8_t **instruction_ptr)
{
    instruction_operand_t result;
    result.type = instruction_operand_type_immediate;
    
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

static instruction_operand_t get_relative_jump_immediate_operand(uint8_t **instruction_ptr)
{
    instruction_operand_t result;
    result.type = instruction_operand_type_relative_jump_immediate;
    
    const uint8_t relative_immediate = **instruction_ptr;
    // NOTE: NASM will add a -2 by itself to the displacement so add 2 to counter that.
    result.payload.rel_jump_imm.value = (int32_t)(sign_extend_8_to_16(relative_immediate)) + 2;
    *instruction_ptr = *instruction_ptr + 1;
    return result;
};

#define file_print(file, fmt, ...)\
{\
const int retval = fprintf(file, fmt, ##__VA_ARGS__);\
assert(retval >= 0);\
}

static uint8_t print_signed_constant(int32_t constant, char *dst)
{
    uint8_t result = 0;
    if (constant >= 0)
        dst[result++] = '+';
    
    // TODO(achal): Could I have just used "%+d"?
    const uint8_t chars_written = (uint8_t)sprintf(dst+result, "%d", constant);
    result += chars_written;
    
    return result;
}

static __forceinline const char * get_register_name(const register_operand_t *op)
{
    const char *names[][3] =
    {
        { "al", "ah", "ax" },
        { "bl", "bh", "bx" },
        { "cl", "ch", "cx" },
        { "dl", "dh", "dx" },
        { "sp", "sp", "sp" },
        { "bp", "bp", "bp" },
        { "si", "si", "si" },
        { "di", "di", "di" },
    };
    
    return names[op->name][(op->offset == 1) ? 1 : (op->count == 2 ? 2 : 0)];
}

static void print_register_operand(FILE *file, register_operand_t op)
{
    const char *name = get_register_name(&op);
    file_print(file, "%s", name);
}

static void print_memory_operand(FILE *file, memory_operand_t op)
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
                "bp",
                "bx"
            };
            
            const uint8_t chars_written = (uint8_t)sprintf(expression+len, "%s", table[op.address_expression]);
            len += chars_written;
        }
        
        {
            const bool has_direct_address = (op.address_expression == memory_address_expression_count);
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
                // NOTE(achal): Displacement to memory addresses should always fit in 16 bits.
                assert(op.displacement <= UINT16_MAX);
                const uint8_t chars_written = print_signed_constant(op.displacement, expression+len);
                len += chars_written;
            }
        }
        
        expression[len++] = ']';
        
        assert(len < core_array_count(expression));
        expression[len] = '\0';
    }
    
    file_print(file, "%s", expression);
}

static void print_immediate(FILE *file, immediate_operand_t op, const char *size_expression)
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
        
        assert(len < core_array_count(expression));
        expression[len] = '\0';
    }
    
    file_print(file, "%s", expression);
}

static void print_relative_jump_immediate(FILE *file, relative_jump_immediate_operand_t op)
{
    uint8_t len = 0;
    char expression[8];
    {
        expression[len++] = '$';
        
        const uint8_t chars_written = print_signed_constant(op.value, expression+len);
        len += chars_written;
        
        assert(len < core_array_count(expression));
        expression[len] = '\0';
    }
    
    file_print(file, "%s", expression);
}

static void print_instruction_operand(FILE *file, instruction_operand_t op, const char *size_expression)
{
    switch (op.type)
    {
        case instruction_operand_type_register:
        print_register_operand(file, op.payload.reg);
        break;
        
        case instruction_operand_type_memory:
        print_memory_operand(file, op.payload.mem);
        break;
        
        case instruction_operand_type_immediate:
        print_immediate(file, op.payload.imm, size_expression);
        break;
        
        case instruction_operand_type_relative_jump_immediate:
        print_relative_jump_immediate(file, op.payload.rel_jump_imm);
        break;
        
        default:
        assert(false);
        break;
    }
}

static void print_instruction(FILE *file, const instruction_t *instruction)
{
    const char *op_mnemonic_table[] =
    {
        "mov",
        "add",
        "sub",
        "cmp",
        "", // add_sub_cmp
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
    assert(core_array_count(op_mnemonic_table) == op_type_count);
    
    assert(instruction->op_type != op_type_add_sub_cmp);
    
    const bool should_print_size_expression =
    ((instruction->operands[0].type == instruction_operand_type_memory) && (instruction->operands[1].type == instruction_operand_type_immediate)) ||
    ((instruction->operands[0].type == instruction_operand_type_immediate) && (instruction->operands[1].type == instruction_operand_type_memory));
    
    const char byte_str[] = "byte";
    const char word_str[] = "word";
    const char *size_expression = NULL;
    if (should_print_size_expression)
        size_expression = (instruction->w == 0x0) ? byte_str : word_str;
    
    file_print(file, "%s ", op_mnemonic_table[instruction->op_type]);
    print_instruction_operand(file, instruction->operands[0], NULL);
    if (instruction->operands[1].type != instruction_operand_type_count)
    {
        file_print(file, ", ");
        print_instruction_operand(file, instruction->operands[1], size_expression);
    }
}

int main(int argc, char **argv)
{
    assert(argc >= 1);
    if (argc == 1)
    {
        LOG_FATAL("Usage: porfavor <path_to_asm_file>");
        return -1;
    }
    
    bool is_execution_mode = false;
    const char *in_file_path = NULL;
    const char *out_file_path = NULL;
    
    // NOTE(achal): We assume that the first path will always be to the input file.
    for (int i = 1; i < argc; ++i)
    {
        const char exec_flag_str[] = "-exec";
        if (strcmp(argv[i], exec_flag_str) == 0)
        {
            is_execution_mode = true;
            continue;
        }
        
        if (!in_file_path)
        {
            in_file_path = argv[i];
            continue;
        }
        
        if (!out_file_path)
        {
            out_file_path = argv[i];
            continue;
        }
        
        LOG_WARNING("Unknown flag recieved: %s. Ignoring..", argv[i]);
    }
    
    uint32_t assembled_code_size = 0;
    uint8_t *assembled_code = NULL;
    {
        if (!in_file_path)
        {
            LOG_FATAL("No input file path specified");
            return -1;
        }
        
        FILE *file = fopen(in_file_path, "rb");
        if (!file)
        {
            LOG_FATAL("Could not read file: %s", in_file_path);
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
    
    // Open the output file
    FILE *out_file = stdout;
    if (out_file_path)
    {
        out_file = fopen(out_file_path, "w");
        if (!out_file)
        {
            LOG_ERROR("Could not open a file for writing: %s", out_file_path);
            return -1;
        }
    }
    
    if (is_execution_mode)
    {
        const char *file_name = in_file_path;
        {
            const char *listing_name = strchr(in_file_path, '/');
            if (!listing_name)
                listing_name = strchr(in_file_path, '\\');
            
            if (listing_name && (strlen(listing_name) > 1))
                file_name = listing_name+1; // eat up the slash
        }
        
        file_print(out_file, "--- test\\%s execution ---\n", file_name);
    }
    else
    {
        file_print(out_file, "bits 16\n\n");
    }
    
    uint16_t registers[register_name_count];
    memset(registers, 0, sizeof(registers));
    
    uint32_t decoded_instruction_count = 0;
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
            
            decoded_instruction.op_type = get_op_type(opcode, 0xFF, opcode_bit_count);
            if (decoded_instruction.op_type == op_type_count)
                continue;
            
            op_code_found = true;
            
            switch (opcode)
            {
                case 0b100010:  // mov, Register/memory to/from register
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
                    
                    const bool is_wide = (decoded_instruction.w == 0x1);
                    
                    decoded_instruction.operands[1] = get_register_operand(reg, is_wide);
                    
                    if (mod == 0x3)
                        decoded_instruction.operands[0] = get_register_operand(r_m, is_wide);
                    else
                        decoded_instruction.operands[0] = get_memory_operand(r_m, mod, &instruction_ptr);
                    
                    if (d == 0x1)
                        core_swap(decoded_instruction.operands[0], decoded_instruction.operands[1], instruction_operand_t);
                } break;
                
                case 0b1100011: // mov, Immediate to register/memory
                case 0b100000:  // add or sub or cmp, Immediate to register/memory
                {
                    const uint8_t mod_extra_opcode_r_m_byte = *instruction_ptr++;
                    const uint8_t extra_opcode = bitfield_extract(mod_extra_opcode_r_m_byte, 3, 3);
                    decoded_instruction.op_type = get_op_type(opcode, extra_opcode, opcode_bit_count);
                    if (decoded_instruction.op_type == op_type_add_sub_cmp)
                    {
                        op_code_found = false;
                        assert(false);
                        continue;
                    }
                    
                    decoded_instruction.w = bitfield_extract(opcode_byte, 0, 1);
                    const bool is_wide = (decoded_instruction.w == 0b1);
                    
                    const uint8_t s = bitfield_extract(opcode_byte, 1, 1);
                    const bool should_sign_extend = (decoded_instruction.op_type == op_type_mov) ? false : (s == 0b1);
                    
                    const uint8_t mod = bitfield_extract(mod_extra_opcode_r_m_byte, 6, 2);
                    const uint8_t r_m = bitfield_extract(mod_extra_opcode_r_m_byte, 0, 3);
                    if (mod == 0b11)
                    {
                        assert(decoded_instruction.op_type != op_type_mov && "The mov instruction usually doesn't take this path. There is a separate opcode for this for mov.");
                        decoded_instruction.operands[0] = get_register_operand(r_m, is_wide);
                    }
                    else
                    {
                        decoded_instruction.operands[0] = get_memory_operand(r_m, mod, &instruction_ptr);
                    }
                    
                    decoded_instruction.operands[1] = get_immediate_operand(should_sign_extend, is_wide, &instruction_ptr);
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
                    // NOTE(achal): Assume Memory to accumulator..
                    decoded_instruction.w = bitfield_extract(opcode_byte, 0, 1);
                    
                    decoded_instruction.operands[0] = get_register_operand(0b000, decoded_instruction.w == 0b1);
                    decoded_instruction.operands[1] = get_memory_operand(0b110, 0b00, &instruction_ptr);
                    
                    // NOTE(achal): Swap if it is Accumulator to memory
                    if (opcode == 0b1010001)
                        core_swap(decoded_instruction.operands[0], decoded_instruction.operands[1], instruction_operand_t);
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
                    decoded_instruction.operands[1].type = instruction_operand_type_count;
                } break;
                
                default:
                assert(false);
                continue;
            }
            
            break;
        }
        
        if (!op_code_found)
        {
            LOG_FATAL("Unknown opcode");
            return -1;
        }
        
        ++decoded_instruction_count;
        
        // Print instructions and/or trace
        {
            assert(out_file);
            
            if (is_execution_mode)
            {
                if (out_file_path)
                {
                    const char *extension = strrchr(out_file_path, '.');
                    
                    const char txt_extension_str[] = ".txt";
                    if (!extension || (strcmp(extension, txt_extension_str) != 0))
                        LOG_WARNING("Execution mode is enabled, output will be a text file but .txt extension not detected in the output path: %s", out_file_path);
                }
                print_instruction(out_file, &decoded_instruction);
                
                assert(decoded_instruction.operands[0].type == instruction_operand_type_register);
                const uint32_t reg_idx = (uint32_t)decoded_instruction.operands[0].payload.reg.name;
                
                const uint16_t old_value = registers[reg_idx];
                
                assert(decoded_instruction.op_type == op_type_mov);
                assert(decoded_instruction.operands[1].type == instruction_operand_type_immediate);
                registers[reg_idx] = (uint16_t)decoded_instruction.operands[1].payload.imm.value;
                
                const char *reg_name = get_register_name(&decoded_instruction.operands[0].payload.reg);
                file_print(out_file, " ; %s:0x%X->0x%X", reg_name, old_value, registers[reg_idx]);
            }
            else
            {
                print_instruction(out_file, &decoded_instruction);
            }
            
            file_print(out_file, "\n");
        }
    }
    
    file_print(out_file, "\nFinal registers:\n");
    for (uint32_t i = 0; i < register_name_count; ++i)
    {
        const char *name = NULL;
        {
            register_operand_t reg = { 0 };
            reg.name = i;
            reg.count = 2;
            name = get_register_name(&reg);
        }
        
        file_print(out_file, "\t%s: 0x%04X (%u)\n", name, registers[i], registers[i]);
    }
    
    if (out_file != stdout)
        fclose(out_file);
    
    LOG_INFO("Instructions decoded: %u", decoded_instruction_count);
    
    return 0;
}