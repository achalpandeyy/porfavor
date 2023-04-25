#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cassert>

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

    uint8_t* instruction_stream = assembled_code.data();
    std::stringstream output_stream;

    while (instruction_stream != assembled_code.data() + assembled_code.size())
    {
        output_stream << "mov ";

        auto get_register_name = [](const uint8_t predicate, const bool is_not_wide) -> std::string
        {
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

        auto get_memory_address_expression = [&instruction_stream](const uint8_t MOD, const uint8_t R_M) -> std::string
        {
            if (MOD == 0b11) // not a memory mode
                return "";

            auto get_register_name_expression = [](const uint8_t predicate) -> std::string
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
                        const uint16_t lo = static_cast<uint16_t>(*instruction_stream++);
                        const uint16_t hi = static_cast<uint16_t>(*instruction_stream++);
                        const uint16_t direct_address = (hi << 8) | lo;
                        result = std::to_string(direct_address);
                    }
                    else
                    {
                        result = get_register_name_expression(R_M);
                    }
                } break;

                case 0b01: // 8-bit displacement
                {
                    result = get_register_name_expression(R_M);

                    const int8_t displacement = static_cast<int8_t>(*instruction_stream++);

                    // NOTE: If MOD is 01 we always consider the displacement to be signed, according to the instruction manual, of course.
                    if (displacement >= 0)
                        result += " + " + std::to_string(displacement);
                    else
                        result += " - " + std::to_string(-displacement);
                } break;

                case 0b10: // 16-bit displacement
                {
                    const uint16_t lo = static_cast<uint16_t>(*instruction_stream++);
                    const uint16_t hi = static_cast<uint16_t>(*instruction_stream++);
                    const uint16_t displacement = (hi << 8) | lo;
                
                    result = get_register_name_expression(R_M) + " + " + std::to_string(displacement);
                } break;

                default:
                    assert(!"Invalid code path.");
                    return "";
            }

            return "[" + result + "]";
        };

        auto get_immediate_value = [&instruction_stream](const uint8_t W) -> uint16_t
        {
            switch (W)
            {
                case 0b0:
                {
                    const uint8_t immediate_value = *instruction_stream++;
                    return immediate_value;
                };

                case 0b1:
                {
                    const uint16_t lo = static_cast<uint16_t>(*instruction_stream++);
                    const uint16_t hi = static_cast<uint16_t>(*instruction_stream++);
                    const uint16_t immediate_value = (hi << 8) | lo;
                    return immediate_value;
                }

                default:
                    assert(!"Invalid code path.");
                    return UINT16_MAX;
            }
        };

        const uint8_t opcode_byte = *instruction_stream++;
        if (((opcode_byte >> 2) & 0b111111) == 0b100010) // Register/memory to/from register
        {
            // NOTE: This case should handle instructions of the form:
            // 1. Register to register move:    mov al, bl
            // 2. Register to memory move:      mov [bx + di], cx
            // 3. Memory to register move:      mov cx, [bx + di]

            const uint8_t D     = (opcode_byte >> 1) & 0b1;
            const uint8_t W     = opcode_byte        & 0b1;

            const uint8_t mod_reg_r_m_byte = *instruction_stream++;
            const uint8_t MOD   = (mod_reg_r_m_byte >> 6) & 0b11;
            const uint8_t REG   = (mod_reg_r_m_byte >> 3) & 0b111;
            const uint8_t R_M   = mod_reg_r_m_byte        & 0b111;

            std::string src = get_register_name(REG, W == 0b0);
            std::string dst = (MOD == 0b11) ? get_register_name(R_M, W == 0b0) : get_memory_address_expression(MOD, R_M);

            if (static_cast<uint8_t>(D) == 0b1)
                std::swap(src, dst);
            else if (static_cast<uint8_t>(D) != 0b0)
                assert(!"Invalid D value.");

            output_stream << dst << ", " << src;
        }
        else if (((opcode_byte >> 1) & 0b1111111) == 0b1100011) // Immediate to register/memory
        {
            // NOTE: This case should handle instructions of the form:
            // mov [reg + displacement], size immediate

            const uint8_t W = opcode_byte & 0b1;
            const std::string size_expression = (W == 0b0) ? "byte" : "word";

            const uint8_t mod_reg_r_m_byte = *instruction_stream++;
            const uint8_t MOD = (mod_reg_r_m_byte >> 6) & 0b11;
            assert(MOD != 0b11); // This instruction shouldn't be able to support register-to-register mode.

#ifdef _DEBUG
            // NOTE: Technically there is no REG field in this instruction, it is always 0b000.
            const uint8_t REG = (mod_reg_r_m_byte >> 3) & 0b111;
            assert(REG == 0b000);
#endif
            const uint8_t R_M = mod_reg_r_m_byte & 0b111;

            const std::string dst = get_memory_address_expression(MOD, R_M);

            output_stream << dst << ", " << size_expression << " " << std::to_string(get_immediate_value(W));
        }
        else if (((opcode_byte >> 4) & 0b1111)== 0b1011) // Immediate to register
        {
            const uint8_t W = static_cast<uint8_t>((opcode_byte >> 3) & 0b1);
            const uint8_t REG = static_cast<uint8_t>(opcode_byte & 0b111);

            const std::string register_name = get_register_name(REG, W == 0);

            output_stream << register_name << ", " << std::to_string(get_immediate_value(W));
        }
        else if (((instruction_stream[0] >> 1) & 0b1111111) == 0b1010000) // Memory to accumulator
        {
            assert(false);
        }
        else if (((instruction_stream[0] >> 1) & 0b1111111) == 0b1010001) // Accumulator to memory
        {
            assert(false);
        }
        else
        {
            std::cout << "[ERROR]: Unknown instruction. Can't derive the opcode." << std::endl;
            return -1;
        }

        output_stream << "\n";
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