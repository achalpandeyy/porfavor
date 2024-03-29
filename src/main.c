#include "logger.h"

#define LOG_INFO(fmt, ...) core_logger_log(core_logger_level_info, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) core_logger_log(core_logger_level_debug, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) core_logger_log(core_logger_level_warning, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) core_logger_log(core_logger_level_fatal, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) core_logger_log(core_logger_level_error, fmt, ##__VA_ARGS__)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int16_t s16;
typedef int32_t s32;

#include <stdlib.h>
#include <string.h>

static u8 bitfield_extract(u8 value, u8 offset, u8 count)
{
    const u8 mask = (1 << count) - 1;
    const u8 result = (value >> offset) & mask;
    return result;
}

static s16 sign_extend_8_to_16(u8 value)
{
    const s16 result = (value & 0x80) ? (0xFF00 | value) : value;
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
    u16 ip;
    
    // NOTE(achal): These members are only required in simulation mode
    u16 flags;
    u16 registers[register_name_count];
    u8 *memory; // 1 MB
};

typedef struct register_operand_t register_operand_t;
struct register_operand_t
{
    register_name_t name;
    u8 offset;
    u8 count;
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
    s32 displacement; // just 16 bits should do but the compiler will pad it up anyways, so why not just claim the memory
};

typedef struct immediate_operand_t immediate_operand_t;
struct immediate_operand_t
{
    s32 value;
};

typedef struct relative_jump_immediate_operand_t relative_jump_immediate_operand_t;
struct relative_jump_immediate_operand_t
{
    s32 value;
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

static __forceinline op_type_t get_op_type(u8 opcode, u8 extra_opcode, uint32_t bits_to_match)
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
    const u8 *address;
    
    // NOTE: operands[0] is the the one which appears first in
    // the disassembly instruction, this is usually the destination operand.
    // For instructions which have only one operand, operand[0] will be used.
    instruction_operand_t operands[2];
    
    u32 op_clocks;
    u32 ea_clocks;
};

static instruction_operand_t get_register_operand(u8 index, bool is_wide)
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
static uint16_t get_memory_operand(instruction_operand_t *mem_op, u8 r_m, u8 mod, const u8 *data)
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
    
    u16 displacement_size = 0;
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

static uint16_t get_immediate_operand(instruction_operand_t *imm_op, bool should_sign_extend, bool is_wide, const u8 *data)
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

static uint16_t get_relative_jump_immediate_operand(instruction_operand_t *rel_op, const u8 *data)
{
    rel_op->type = instruction_operand_type_relative_jump_immediate;
    
    const u8 relative_immediate = *data;
    rel_op->payload.rel_jump_imm.value = (int32_t)(sign_extend_8_to_16(relative_immediate));
    return 1;
};

static u32 get_effective_address(const memory_operand_t *mem_op, const processor_state_t *processor_state)
{
    u32 result = 0;
    switch (mem_op->address_expression)
    {
        case memory_address_expression_bx_plus_si:
        {
            const u16 bx = processor_state->registers[register_name_b];
            const u16 si = processor_state->registers[register_name_si];
            result += bx+si;
        } break;
        
        case memory_address_expression_bx_plus_di:
        {
            const u16 bx = processor_state->registers[register_name_b];
            const u16 di = processor_state->registers[register_name_di];
            result += bx+di;
        } break;
        
        case memory_address_expression_bp_plus_si:
        {
            const u16 bp = processor_state->registers[register_name_bp];
            const u16 si = processor_state->registers[register_name_si];
            result += bp+si;
        } break;
        
        case memory_address_expression_bp_plus_di:
        {
            const u16 bp = processor_state->registers[register_name_bp];
            const u16 di = processor_state->registers[register_name_di];
            result += bp+di;
        } break;
        
        case memory_address_expression_si:
        {
            const u16 si = processor_state->registers[register_name_si];
            result += si;
        } break;
        
        case memory_address_expression_di:
        {
            const u16 di = processor_state->registers[register_name_di];
            result += di;
        } break;
        
        case memory_address_expression_bp:
        {
            const u16 bp = processor_state->registers[register_name_bp];
            result += bp;
        } break;
        
        case memory_address_expression_bx:
        {
            const u16 bx = processor_state->registers[register_name_b];
            result += bx;
        } break;
    }
    
    result += mem_op->displacement;
    
    return result;
}

