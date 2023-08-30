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

typedef struct processor_state_t processor_state_t;
struct processor_state_t
{
    uint16_t registers[register_name_count];
    uint16_t flags;
    uint16_t ip;
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
    uint16_t w;
    uint16_t size;
    const uint8_t *address;
    
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

// NOTE(achal): Returns the size of the parsed data pointed to by `data`.
static uint16_t get_memory_operand(instruction_operand_t *mem_op, uint8_t r_m, uint8_t mod, const uint8_t *data)
{
    mem_op->type = instruction_operand_type_memory;
    
    assert(mod != 0x3);
    
    switch (r_m)
    {
        case 0x0:
        mem_op->payload.mem.address_expression = memory_address_expression_bx_plus_si;
        break;
        case 0x1:
        mem_op->payload.mem.address_expression = memory_address_expression_bx_plus_di;
        break;
        case 0x2:
        mem_op->payload.mem.address_expression = memory_address_expression_bp_plus_si;
        break;
        case 0x3: 
        mem_op->payload.mem.address_expression = memory_address_expression_bp_plus_di;
        break;
        case 0x4:
        mem_op->payload.mem.address_expression = memory_address_expression_si;
        break;
        case 0x5:               
        mem_op->payload.mem.address_expression = memory_address_expression_di;
        break;
        case 0x6:
        mem_op->payload.mem.address_expression = memory_address_expression_bp;
        break;
        case 0x7:
        mem_op->payload.mem.address_expression = memory_address_expression_bx;
        break;
        default:
        assert(false);
    }
    
    const bool is_direct_address_case = (mod == 0x0 && r_m == 0x6);
    if (is_direct_address_case)
        // NOTE(achal): We use this condition later to detect if the instruction had direct address.
        mem_op->payload.mem.address_expression = memory_address_expression_count;
    
    uint16_t displacement_size = 0;
    if ((mod == 0x2) || is_direct_address_case)
    {
        displacement_size = 2;
        mem_op->payload.mem.displacement = (int32_t)(*((const uint16_t *)data));
    }
    else if (mod == 0x1)
    {
        displacement_size = 1;
        mem_op->payload.mem.displacement = (int32_t)(sign_extend_8_to_16(*data));
    }
    
    return displacement_size;
}

static uint16_t get_immediate_operand(instruction_operand_t *imm_op, bool should_sign_extend, bool is_wide, const uint8_t *data)
{
    imm_op->type = instruction_operand_type_immediate;
    
    uint16_t immediate_size = 0;
    if (!should_sign_extend && !is_wide)
    {
        immediate_size = 1;
        imm_op->payload.imm.value = (int32_t)(*data);
    }
    else if (!should_sign_extend && is_wide)
    {
        immediate_size = 2;
        imm_op->payload.imm.value = (int32_t)(*((const uint16_t *)data));
    }
    else if (should_sign_extend && is_wide)
    {
        immediate_size = 1;
        imm_op->payload.imm.value = (int32_t)(sign_extend_8_to_16(*data));
    }
    else
    {
        // NOTE(achal): This case shouldn't be possible.
        assert(false);
    }
    
    return immediate_size;
};

static uint16_t get_relative_jump_immediate_operand(instruction_operand_t *rel_op, const uint8_t *data)
{
    rel_op->type = instruction_operand_type_relative_jump_immediate;
    
    const uint8_t relative_immediate = *data;
    rel_op->payload.rel_jump_imm.value = (int32_t)(sign_extend_8_to_16(relative_immediate));
    return 1;
};

static uint16_t get_processor_flags(const uint16_t ref_value)
{
    const uint32_t Bit_ZF = 6;
    const uint32_t Bit_SF = 7;
    
    uint16_t result = 0;
    
    if (ref_value == 0) // ZF
    {
        result |= (0x1 << Bit_ZF);
    }
    else if ((int16_t)ref_value < 0) // SF
    {
        result|= (0x1 << Bit_SF);
    }
    
    return result;
}

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
        
