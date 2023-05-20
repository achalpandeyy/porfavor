#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cassert>

#define my_internal static

#define array_count(array) (sizeof(array) / sizeof(array[0]))

my_internal constexpr inline uint8_t bitfield_extract(const uint8_t value, const uint8_t offset, const uint8_t count)
{
    const uint8_t mask = (1 << count) - 1;
    const uint8_t result = (value >> offset) & mask;
    return result;
}

my_internal constexpr inline int16_t sign_extend_8_to_16(const uint8_t value)
{
    const int16_t result = (value & 0x80) ? (0xFF00 | value) : value;
    return result;
}

int main(int argc, char** argv)
{
    const char* in_file_path;
    if (argc == 1)
    {
        std::cout << "No input file provided. Using the default one." << std::endl;
        in_file_path = "tests/listing_0037_single_register_mov";
    }
    else
    {
        in_file_path = argv[1];
    }

    std::cout << "[INFO]: Input file: " << in_file_path << std::endl;

    std::vector<uint8_t> assembled_code;
    {
        std::ifstream read_file(in_file_path, std::ios::binary | std::ios::ate | std::ios::in);
        if (read_file.is_open())
        {
            const auto file_size = read_file.tellg();
            if (file_size != std::ios::pos_type(-1))
            {
                std::cout << "[INFO]: File size: " << file_size << " bytes." << std::endl;

                assembled_code.resize(file_size);

                read_file.seekg(0, std::ios::beg);
                read_file.read(reinterpret_cast<char*>(assembled_code.data()), file_size);
                if (read_file.fail())
                    std::cout << "[ERROR]: Could not read file " << in_file_path << std::endl;
            }
            else
            {
                std::cout << "[ERROR]: Could not get file size." << std::endl;
            }
            
            read_file.close();
        }
        else
        {
            std::cout << "[ERROR]: Could not open file " << in_file_path << std::endl;
            return -1;
        }
    }

    uint8_t* instruction_ptr = assembled_code.data();
    std::stringstream output_stream("bits 16\n\n");

    while (instruction_ptr != assembled_code.data() + assembled_code.size())
    {
        // TODO: Remove.
        auto get_register_name = [](const uint8_t predicate, const uint8_t W) -> std::string
        {
            const bool is_not_wide = (W == 0b0);
            switch (predicate)
            {
                case 0b000:
                    return is_not_wide ? "al" : "ax";
                case 0b001: 
                    return is_not_wide ? "cl" : "cx";
                case 0b010: 
                    return is_not_wide ? "dl" : "dx";
                case 0b011:
                    return is_not_wide ? "bl" : "bx";
                case 0b100:
                    return is_not_wide ? "ah" : "sp";
                case 0b101: 
                    return is_not_wide ? "ch" : "bp";
                case 0b110:
                    return is_not_wide ? "dh" : "si";
                case 0b111:
                    return is_not_wide ? "bh" : "di";
                default:
                    assert(!"Invalid code path.");
                    return "";
            }
        };

        enum Register_Name : uint8_t
        {
            Register_Name_a = 0,
            Register_Name_c,
            Register_Name_d,
            Register_Name_b,
            Register_Name_sp,
            Register_Name_bp,
            Register_Name_si,
            Register_Name_di,

            Register_Name_Count
        };

        struct Register_Operand
        {
            Register_Name name;
            uint8_t offset;
            uint8_t count;
        };

        enum Memory_Address_Expression : uint8_t
        {
            Memory_Address_Expression_bx_plus_si = 0,
            Memory_Address_Expression_bx_plus_di,
            Memory_Address_Expression_bp_plus_si,
            Memory_Address_Expression_bp_plus_di,
            Memory_Address_Expression_si,
            Memory_Address_Expression_di,
            Memory_Address_Expression_direct_address,
            Memory_Address_Expression_bx,

            Memory_Address_Expression_Count
        };

        struct Memory_Operand
        {
            Memory_Address_Expression address_expression;
            int32_t displacement;                       
        };

        struct Immediate_Operand
        {
            int32_t value;
        };

        struct Relative_Jump_Immediate_Operand
        {
            int32_t value;
        };

        enum Instruction_Operand_Type : uint8_t
        {
            Instruction_Operand_Type_Register = 0,
            Instruction_Operand_Type_Memory,
            Instruction_Operand_Type_Immediate,
            Instruction_Operand_Type_Relative_Jump_Immediate,

            Instruction_Operand_Type_Count
        };

        struct Instruction_Operand
        {
            Instruction_Operand_Type type = Instruction_Operand_Type_Count;
            union
            {
                Register_Operand reg;
                Memory_Operand mem;
                Immediate_Operand imm;
                Relative_Jump_Immediate_Operand rel_jump_imm;
            };
        };

        auto get_register_operand = [](const uint8_t index, const bool is_wide) -> Instruction_Operand
        {
            Instruction_Operand result;
            result.type = Instruction_Operand_Type_Register;
            result.reg.count = is_wide ? 2 : 1;

            bool is_register_name_set = false;
            auto set_register_name = [&is_register_name_set, &result](const Register_Name name)
            {
                if (!is_register_name_set)
                {
                    result.reg.name = name;
                    is_register_name_set = true;
                }
            };

            switch (index)
            {
                case 0b000:
                    set_register_name(Register_Name_a);
                case 0b001:
                    set_register_name(Register_Name_c);
                case 0b010:
                    set_register_name(Register_Name_d);
                case 0b011:
                    set_register_name(Register_Name_b);
                    result.reg.offset = 0;
                    break;

                case 0b100:
                    set_register_name(is_wide ? Register_Name_sp : Register_Name_a);
                case 0b101:
                    set_register_name(is_wide ? Register_Name_bp : Register_Name_c);
                case 0b110:
                    set_register_name(is_wide ? Register_Name_si : Register_Name_d);
                case 0b111:
                    set_register_name(is_wide ? Register_Name_di : Register_Name_b);
                    result.reg.offset = is_wide ? 0 : 1;
                    break;
                
                default:
                    assert(!"Invalid code path.");
                    break;
            }

            return result;
        };
        
        auto get_memory_operand = [](const uint8_t index, const uint8_t mod, const uint8_t* displacement, uint8_t& displacement_size) -> Instruction_Operand
        {
            Instruction_Operand result;
            result.type = Instruction_Operand_Type_Memory;

            switch (index)
            {
                case 0b000:
                    result.mem.address_expression = Memory_Address_Expression_bx_plus_si;
                    break;
                case 0b001:
                    result.mem.address_expression = Memory_Address_Expression_bx_plus_di;
                    break;
                case 0b010:
                    result.mem.address_expression = Memory_Address_Expression_bp_plus_si;
                    break;
                case 0b011: 
                    result.mem.address_expression = Memory_Address_Expression_bp_plus_di;
                    break;
                case 0b100:
                    result.mem.address_expression = Memory_Address_Expression_si;
                    break;
                case 0b101:               
                    result.mem.address_expression = Memory_Address_Expression_di;
                    break;
                case 0b110:
                    result.mem.address_expression = Memory_Address_Expression_direct_address;
                    break;
                case 0b111:
                    result.mem.address_expression = Memory_Address_Expression_bx;
                    break;
                default:
                    assert(!"Invalid code path.");
            }

            if ((mod == 0b10) || (mod == 0b00 && index == 0b110) /*special case (direct address)*/)
            {
                displacement_size = 2;
                result.mem.displacement = static_cast<int32_t>(*reinterpret_cast<const uint16_t*>(displacement));
            }
            else
            {
                displacement_size = 1;
                result.mem.displacement = static_cast<int32_t>(sign_extend_8_to_16(*displacement));
            }

            return result;
        };

        // TODO: Remove.
        auto get_memory_address_expression = [&instruction_ptr](const uint8_t MOD, const uint8_t R_M) -> std::string
        {
            if (MOD == 0b11) // not a memory mode
            {
                assert(!"MOD cannot be 11. This has to be a memory mode.");
                return "";
            }

            auto get_register_expression = [](const uint8_t predicate) -> std::string
            {
                switch (predicate)
                {
                    case 0b000:
                        return "bx + si";
                    case 0b001:
                        return "bx + di";
                    case 0b010: 
                        return "bp + si";
                    case 0b011:
                        return "bp + di";
                    case 0b100:
                        return "si";
                    case 0b101:
                        return "di";
                    case 0b110:
                        return "bp"; // this case shouldn't run for MOD = 00
                    case 0b111:
                        return "bx";
                    default:
                        assert(!"Invalid predicate");
                        return "";
                }
            };

            std::string result;
            switch (MOD)
            {
                case 0b00: // no displacement, except the direct address special case
                {
                    if (R_M == 0b110) // special case
                    {
                        const uint16_t lo = static_cast<uint16_t>(*instruction_ptr++);
                        const uint16_t hi = static_cast<uint16_t>(*instruction_ptr++);
                        const uint16_t direct_address = (hi << 8) | lo;
                        result = std::to_string(direct_address);
                    }
                    else
                    {
                        result = get_register_expression(R_M);
                    }
                } break;

                case 0b01: // 8-bit displacement
                {
                    result = get_register_expression(R_M);

                    const int8_t displacement = static_cast<int8_t>(*instruction_ptr++);

                    // NOTE: If MOD is 01 we always consider the displacement to be signed, according to the instruction manual, of course.
                    if (displacement >= 0)
                        result += " + " + std::to_string(displacement);
                    else
                        result += " - " + std::to_string(-displacement);
                } break;

                case 0b10: // 16-bit displacement
                {
                    const uint16_t lo = static_cast<uint16_t>(*instruction_ptr++);
                    const uint16_t hi = static_cast<uint16_t>(*instruction_ptr++);
                    const uint16_t displacement = (hi << 8) | lo;
                
                    result = get_register_expression(R_M) + " + " + std::to_string(displacement);
                } break;

                default:
                    assert(!"Invalid code path.");
                    return "";
            }

            return "[" + result + "]";
        };

        auto get_immediate_operand = [](const bool should_sign_extend, const bool is_wide, const uint8_t* immediate_data, uint8_t& immediate_size) -> Instruction_Operand
        {
            Instruction_Operand result;
            result.type = Instruction_Operand_Type_Immediate;

            if (!should_sign_extend && !is_wide)
            {
                immediate_size = 1;
                result.imm.value = static_cast<int32_t>(*immediate_data);
            }
            else if (!should_sign_extend && is_wide)
            {
                immediate_size = 2;
                result.imm.value = static_cast<int32_t>(*reinterpret_cast<const uint16_t*>(immediate_data));
            }
            else if (should_sign_extend && is_wide)
            {
                immediate_size = 1;
                result.imm.value = static_cast<int32_t>(sign_extend_8_to_16(*immediate_data));
            }
            else
            {
                // NOTE: This case shouldn't be possible.
                assert(!"Invalid code path.");
            }

            return result;
        };

        // TODO: Remove.
        auto get_immediate_expression = [&instruction_ptr](const uint8_t S, const uint8_t W) -> std::string
        {
            if (S == 0b0 && W == 0b0)
            {
                const uint8_t immediate_value = *instruction_ptr++;
                return std::to_string(immediate_value);
            }
            else if (S == 0b0 && W == 0b1)
            {
                const uint16_t lo = static_cast<uint16_t>(*instruction_ptr++);
                const uint16_t hi = static_cast<uint16_t>(*instruction_ptr++);
                const uint16_t immediate_value = (hi << 8) | lo;
                return std::to_string(immediate_value);
            }
            else if (S == 0b1 && W == 0b1)
            {
                const int8_t immediate_value = static_cast<int8_t>(*instruction_ptr++);

                // Sign-extension
                if (immediate_value >= 0)
                    return std::to_string(immediate_value);
                else
                    return "-" + std::to_string(-immediate_value);
            }
            else
            {
                assert(!"Invalid code path.");
                return "";
            }
        };

        auto get_relative_jump_immediate_operand = [](const uint8_t* data, uint8_t& data_size) -> Instruction_Operand
        {
            Instruction_Operand result;
            result.type = Instruction_Operand_Type_Relative_Jump_Immediate;

            data_size = 1;
            // NOTE: NASM will add a -2 by itself to the displacement so add 2 to counter.
            result.rel_jump_imm.value = static_cast<int32_t>(sign_extend_8_to_16(*data)) + 2;
            return result;
        };

        // TODO: Remove.
        auto get_jump_displacement_expression = [&instruction_ptr]() -> std::string
        {
            // NOTE: NASM will add a -2 by itself to the displacement so add 2 to counter.
            const int8_t displacement = static_cast<int8_t>(*instruction_ptr++) + 2;
            if (displacement >= 0)
                return "$+" + std::to_string(displacement);
            else
                return "$" + std::to_string(displacement);
        };

        enum Op_Type : uint8_t
        {
            Op_Type_mov = 0,
            Op_Type_add,
            Op_Type_sub,
            Op_Type_cmp,
            Op_Type_jz,     // je
            Op_Type_jl,     // jnge
            Op_Type_jle,    // jng
            Op_Type_jb,     // jnae
            Op_Type_jbe,    // jna
            Op_Type_jp,     // jpe
            Op_Type_jo,
            Op_Type_js,
            Op_Type_jnz,    // jne
            Op_Type_jnl,    // jge
            Op_Type_jnle,   // jg
            Op_Type_jnb,    // jae
            Op_Type_jnbe,   // ja
            Op_Type_jnp,    // jpo
            Op_Type_jno,
            Op_Type_jns,
            Op_Type_loop,
            Op_Type_loopz,  // loope
            Op_Type_loopnz, // loopne
            Op_Type_jcxz,

            Op_Type_Count
        };

        const char* op_mnemonic_table[] =
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
        static_assert(array_count(op_mnemonic_table) == Op_Type_Count, "op_mnemonic_table size doesn't match Op_Type_Count.");

        struct Instruction
        {
            Op_Type op_type = Op_Type_Count;
            uint8_t w       = UINT8_MAX;
            uint8_t size    = 0;

            // NOTE: operands[0] is the the one which appears first in
            // the disassembly instruction, this is usually the destination operand.
            // For instructions which have only one operand, operand[0] will be used.
            Instruction_Operand operands[2];
        };

        Instruction decoded_instruction;

        constexpr uint8_t MinOPCodeBitCount = 4;
        constexpr uint8_t MaxOPCodeBitCount = 8;

        const uint8_t opcode_byte = instruction_ptr[decoded_instruction.size++];

        bool op_code_found = true;
        for (uint8_t opcode_bit_count = MaxOPCodeBitCount; opcode_bit_count >= MinOPCodeBitCount; --opcode_bit_count)
        {
            const uint8_t opcode = (opcode_byte >> (8 - opcode_bit_count)) & ((1 << opcode_bit_count) - 1);

            bool is_op_type_set = false;
            auto set_op_type = [&is_op_type_set, &decoded_instruction](const Op_Type op_type)
            {
                if (!is_op_type_set)
                {
                    decoded_instruction.op_type = op_type;
                    is_op_type_set = true;
                }
            };

            switch (opcode)
            {
                case 0b100010: // mov, Register/memory to/from register
                    set_op_type(Op_Type_mov);
                case 0b000000: // add, Register/memory to/from register
                    set_op_type(Op_Type_add);
                case 0b001010: // sub, Register/memory to/from register
                    set_op_type(Op_Type_sub);
                case 0b001110: // cmp, Register/memory to/from register
                {
                    set_op_type(Op_Type_cmp);

                    decoded_instruction.w = bitfield_extract(opcode_byte, 0, 1);
                    const uint8_t d = bitfield_extract(opcode_byte, 1, 1);

                    const uint8_t mod_reg_r_m_byte = instruction_ptr[decoded_instruction.size++];
                    const uint8_t mod = bitfield_extract(mod_reg_r_m_byte, 6, 2);
                    const uint8_t reg = bitfield_extract(mod_reg_r_m_byte, 3, 3);
                    const uint8_t r_m = bitfield_extract(mod_reg_r_m_byte, 0, 3);

                    decoded_instruction.operands[1] = get_register_operand(reg, decoded_instruction.w == 0b1);
                    Instruction_Operand& dst = decoded_instruction.operands[0];

                    if (mod == 0b11)
                    {
                        dst = get_register_operand(r_m, decoded_instruction.w == 0b1);
                    }
                    else
                    {
                        const uint8_t* displacement = instruction_ptr + decoded_instruction.size;
                        uint8_t displacement_size;
                        dst = get_memory_operand(r_m, mod, displacement, displacement_size);
                        decoded_instruction.size += displacement_size;
                    }

                    if (d == 0b1)
                        std::swap(decoded_instruction.operands[1], dst);
                } break;

                case 0b1100011: // mov, Immediate to register/memory
                    set_op_type(Op_Type_mov);
                case 0b100000:  // add or sub or cmp, Immediate to register/memory
                {
                    const uint8_t mod_extra_opcode_r_m_byte = instruction_ptr[decoded_instruction.size++];

                    const uint8_t extra_opcode = bitfield_extract(mod_extra_opcode_r_m_byte, 3, 3);
                    switch (extra_opcode)
                    {
                        case 0b000: // add
                            set_op_type(Op_Type_add);
                            break;
                        case 0b101: // sub
                            set_op_type(Op_Type_sub);
                            break;
                        case 0b111: // cmp
                            set_op_type(Op_Type_cmp);
                            break;
                        default:
                            assert(!"Invalid code path.");
                            break;
                    }

                    const uint8_t s = (decoded_instruction.op_type != Op_Type_mov) ? bitfield_extract(opcode_byte, 1, 1) : 0b0;
                    decoded_instruction.w = bitfield_extract(opcode_byte, 0, 1);

                    const uint8_t* immediate_data = instruction_ptr + decoded_instruction.size;
                    uint8_t immediate_size;
                    decoded_instruction.operands[1] = get_immediate_operand(s == 0b1, decoded_instruction.w == 0b1, immediate_data, immediate_size);
                    decoded_instruction.size += immediate_size;

                    Instruction_Operand& dst = decoded_instruction.operands[0];

                    const uint8_t mod = bitfield_extract(mod_extra_opcode_r_m_byte, 6, 2);
                    const uint8_t r_m = bitfield_extract(mod_extra_opcode_r_m_byte, 0, 3);
                    if (mod == 0b11)
                    {
                        assert(decoded_instruction.op_type != Op_Type_mov && "The mov instruction usually doesn't take this path. There is a separate opcode for this for mov.");
                        dst = get_register_operand(r_m, decoded_instruction.w == 0b1);
                    }
                    else
                    {
                        const uint8_t* displacement = instruction_ptr + decoded_instruction.size;
                        uint8_t displacement_size;
                        dst = get_memory_operand(r_m, mod, displacement, displacement_size);
                        decoded_instruction.size += displacement_size;
                    }
                } break;

                case 0b1011: // mov, Immediate to register
                {
                    set_op_type(Op_Type_mov);

                    decoded_instruction.w = bitfield_extract(opcode_byte, 3, 1);

                    const uint8_t* immediate_data = instruction_ptr + decoded_instruction.size;
                    uint8_t immediate_size;
                    decoded_instruction.operands[1] = get_immediate_operand(false, decoded_instruction.w == 0b1, immediate_data, immediate_size);
                    decoded_instruction.size += immediate_size;

                    const uint8_t reg = bitfield_extract(opcode_byte, 0, 3);
                    decoded_instruction.operands[0] = get_register_operand(reg, decoded_instruction.w == 0b1);
                } break;

                case 0b1010000: // mov, Memory to accumulator
                case 0b1010001: // mov, Accumulator to memory
                {
                    set_op_type(Op_Type_mov);

                    // NOTE: Assume Memory to accumulator..
                    decoded_instruction.w = bitfield_extract(opcode_byte, 0, 1);

                    decoded_instruction.operands[1] = get_register_operand(0b000, decoded_instruction.w == 0b1);

                    uint8_t direct_address_size;
                    decoded_instruction.operands[0] = get_memory_operand(0b110, 0b00, instruction_ptr + decoded_instruction.size, direct_address_size);
                    assert(direct_address_size == 2 && "This should always be 16 bits.");
                    decoded_instruction.size += direct_address_size;

                    // NOTE: Swap if it is Accumulator to memory
                    if (opcode == 0b1010001)
                        std::swap(decoded_instruction.operands[0], decoded_instruction.operands[1]);
                } break;

                case 0b0000010: // add, Immediate to accumulator
                    set_op_type(Op_Type_add);
                case 0b0010110: // sub, Immediate to accumulator
                    set_op_type(Op_Type_sub);
                case 0b0011110: // cmp, Immediate to accumulator
                {
                    set_op_type(Op_Type_cmp);

                    decoded_instruction.w = bitfield_extract(opcode_byte, 0, 1);

                    const uint8_t* immediate_data = instruction_ptr + decoded_instruction.size;
                    uint8_t immediate_size;
                    decoded_instruction.operands[1] = get_immediate_operand(false, decoded_instruction.w == 0b1, immediate_data, immediate_size);
                    decoded_instruction.size += immediate_size;

                    decoded_instruction.operands[0] = get_register_operand(0b000, decoded_instruction.w == 0b1);
                } break;

                case 0b01110100: // jz/je
                    set_op_type(Op_Type_jz);
                case 0b01111100: // jl/jnge
                    set_op_type(Op_Type_jl);
                case 0b01111110: // jle/jng
                    set_op_type(Op_Type_jle);
                case 0b01110010: // jb/jnae
                    set_op_type(Op_Type_jb);
                case 0b01110110: // jbe/jna
                    set_op_type(Op_Type_jbe);
                case 0b01111010: // jp/jpe
                    set_op_type(Op_Type_jp);
                case 0b01110000: // jo
                    set_op_type(Op_Type_jo);
                case 0b01111000: // js
                    set_op_type(Op_Type_js);
                case 0b01110101: // jnz/jne
                    set_op_type(Op_Type_jnz);
                case 0b01111101: // jnl/jge
                    set_op_type(Op_Type_jnl);
                case 0b01111111: // jnle/jg
                    set_op_type(Op_Type_jnle);
                case 0b01110011: // jnb/jae
                    set_op_type(Op_Type_jnb);
                case 0b01110111: // jnbe/ja
                    set_op_type(Op_Type_jnbe);
                case 0b01111011: // jnp/jpo
                    set_op_type(Op_Type_jnp);
                case 0b01110001: // jno
                    set_op_type(Op_Type_jno);
                case 0b01111001: // jns
                    set_op_type(Op_Type_jns);
                case 0b11100010: // loop
                    set_op_type(Op_Type_loop);
                case 0b11100001: // loopz/loope
                    set_op_type(Op_Type_loopz);
                case 0b11100000: // loopnz/loopne
                    set_op_type(Op_Type_loopnz);
                case 0b11100011: // jcxz
                {
                    set_op_type(Op_Type_jcxz);

                    const uint8_t* data = instruction_ptr + decoded_instruction.size;
                    uint8_t data_size;
                    decoded_instruction.operands[0] = get_relative_jump_immediate_operand(data, data_size);
                    decoded_instruction.size += data_size;
                } break;

                default:
                    op_code_found = false;
                    continue;
            }
            
            op_code_found = true;
            break;
        }

        if (!op_code_found)
        {
            std::cout << "[ERROR]: Unknown opcode." << std::endl;
            return -1;
        }
        assert(decoded_instruction.op_type != Op_Type_Count && "The op_type should have been set by now.");

        output_stream << op_mnemonic_table[decoded_instruction.op_type] << " ";

        const bool should_print_size_expression =
            ((decoded_instruction.operands[0].type == Instruction_Operand_Type_Memory) && (decoded_instruction.operands[1].type == Instruction_Operand_Type_Immediate)) ||
            ((decoded_instruction.operands[0].type == Instruction_Operand_Type_Immediate) && (decoded_instruction.operands[1].type == Instruction_Operand_Type_Memory));

        const char* size_expression = "";
        if (should_print_size_expression)
            size_expression = (decoded_instruction.w == 0b0) ? "byte" : "word";

        auto get_register_expression_string = [](const Register_Operand& reg) -> const char*
        {
            const char* table[Register_Name_Count][3] =
            {
                { "al", "ah", "ax" },
                { "cl", "ch", "cx" },
                { "dl", "dh", "dx" },
                { "bl", "bh", "bx" },
                { "sp", "sp", "sp" },
                { "bp", "bp", "bp" },
                { "si", "si", "si" },
                { "di", "di", "di" }
            };
            static_assert(array_count(table) == Register_Name_Count);

            const char* result = table[reg.name][(reg.count == 2) ? 2 : reg.offset & 1];
            return result;
        };

        auto get_memory_address_expression_string = [](char* out_string, const Memory_Operand& mem, const char* size_expression) -> size_t
        {
            const char* table[] =
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
            static_assert(array_count(table) == Memory_Address_Expression_Count);

            size_t len = 0;
            if (strcmp(size_expression, "") != 0)
            {
                const size_t size_expression_len = strlen(size_expression);
                strcpy_s(out_string + len, size_expression_len, size_expression);
                len += size_expression_len;
                out_string[len++] = ' ';
            }

            out_string[len++] = '[';

            const size_t address_expression_len = strlen(table[mem.address_expression]);
            strcpy_s(out_string + len, address_expression_len, table[mem.address_expression]);
            len += address_expression_len;

            // TODO: Set the default displacement to INT32_MAX so that it works.
            if (mem.displacement != INT32_MAX)
            {
                const char* disp = std::to_string(mem.displacement).c_str();

                if ((mem.displacement) >= 0 && (mem.address_expression != Memory_Address_Expression_direct_address))
                    out_string[len++] = '+';

                const size_t disp_len = strlen(disp);
                strcpy_s(out_string + len, disp_len, disp);
                len += disp_len;
            }

            out_string[len++] = ']';
            out_string[len] = '\0';

            return len;
        };

        switch (decoded_instruction.operands[0].type)
        {
            case Instruction_Operand_Type_Register:
            {
                const char* register_expression = get_register_expression_string(decoded_instruction.operands[0].reg);
                output_stream << register_expression;
            } break;

            case Instruction_Operand_Type_Memory:
            {
                char memory_address_expression[64];
                const size_t len_written = get_memory_address_expression_string(memory_address_expression, decoded_instruction.operands[0].mem, size_expression);
                assert(len_written < array_count(memory_address_expression));

                output_stream << memory_address_expression;
            } break;

            default:
                assert(!"Invalid code path.");
                break;
        }

        if (decoded_instruction.operands[1].type != Instruction_Operand_Type_Count)
        {
            output_stream << ", ";

            switch (decoded_instruction.operands[1].type)
            {
                case Instruction_Operand_Type_Register:
                {
                    const char* register_expression = get_register_expression_string(decoded_instruction.operands[1].reg);
                    output_stream << register_expression;
                } break;

                case Instruction_Operand_Type_Memory:
                {
                    char memory_address_expression[64];
                    const size_t len_written = get_memory_address_expression_string(memory_address_expression, decoded_instruction.operands[1].mem, size_expression);
                    assert(len_written < array_count(memory_address_expression));

                    output_stream << memory_address_expression;
                } break;

                // TODO: Immediate and relative jump immediate operands.
            }
        }

        output_stream << "\n";

        instruction_ptr += decoded_instruction.size;

#if 0
        const uint8_t opcode = (opcode_byte >> 2) & 0b111111;
        if ((opcode == 0b100010) || (opcode == 0b000000) || (opcode == 0b001010) || (opcode == 0b001110)) // Register/memory to/from register
        {
            // NOTE: This case should handle instructions of the form:
            // {mov/add/sub/cmp} al, bl
            // {mov/add/sub/cmp} [bx + di], cx
            // {mov/add/sub/cmp} cx, [bx + di]

            const uint8_t D     = (opcode_byte >> 1) & 0b1;
            const uint8_t W     = opcode_byte        & 0b1;

            const uint8_t mod_reg_r_m_byte = *instruction_ptr++;
            const uint8_t MOD   = (mod_reg_r_m_byte >> 6) & 0b11;
            const uint8_t REG   = (mod_reg_r_m_byte >> 3) & 0b111;
            const uint8_t R_M   = mod_reg_r_m_byte        & 0b111;

            std::string src = get_register_name(REG, W);
            std::string dst = (MOD == 0b11) ? get_register_name(R_M, W) : get_memory_address_expression(MOD, R_M);

            if (static_cast<uint8_t>(D) == 0b1)
                std::swap(src, dst);
            else if (static_cast<uint8_t>(D) != 0b0)
                assert(!"Invalid D value.");

            std::string instruction_mnemonic;
            switch (opcode)
            {
                case 0b100010:
                    instruction_mnemonic = "mov";
                    break;
                case 0b000000:
                    instruction_mnemonic = "add";
                    break;
                case 0b001010:
                    instruction_mnemonic = "sub";
                    break;
                case 0b001110:
                    instruction_mnemonic = "cmp";
                    break;
                default:
                    assert(!"Invalid code path.");
            }

            output_stream << instruction_mnemonic << " " << dst << ", " << src;
        }
        else if ((((opcode_byte >> 1) & 0b1111111) == 0b1100011)) // mov, Immediate to register/memory
        {
            const uint8_t W = opcode_byte & 0b1;
            const uint8_t mod_extra_opcode_r_m_byte = *instruction_ptr++;
            const uint8_t MOD = (mod_extra_opcode_r_m_byte >> 6) & 0b11;
            const uint8_t R_M = mod_extra_opcode_r_m_byte & 0b111;

            const std::string size_expression = (W == 0b0) ? "byte" : "word";

            // NOTE: This is weird because the instruction manual says that this category is "Immediate to register/memory" but none of my immediate to register
            // mov instruction have ever fallen into this category.
            assert((MOD != 0b11) && "Moving an immediate to register shouldn't fall in this category.");

            const std::string dst = get_memory_address_expression(MOD, R_M);
            output_stream << "mov" << " " << dst << ", " << size_expression << " " << get_immediate_expression(0b0, W);
        }
        else if (((opcode_byte >> 4) & 0b1111)== 0b1011) // mov, Immediate to register
        {
            const uint8_t W = static_cast<uint8_t>((opcode_byte >> 3) & 0b1);
            const uint8_t REG = static_cast<uint8_t>(opcode_byte & 0b111);

            const std::string register_name = get_register_name(REG, W);

            // NOTE: We don't want any sign-extension in a mov instruction because it just copies the bit pattern
            // doesn't matter if I'my copying the bit pattern of -34 or 65502 because they are exactly the same in 16 bits.
            output_stream << "mov " << register_name << ", " << get_immediate_expression(0b0, W);
        }
        else if (((opcode_byte >> 1) & 0b1111111) == 0b1010000) // mov, Memory to accumulator
        {
            const uint8_t W = opcode_byte & 0b1;
            const std::string register_name = (W == 0b0) ? "al" : "ax";

            // NOTE: This is a direct address.
            const uint16_t addr_lo = static_cast<uint16_t>(*instruction_ptr++);
            const uint16_t addr_hi = static_cast<uint16_t>(*instruction_ptr++);
            const uint16_t addr = (addr_hi << 8) | addr_lo;
            
            output_stream << "mov " << register_name << ", " << "[" << std::to_string(addr) << "]";
        }
        else if (((opcode_byte >> 1) & 0b1111111) == 0b1010001) // mov, Accumulator to memory
        {
            const uint8_t W = opcode_byte & 0b1;
            const std::string register_name = (W == 0b0) ? "al" : "ax";

            const uint16_t addr_lo = static_cast<uint16_t>(*instruction_ptr++);
            const uint16_t addr_hi = static_cast<uint16_t>(*instruction_ptr++);
            const uint16_t addr = (addr_hi << 8) | addr_lo;

            output_stream << "mov " << "[" << std::to_string(addr) << "]" << ", " << register_name;
        }
        else if (((opcode_byte >> 2) & 0b111111) == 0b100000) // add/sub/cmp, Immediate to register/memory
        {
            const uint8_t S = (opcode_byte >> 1) & 0b1;
            const uint8_t W = opcode_byte & 0b1;
            assert((W == 0b1) || ((W == 0b0) && (S != 0b1)) && "You cannot sign extend 8 bit to 16 bit if the instruction doesn't operate on 16 bits of data.");

            const uint8_t mod_extra_opcode_r_m_byte = *instruction_ptr++;
            const uint8_t MOD = (mod_extra_opcode_r_m_byte >> 6) & 0b11;

            const uint8_t extra_opcode = (mod_extra_opcode_r_m_byte >> 3) & 0b111;

            std::string instruction_mnemonic = "";
            switch (extra_opcode)
            {
                case 0b000:
                    instruction_mnemonic = "add";
                    break;

                case 0b101:
                    instruction_mnemonic = "sub";
                    break;

                case 0b111:
                    instruction_mnemonic = "cmp";
                    break;

                default:
                    assert(!"Invalid code path.");
            }
            
            const uint8_t R_M = mod_extra_opcode_r_m_byte & 0b111;

            if (MOD == 0b11)
            {
                const std::string dst = get_register_name(R_M, W);
                output_stream << instruction_mnemonic << " " << dst << ", " << get_immediate_expression(S, W);
            }
            else
            {
                const std::string size_expression = (W == 0b0) ? "byte" : "word";

                const std::string dst = get_memory_address_expression(MOD, R_M);
                output_stream << instruction_mnemonic << " " << dst << ", " << size_expression << " " << get_immediate_expression(S, W);
            }
        }
        
        else if (((opcode_byte >> 1) & 0b1111111) == 0b0000010) // add, Immediate to accumulator
        {
            const uint8_t W = (opcode_byte & 0b1);
            const std::string dst = get_register_name(0b000, W);
            output_stream << "add " << dst << ", " << get_immediate_expression(0b0, W);
        }
        else if (((opcode_byte >> 1) & 0b1111111) == 0b0010110) // sub, Immediate to accumulator
        {
            const uint8_t W = (opcode_byte & 0b1);
            const std::string dst = get_register_name(0b000, W);
            output_stream << "sub " << dst << ", " << get_immediate_expression(0b0, W);
        }
        else if (((opcode_byte >> 1) & 0b1111111) == 0b0011110) // cmp, Immediate to accumulator
        {
            const uint8_t W = (opcode_byte & 0b1);
            const std::string dst = get_register_name(0b000, W);
            output_stream << "cmp " << dst << ", " << get_immediate_expression(0b0, W);
        }
        else if (opcode_byte == 0b01110100 ) // jz/je
        {
            output_stream << "jz " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b01111100) // jl/jnge
        {
            output_stream << "jl " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b01111110) // jle/jng
        {
            output_stream << "jle " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b01110010) // jb/jnae
        {
            output_stream << "jb " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b01110110) // jbe/jna
        {
            output_stream << "jbe " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b01111010) // jp/jpe
        {
            output_stream << "jp " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b01110000) // jo
        {
            output_stream << "jo " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b01111000) // js
        {
            output_stream << "js " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b01110101) // jnz/jne
        {
            output_stream << "jnz " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b01111101) // jnl/jge
        {
            output_stream << "jnl " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b01111111) // jnle/jg
        {
            output_stream << "jnle " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b01110011) // jnb/jae
        {
            output_stream << "jnb " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b01110111) // jnbe/ja
        {
            output_stream << "jnbe " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b01111011) // jnp/jpo
        {
            output_stream << "jnp " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b01110001) // jno
        {
            output_stream << "jno " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b01111001) // jns
        {
            output_stream << "jns " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b11100010) // loop
        {
            output_stream << "loop " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b11100001) // loopz/loope
        {
            output_stream << "loopz " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b11100000) // loopnz/loopne
        {
            output_stream << "loopnz " << get_jump_displacement_expression();
        }
        else if (opcode_byte == 0b11100011) // jcxz
        {
            output_stream << "jcxz " << get_jump_displacement_expression();
        }
        else
        {
            std::cout << "[ERROR]: Unknown instruction. Can't derive the opcode." << std::endl;
            return -1;
        }

        // TODO: Use `decoded_instruction` to generate the text I want to print.

        output_stream << "\n";
#endif
    }

    const char* out_file_path;
    {
        std::stringstream path_stream;
        path_stream << "tests/";
        path_stream << "out_";
        const auto in_file_path_string = std::string(in_file_path);
        const auto slash_index = in_file_path_string.find_first_of("/\\");
        assert(slash_index != std::string::npos);

        const auto real_file_name = in_file_path_string.substr(slash_index + 1, std::string::npos);
        path_stream << real_file_name;
        path_stream << ".asm";

        out_file_path = path_stream.str().c_str();
    }

    std::ofstream out_file(out_file_path);
    if (out_file.is_open())
    {
        out_file << output_stream.str();
        out_file.close();
    }
    else
    {
        std::cout << "[ERROR]: Could not open file " << out_file_path << std::endl;
        return -1;
    }

    return 0;
}