static u16 get_processor_flags(const u16 value)
{
    const uint32_t Bit_ZF = 6;
    const uint32_t Bit_SF = 7;
    
    u16 result = 0;
    
    if (value == 0) // ZF
    {
        result |= (0x1 << Bit_ZF);
    }
    else if ((s16)value < 0) // SF
    {
        result|= (0x1 << Bit_SF);
    }
    
    return result;
}

static u32 get_effective_address_clocks(memory_address_expression_t expression, int has_disp)
{
    if (expression == memory_address_expression_count)
    {
        // NOTE(achal): Direct address case
        assert(has_disp);
        return 6;
    }
    
    // NOTE(achal): Table 2-20 of the manual
    static u32 ea_clocks_table[] = { 7, 8, 8, 7, 5, 5, 5, 5 };
    assert(core_array_count(ea_clocks_table) == memory_address_expression_count);
    
    u32 result = ea_clocks_table[expression];
    if (has_disp)
        result += 4;
    return result;
}

//- NOTE(achal): Printing

#define file_print(file, fmt, ...)\
{\
const int retval = fprintf(file, fmt, ##__VA_ARGS__);\
assert(retval >= 0);\
}

static u8 print_signed_constant(int32_t constant, char *dst)
{
    u8 result = 0;
    if (constant >= 0)
        dst[result++] = '+';
    
    // TODO(achal): Could I have just used "%+d"?
    const u8 chars_written = (u8)sprintf(dst+result, "%d", constant);
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
    u8 len = 0;
    char expression[32];
    {
        expression[len++] = '[';
        
        const bool has_direct_address = (op.address_expression == memory_address_expression_count);
        
        if (!has_direct_address)
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
            
            assert(op.address_expression < memory_address_expression_count);
            const u8 chars_written = (u8)sprintf(expression+len, "%s", table[op.address_expression]);
            len += chars_written;
        }
        
        {
            const bool has_disp = !has_direct_address && (op.displacement != 0);
            
            if (has_direct_address)
            {
                const u8 chars_written = (u8)sprintf(expression+len, "%d", op.displacement);
                // sprintf will also write the null character, but we will overwrite it because
                // we still need to add more stuff to the string. Also note that `chars_written`
                // will not include the null character.
                len += chars_written;
            }
            else if (has_disp)
            {
                // NOTE(achal): Displacement to memory addresses should always fit in 16 bits.
                assert(op.displacement <= UINT16_MAX);
                const u8 chars_written = print_signed_constant(op.displacement, expression+len);
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
    u8 len = 0;
    char expression[16];
    {
        if (size_expression)
        {
            const u8 chars_written = (u8)sprintf(expression+len, "%s ", size_expression);
            len += chars_written;
        }
        
        {
            const u8 chars_written = (u8)sprintf(expression+len, "%d", op.value);
            len += chars_written;
        }
        
        assert(len < core_array_count(expression));
        expression[len] = '\0';
    }
    
    file_print(file, "%s", expression);
}

static void print_relative_jump_immediate(FILE *file, relative_jump_immediate_operand_t op)
{
    u8 len = 0;
    char expression[8];
    {
        expression[len++] = '$';
        
        // NOTE(achal): NASM will add a -2 by itself to the relative jump immediate so add 2 to counter that.
        const u8 chars_written = print_signed_constant(op.value+2, expression+len);
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

static void print_processor_flags(FILE *file, u16 flags)
{
    const char *flags_name_table[] = { "CF", "", "PF", "", "AF", "", "ZF", "SF", "TF", "IF", "DF", "OF", "", "", "", "" };
    
    for (uint32_t i = 0; i < 16; ++i)
    {
        const u16 mask = (0x1 << i);
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
    
    bool is_simulation_mode = false;
    bool should_dump_memory = false;
    bool show_clocks = false;
    bool explain_clocks = false;
    const char *in_file_path = NULL;
    const char *out_file_path = NULL;
    
    // NOTE(achal): We assume that the first path will always be to the input file.
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '-')
        {
            ++argv[i];
            
            const char exec_flag_str[] = "exec";
            const char dump_flag_str[] = "dump";
            const char showclocks_str[] = "showclocks";
            const char explainclocks_str[] = "explainclocks";
            
            if (strcmp(argv[i], exec_flag_str) == 0)
            {
                is_simulation_mode = true;
            }
            else if (strcmp(argv[i], dump_flag_str) == 0)
            {
                should_dump_memory = true;
            }
            else if (strcmp(argv[i], showclocks_str) == 0)
            {
                show_clocks = true;
            }
            else if (strcmp(argv[i], explainclocks_str) == 0)
            {
                explain_clocks = true;
            }
            else
            {
                LOG_WARNING("Unknown flag: %s", argv[i]);
            }
        }
        else if (!in_file_path)
        {
            in_file_path = argv[i];
        }
        else if (!out_file_path)
        {
            out_file_path = argv[i];
        }
    }
    
    u32 assembled_code_size = 0;
    u8 *assembled_code = NULL;
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
        
        // TODO(achal): This should go onto the processor's 1MB memory
        assembled_code = (u8 *)malloc(assembled_code_size);
        
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
    
    file_print(out_file, "bits 16\n\n");
    
    enum { ProcessorMemorySize = 1*1024*1024u };
    processor_state_t processor_state = { 0 };
    if (is_simulation_mode)
    {
        processor_state.memory = (u8 *)malloc(ProcessorMemorySize);
        assert(processor_state.memory);
    }
    u32 total_clocks = 0;
    
    while (processor_state.ip < assembled_code_size)
    {
        instruction_t instruction = { 0 };
        
        instruction.address = assembled_code + (size_t)processor_state.ip;
        
        enum { MinOPCodeBitCount = 4 };
        enum { MaxOPCodeBitCount = 8 };
        
        const u8 opcode_byte = instruction.address[instruction.size++];
        
        bool op_code_found = false;
        for (u8 opcode_bit_count = MaxOPCodeBitCount; opcode_bit_count >= MinOPCodeBitCount; --opcode_bit_count)
        {
            const u8 opcode = bitfield_extract(opcode_byte, 8-opcode_bit_count, opcode_bit_count);
            
            instruction.op_type = get_op_type(opcode, 0xFF, opcode_bit_count);
            if (instruction.op_type == op_type_count)
                continue;
            
            op_code_found = true;
            
            switch (opcode)
            {
                case 0b100010:  // mov, Register/memory to/from register
                case 0b000000:  // add, Register/memory to/from register
                case 0b001010:  // sub, Register/memory to/from register
                case 0b001110:  // cmp, Register/memory to/from register
                {
                    instruction.w = bitfield_extract(opcode_byte, 0, 1);
                    const u8 d = bitfield_extract(opcode_byte, 1, 1);
                    
                    const u8 mod_reg_r_m_byte = instruction.address[instruction.size++];
                    const u8 mod = bitfield_extract(mod_reg_r_m_byte, 6, 2);
                    const u8 reg = bitfield_extract(mod_reg_r_m_byte, 3, 3);
                    const u8 r_m = bitfield_extract(mod_reg_r_m_byte, 0, 3);
                    
                    const bool is_wide = (instruction.w == 0x1);
                    
                    instruction.operands[1] = get_register_operand(reg, is_wide);
                    
                    if (mod == 0x3)
                    {
                        instruction.operands[0] = get_register_operand(r_m, is_wide);
                        switch (instruction.op_type)
                        {
                            case op_type_mov:
                            instruction.op_clocks = 2;
                            break;
                            
                            case op_type_add:
                            case op_type_sub:
                            case op_type_cmp:
                            instruction.op_clocks = 3;
                            break;
                            
                            default:
                            assert(false);
                        }
                    }
                    else
                    {
                        u16 disp_size = get_memory_operand(&instruction.operands[0], r_m, mod, instruction.address + instruction.size);
                        instruction.size += disp_size;
                        
                        switch (instruction.op_type)
                        {
                            case op_type_mov:
                            instruction.op_clocks = (d == 0x0) ? 9 : 8;
                            break;
                            
                            case op_type_add:
                            case op_type_sub:
                            instruction.op_clocks = (d == 0x0) ? 16 : 9;
                            break;
                            
                            case op_type_cmp:
                            instruction.op_clocks = 9;
                            break;
                            
                            default:
                            assert(false);
                        }
                        
                        instruction.ea_clocks = get_effective_address_clocks(instruction.operands[0].payload.mem.address_expression, disp_size != 0);
                    }
                    
                    if (d == 0x1)
                        core_swap(instruction.operands[0], instruction.operands[1], instruction_operand_t);
                } break;
                
                case 0b1100011: // mov, Immediate to register/memory
                case 0b100000:  // add or sub or cmp, Immediate to register/memory
                {
                    const u8 mod_extra_opcode_r_m_byte = instruction.address[instruction.size++];
                    const u8 extra_opcode = bitfield_extract(mod_extra_opcode_r_m_byte, 3, 3);
                    instruction.op_type = get_op_type(opcode, extra_opcode, opcode_bit_count);
                    
                    if (instruction.op_type == op_type_add_sub_cmp)
                    {
                        assert(!"THIS CANNOT HAPPEN");
                    }
                    
                    instruction.w = bitfield_extract(opcode_byte, 0, 1);
                    const bool is_wide = (instruction.w == 0x1);
                    
                    const u8 s = bitfield_extract(opcode_byte, 1, 1);
                    const bool should_sign_extend = (instruction.op_type == op_type_mov) ? false : (s == 0x1);
                    
                    const u8 mod = bitfield_extract(mod_extra_opcode_r_m_byte, 6, 2);
                    const u8 r_m = bitfield_extract(mod_extra_opcode_r_m_byte, 0, 3);
                    if (mod == 0x3)
                    {
                        assert(instruction.op_type != op_type_mov && "The mov instruction usually doesn't take this path. There is a separate opcode for this for mov.");
                        instruction.operands[0] = get_register_operand(r_m, is_wide);
                        instruction.op_clocks = 4;
                    }
                    else
                    {
                        u16 disp_size = get_memory_operand(&instruction.operands[0], r_m, mod, instruction.address + instruction.size);
                        instruction.size += disp_size;
                        
                        switch (instruction.op_type)
                        {
                            case op_type_mov:
                            case op_type_cmp:
                            instruction.op_clocks = 10;
                            break;
                            
                            case op_type_add:
                            case op_type_sub:
                            instruction.op_clocks = 17;
                            break;
                            
                            default:
                            assert(false);
                        }
                        
                        instruction.ea_clocks = get_effective_address_clocks(instruction.operands[0].payload.mem.address_expression, disp_size != 0);
                    }
                    
                    instruction.size += get_immediate_operand(&instruction.operands[1], should_sign_extend, is_wide, instruction.address+instruction.size);
                    
                } break;
                
                case 0b1011: // mov, Immediate to register
                {
                    instruction.w = bitfield_extract(opcode_byte, 3, 1);
                    const bool is_wide = (instruction.w == 0x1);
                    
                    instruction.size += get_immediate_operand(&instruction.operands[1], false, is_wide, instruction.address + instruction.size);
                    
                    const u8 reg = bitfield_extract(opcode_byte, 0, 3);
                    instruction.operands[0] = get_register_operand(reg, is_wide);
                    instruction.op_clocks = 4;
                } break;
                
                case 0b1010000: // mov, Memory to accumulator
                case 0b1010001: // mov, Accumulator to memory
                {
                    // NOTE(achal): Assume Memory to accumulator..
                    instruction.w = bitfield_extract(opcode_byte, 0, 1);
                    
                    instruction.operands[0] = get_register_operand(0x0, instruction.w == 0x1);
                    instruction.size += get_memory_operand(&instruction.operands[1], 0x6, 0x0, instruction.address + instruction.size);
                    
                    // NOTE(achal): Shockingly there are no clocks associated with EA calculation here..
                    instruction.op_clocks = 10;
                    
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
                    
                    instruction.op_clocks = 4;
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
                    assert(!show_clocks && !explain_clocks);
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
        
        // Print instructions and/or trace
        {
            assert(out_file);
            
            const processor_state_t prev_processor_state = processor_state;
            
            print_instruction(out_file, &instruction);
            processor_state.ip += instruction.size;
            total_clocks += instruction.op_clocks + instruction.ea_clocks;
            
            if (is_simulation_mode)
            {
                if (out_file_path)
                {
                    const char *extension = strrchr(out_file_path, '.');
                    
                    const char txt_extension_str[] = ".txt";
                    if (!extension || (strcmp(extension, txt_extension_str) != 0))
                        LOG_WARNING("Execution mode is enabled, output will be a text file but .txt extension not detected in the output path: %s", out_file_path);
                }
                
                const instruction_operand_t *src_op = &instruction.operands[1];
                const instruction_operand_t *dst_op = &instruction.operands[0];
                if (src_op->type == instruction_operand_type_count)
                {
                    src_op = dst_op;
                    dst_op = NULL;
                }
                
                u16 src = 0;
                {
                    assert(src_op);
                    
                    switch (src_op->type)
                    {
                        case instruction_operand_type_register:
                        {
                            const register_operand_t *op = &src_op->payload.reg;
                            
                            const u32 idx = (u32)op->name;
                            src = processor_state.registers[idx];
                            if (op->count == 1)
                            {
                                assert(!instruction.w);
                                
                                src >>= (8*op->offset);
                                src &= 0xFF;
                            }
                        } break;
                        
                        case instruction_operand_type_memory:
                        {
                            const memory_operand_t *op = &src_op->payload.mem;
                            
                            const u32 addr = get_effective_address(op, &processor_state);
                            assert(dst_op && (dst_op->type == instruction_operand_type_register));
                            
                            // TODO(achal): You can most likely just cast and deref here
                            const u16 byte1 = processor_state.memory[addr];
                            u16 byte2 = 0;
                            if (instruction.w)
                                byte2 = processor_state.memory[addr+1];
                            
                            src = (byte2 << 8) | byte1;
                        } break;
                        
                        case instruction_operand_type_immediate:
                        {
                            src = (u16)src_op->payload.imm.value;
                        } break;
                        
                        case instruction_operand_type_relative_jump_immediate:
                        {
                            src = (u16)src_op->payload.rel_jump_imm.value;
                        } break;
                        
                        default:
                        assert(false);
                    }
                }
                
                const u32 dst_size = instruction.w ? 2 : 1;
                u8 *dst = NULL;
                
                if (dst_op)
                {
                    switch (dst_op->type)
                    {
                        case instruction_operand_type_register:
                        {
                            const register_operand_t *reg_op = &dst_op->payload.reg;
                            
                            const u32 idx = (u32)reg_op->name;
                            assert(idx < register_name_count);
                            
                            assert(reg_op->offset <= 1);
                            dst = (u8 *)(processor_state.registers + idx) + reg_op->offset;
                        } break;
                        
                        case instruction_operand_type_memory:
                        {
                            const memory_operand_t *mem_op = &dst_op->payload.mem;
                            const u32 addr = get_effective_address(mem_op, &processor_state);
                            dst = processor_state.memory + addr;
                        } break;
                        
                        default:
                        assert(false);
                    }
                }
                
                switch (instruction.op_type)
                {
                    case op_type_mov:
                    {
                        if (dst_size == 2)
                            *((u16 *)dst) = src;
                        else if (dst_size == 1)
                            *dst = (u8)src;
                        else
                            assert(false);
                    } break;
                    
                    case op_type_add:
                    {
                        if (dst_size == 2)
                        {
                            u16 *dst_16 = (u16 *)dst;
                            *dst_16 += src;
                            processor_state.flags = get_processor_flags(*dst_16);
                        }
                        else if (dst_size == 1)
                        {
                            *dst += (u8)src;
                            processor_state.flags = get_processor_flags(*dst);
                        }
                        else
                        {
                            assert(false);
                        }
                    } break;
                    
                    case op_type_sub:
                    {
                        if (dst_size == 2)
                        {
                            u16 *dst_16 = (u16 *)dst;
                            *dst_16 -= src;
                            processor_state.flags = get_processor_flags(*dst_16);
                        }
                        else if (dst_size == 1)
                        {
                            *dst -= (u8)src;
                            processor_state.flags = get_processor_flags(*dst);
                        }
                        else
                        {
                            assert(false);
                        }
                    } break;
                    
                    case op_type_cmp:
                    {
                        u16 temp = UINT16_MAX;
                        if (dst_size == 2)
                        {
                            u16 *dst_16 = (u16 *)dst;
                            temp = *dst_16 - src;
                        }
                        else if (dst_size == 1)
                        {
                            temp = *dst - (u8)src;
                        }
                        else
                        {
                            assert(false);
                        }
                        processor_state.flags = get_processor_flags(temp);
                    } break;
                    
                    case op_type_jnz:
                    {
                        const u32 Bit_ZF = 6;
                        if ((processor_state.flags & (0x1 << Bit_ZF)) == 0)
                        {
                            assert(processor_state.ip != 0);
                            processor_state.ip += src;
                        }
                    } break;
                    
                    default:
                    assert(false);
                    break;
                }
                
                //-print trace
                file_print(out_file, " ;");
                {
                    //-clocks
                    if (show_clocks || explain_clocks)
                    {
                        u32 current_instruction_clocks = instruction.op_clocks + instruction.ea_clocks;
                        file_print(out_file, " Clocks: +%u = %u", current_instruction_clocks, total_clocks);
                        if (explain_clocks && (instruction.ea_clocks != 0))
                        {
                            file_print(out_file, " (%u + %uea)", instruction.op_clocks, instruction.ea_clocks);
                        }
                        file_print(out_file, " |");
                    }
                    
                    //-register and memory (in the future) state
                    if (dst_op)
                    {
                        switch (dst_op->type)
                        {
                            case instruction_operand_type_register:
                            {
                                const register_operand_t *reg_op = &dst_op->payload.reg;
                                const u32 idx = (u32)reg_op->name;
                                
                                if (prev_processor_state.registers[idx] != processor_state.registers[idx])
                                {
                                    const char *name = get_register_name(&dst_op->payload.reg);
                                    file_print(out_file, " %s:0x%X->0x%X", name, prev_processor_state.registers[idx], processor_state.registers[idx]);
                                }
                            } break;
                            
                            /*case instruction_operand_type_memory:
                            {
                                // This will make sense when I have GUI, so it becomes easier to print out memory.
                            } break;*/
                        }
                    }
                    
                    //-ip
                    if (prev_processor_state.ip != processor_state.ip)
                    {
                        file_print(out_file, " ip:0x%X->0x%X", prev_processor_state.ip, processor_state.ip);
                    }
                    
                    //-flags
                    if (prev_processor_state.flags != processor_state.flags)
                    {
                        file_print(out_file, " flags:");
                        print_processor_flags(out_file, prev_processor_state.flags);
                        file_print(out_file, "->");
                        print_processor_flags(out_file, processor_state.flags);
                    }
                }
            }
            
            file_print(out_file, "\n");
        }
    }
    
    if (is_simulation_mode)
    {
        file_print(out_file, "\nFinal registers:\n");
        for (u32 i = 0; i < register_name_count; ++i)
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
    
    if (should_dump_memory)
    {
        if (!is_simulation_mode)
            LOG_WARNING("User has asked to dump memory but no simulation was performed so memory will be garbage");
        
        const char path[] = "memory_dump.data";
        FILE *file = fopen(path, "wb");
        if (!file)
        {
            LOG_ERROR("Could not open file for writing: %s", path);
            return -1;
        }
        
        const size_t bytes_written = fwrite(processor_state.memory, 1, ProcessorMemorySize, file);
        if (bytes_written < ProcessorMemorySize)
            LOG_WARNING("Only %llu (out of %u) bytes were written successfully", bytes_written, ProcessorMemorySize);
        
        fclose(file);
    }
    
    return 0;
}