        // NOTE(achal): NASM will add a -2 by itself to the relative jump immediate so add 2 to counter that.
        const uint8_t chars_written = print_signed_constant(op.value+2, expression+len);
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

static void print_processor_flags(FILE *file, uint16_t flags)
{
    const char *flags_name_table[] = { "CF", "", "PF", "", "AF", "", "ZF", "SF", "TF", "IF", "DF", "OF", "", "", "", "" };
    
    for (uint32_t i = 0; i < 16; ++i)
    {
        const uint16_t mask = (0x1 << i);
        if (flags & mask)
            file_print(file, "%s", flags_name_table[i]);
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
    
    processor_state_t processor_state = { 0 };
    
    // TODO(achal): Now, since, I save the starting address of each instruction I don't think
    // I have to keep track of this value anymore. This will get more and more unused and
    // hard-to-make-sense-of as I implement jumps.
    uint32_t decoded_instruction_count = 0;
    
    while (processor_state.ip < assembled_code_size)
    {
        instruction_t instruction = { 0 };
        instruction.address = assembled_code + (size_t)processor_state.ip;
        
        enum { MinOPCodeBitCount = 4 };
        enum { MaxOPCodeBitCount = 8 };
        
        const uint8_t opcode_byte = instruction.address[instruction.size++];
        
        bool op_code_found = false;
        for (uint8_t opcode_bit_count = MaxOPCodeBitCount; opcode_bit_count >= MinOPCodeBitCount; --opcode_bit_count)
        {
            const uint8_t opcode = bitfield_extract(opcode_byte, 8-opcode_bit_count, opcode_bit_count);
            
            instruction.op_type = get_op_type(opcode, 0xFF, opcode_bit_count);
            if (instruction.op_type == op_type_count)
                continue;
            
            op_code_found = true;
            
            switch (opcode)
            {
                case 0b100010:  // mov, Register/memory to/from register
                case 0b000000:  // add, Register/memroy to/from register
                case 0b001010:  // sub, Register/memory to/from register
                case 0b001110:  // cmp, Register/memory to/from register
                {
                    instruction.w = bitfield_extract(opcode_byte, 0, 1);
                    const uint8_t d = bitfield_extract(opcode_byte, 1, 1);
                    
                    const uint8_t mod_reg_r_m_byte = instruction.address[instruction.size++];
                    const uint8_t mod = bitfield_extract(mod_reg_r_m_byte, 6, 2);
                    const uint8_t reg = bitfield_extract(mod_reg_r_m_byte, 3, 3);
                    const uint8_t r_m = bitfield_extract(mod_reg_r_m_byte, 0, 3);
                    
                    const bool is_wide = (instruction.w == 0x1);
                    
                    instruction.operands[1] = get_register_operand(reg, is_wide);
                    
                    if (mod == 0x3)
                        instruction.operands[0] = get_register_operand(r_m, is_wide);
                    else
                        instruction.size += get_memory_operand(&instruction.operands[0], r_m, mod, instruction.address + instruction.size);
                    
                    if (d == 0x1)
                        core_swap(instruction.operands[0], instruction.operands[1], instruction_operand_t);
                } break;
                
                case 0b1100011: // mov, Immediate to register/memory
                case 0b100000:  // add or sub or cmp, Immediate to register/memory
                {
                    const uint8_t mod_extra_opcode_r_m_byte = instruction.address[instruction.size++];
                    const uint8_t extra_opcode = bitfield_extract(mod_extra_opcode_r_m_byte, 3, 3);
                    instruction.op_type = get_op_type(opcode, extra_opcode, opcode_bit_count);
                    if (instruction.op_type == op_type_add_sub_cmp)
                    {
                        op_code_found = false;
                        assert(false);
                        continue;
                    }
                    
                    instruction.w = bitfield_extract(opcode_byte, 0, 1);
                    const bool is_wide = (instruction.w == 0x1);
                    
                    const uint8_t s = bitfield_extract(opcode_byte, 1, 1);
                    const bool should_sign_extend = (instruction.op_type == op_type_mov) ? false : (s == 0x1);
                    
                    const uint8_t mod = bitfield_extract(mod_extra_opcode_r_m_byte, 6, 2);
                    const uint8_t r_m = bitfield_extract(mod_extra_opcode_r_m_byte, 0, 3);
                    if (mod == 0x3)
                    {
                        assert(instruction.op_type != op_type_mov && "The mov instruction usually doesn't take this path. There is a separate opcode for this for mov.");
                        instruction.operands[0] = get_register_operand(r_m, is_wide);
                    }
                    else
                    {
                        instruction.size += get_memory_operand(&instruction.operands[0], r_m, mod, instruction.address + instruction.size);
                    }
                    
                    instruction.size += get_immediate_operand(&instruction.operands[1], should_sign_extend, is_wide, instruction.address+instruction.size);
                } break;
                
                case 0b1011: // mov, Immediate to register
                {
                    instruction.w = bitfield_extract(opcode_byte, 3, 1);
                    const bool is_wide = (instruction.w == 0x1);
                    
                    instruction.size += get_immediate_operand(&instruction.operands[1], false, is_wide, instruction.address + instruction.size);
                    
                    const uint8_t reg = bitfield_extract(opcode_byte, 0, 3);
                    instruction.operands[0] = get_register_operand(reg, is_wide);
                } break;
                
                case 0b1010000: // mov, Memory to accumulator
                case 0b1010001: // mov, Accumulator to memory
                {
                    // NOTE(achal): Assume Memory to accumulator..
                    instruction.w = bitfield_extract(opcode_byte, 0, 1);
                    
                    instruction.operands[0] = get_register_operand(0x0, instruction.w == 0x1);
                    instruction.size += get_memory_operand(&instruction.operands[1], 0x6, 0x0, instruction.address + instruction.size);
                    
                    // NOTE(achal): Swap if it is Accumulator to memory
                    if (opcode == 0b1010001)
                        core_swap(instruction.operands[0], instruction.operands[1], instruction_operand_t);
                } break;
                
                case 0b0000010: // add, Immediate to accumulator
                case 0b0010110: // sub, Immediate to accumulator
                case 0b0011110: // cmp, Immediate to accumulator
                {
                    instruction.w = bitfield_extract(opcode_byte, 0, 1);
                    const bool is_wide = instruction.w == 0x1;
                    
                    instruction.size += get_immediate_operand(&instruction.operands[1], false, is_wide, instruction.address + instruction.size);
                    instruction.operands[0] = get_register_operand(0x0, is_wide);
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
                    instruction.size += get_relative_jump_immediate_operand(&instruction.operands[0], instruction.address + instruction.size);
                    instruction.operands[1].type = instruction_operand_type_count;
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
            
            const processor_state_t prev_processor_state = processor_state;
            
            processor_state.ip += instruction.size;
            
            if (is_execution_mode)
            {
                if (out_file_path)
                {
                    const char *extension = strrchr(out_file_path, '.');
                    
                    const char txt_extension_str[] = ".txt";
                    if (!extension || (strcmp(extension, txt_extension_str) != 0))
                        LOG_WARNING("Execution mode is enabled, output will be a text file but .txt extension not detected in the output path: %s", out_file_path);
                }
                print_instruction(out_file, &instruction);
                
                const instruction_operand_t *src_op = &instruction.operands[1];
                const instruction_operand_t *dst_op = &instruction.operands[0];
                if (src_op->type == instruction_operand_type_count)
                {
                    src_op = dst_op;
                    dst_op = NULL;
                }
                
                uint32_t src_value = 0xFFFFFFFF;
                switch (src_op->type)
                {
                    case instruction_operand_type_register:
                    {
                        const uint32_t src_reg_idx = (uint32_t)src_op->payload.reg.name;
                        src_value = (uint32_t)processor_state.registers[src_reg_idx];
                    } break;
                    
                    case instruction_operand_type_memory:
                    {
                        assert(false);
                    } break;
                    
                    case instruction_operand_type_immediate:
                    {
                        src_value = (uint32_t)src_op->payload.imm.value;
                    } break;
                    
                    case instruction_operand_type_relative_jump_immediate:
                    {
                        src_value = (uint32_t)src_op->payload.rel_jump_imm.value;
                    } break;
                }
                assert(src_value != 0xFFFFFFFF);
                
                uint32_t dst_reg_idx = UINT32_MAX;
                if (dst_op)
                {
                    assert(dst_op->type == instruction_operand_type_register);
                    dst_reg_idx = (uint32_t)dst_op->payload.reg.name;
                }
                
                switch (instruction.op_type)
                {
                    case op_type_mov:
                    {
                        processor_state.registers[dst_reg_idx] = (uint16_t)src_value;
                    } break;
                    
                    case op_type_add:
                    {
                        processor_state.registers[dst_reg_idx] += (uint16_t)src_value;
                        processor_state.flags = get_processor_flags(processor_state.registers[dst_reg_idx]);
                    } break;
                    
                    case op_type_sub:
                    {
                        processor_state.registers[dst_reg_idx] -= (uint16_t)src_value;
                        processor_state.flags = get_processor_flags(processor_state.registers[dst_reg_idx]);
                    } break;
                    
                    case op_type_cmp:
                    {
                        uint16_t temp = processor_state.registers[dst_reg_idx] - (uint16_t)src_value;
                        processor_state.flags = get_processor_flags(temp);
                    } break;
                    
                    case op_type_jnz:
                    {
                        const uint32_t Bit_ZF = 6;
                        if ((processor_state.flags & (0x1 << Bit_ZF)) == 0)
                        {
                            assert(processor_state.ip != 0);
                            processor_state.ip += (uint16_t)src_value;
                        }
                    } break;
                    
                    default:
                    assert(false);
                    break;
                }
                
                file_print(out_file, " ;");
                
                if (dst_op)
                {
                    assert(dst_reg_idx != UINT32_MAX);
                    
                    if (prev_processor_state.registers[dst_reg_idx] != processor_state.registers[dst_reg_idx])
                    {
                        const char *reg_name = get_register_name(&dst_op->payload.reg);
                        file_print(out_file, " %s:0x%X->0x%X", reg_name, prev_processor_state.registers[dst_reg_idx], processor_state.registers[dst_reg_idx]);
                    }
                }
                
                if (prev_processor_state.ip != processor_state.ip)
                {
                    file_print(out_file, " ip:0x%X->0x%X", prev_processor_state.ip, processor_state.ip);
                }
                
                if (prev_processor_state.flags != processor_state.flags)
                {
                    file_print(out_file, " flags:");
                    print_processor_flags(out_file, prev_processor_state.flags);
                    file_print(out_file, "->");
                    print_processor_flags(out_file, processor_state.flags);
                }
            }
            else
            {
                print_instruction(out_file, &instruction);
            }
            
            file_print(out_file, "\n");
        }
    }
    
    if (is_execution_mode)
    {
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
            
            file_print(out_file, "\t%s: 0x%04X (%u)\n", name, processor_state.registers[i], processor_state.registers[i]);
        }
        
        file_print(out_file, "\tip: 0x%04X (%u)\n", processor_state.ip, processor_state.ip);
        
        file_print(out_file, "\tflags: ");
        print_processor_flags(out_file, processor_state.flags);
        file_print(out_file, "\n");
    }
    
    if (out_file != stdout)
        fclose(out_file);
    
    LOG_INFO("Instructions decoded: %u", decoded_instruction_count);
    
    return 0;
}