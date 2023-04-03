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

    std::vector<std::byte> assembled_code;
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

    std::byte* code_ptr = assembled_code.data();
    std::stringstream output_stream;

    while (code_ptr != assembled_code.data() + assembled_code.size())
    {
        uint64_t instruction_size = 0;

        output_stream << "mov ";

        auto get_register_name = [](const std::byte predicate, const bool is_not_wide) -> std::string
        {
            uint8_t predicate_ = static_cast<uint8_t>(predicate);
            switch (predicate_)
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

        if (((code_ptr[0] >> 2) & std::byte{0b111111}) == std::byte{0b100010}) // Register/memory to/from register
        {
            instruction_size += 2;

            // NOTE:
            // This should cover the following cases:
            // 1. Register to register move:    mov al, bl
            // 2. Register to memory move:      mov [bx + di], cx
            // 3. Memory to register move:      mov cx, [bx + di]

            const std::byte D = (code_ptr[0] >> 1) & std::byte{0b1};
            const std::byte W = (code_ptr[0] & std::byte{0b1});
            const std::byte MOD = (code_ptr[1] >> 6) & std::byte{0b11};
            const std::byte REG = (code_ptr[1] >> 3) & std::byte{0b111};
            const std::byte R_M = code_ptr[1] & std::byte{0b111};

            auto get_effective_address_calculation = [](const std::byte predicate) -> std::string
            {
                uint8_t predicate_ = static_cast<uint8_t>(predicate);
                switch (predicate_)
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
                        assert(!"Invalid R/M value");
                        return "";
                }
            };

            if (MOD == std::byte{0b00})
            {
                const std::string register_name = get_register_name(REG, W == std::byte{0b0});
                std::string memory_address;
                {
                    if (R_M == std::byte{0b110})
                    {
                        const uint16_t direct_address = (static_cast<uint16_t>(code_ptr[3]) << 8) | static_cast<uint16_t>(code_ptr[2]);
                        memory_address = std::to_string(direct_address);
                        instruction_size += 2; // add the bytes for encoding displacement
                    }
                    else
                    {
                        memory_address = get_effective_address_calculation(R_M);
                    }
                }

                switch (static_cast<uint8_t>(D))
                {
                    case 0b0:
                        output_stream << "[" << memory_address << "]" << ", " << register_name;
                        break;

                    case 0b1:
                        output_stream << register_name << ", " << "[" << memory_address << "]";
                        break;

                    default:
                        assert(!"Invalid D value.");
                }
            }
            else if (MOD == std::byte{0b01})
            {
                // 8-bit displacement.
                const uint8_t displacement = static_cast<uint8_t>(code_ptr[2]);

                instruction_size += 1;

                const std::string register_name = get_register_name(REG, W == std::byte{0b0});
                const std::string memory_address = get_effective_address_calculation(R_M);

                switch (static_cast<uint8_t>(D))
                {
                    case 0b0:
                        output_stream << "[" << memory_address << " + " << std::to_string(displacement) << "]" << ", " << register_name;
                        break;

                    case 0b1:
                        output_stream << register_name << ", " << "[" << memory_address << " + " << std::to_string(displacement) << "]";
                        break;

                    default:
                        assert(!"Invalid D value.");
                }
            }
            else if (MOD == std::byte{0b10})
            {
                const uint16_t displacement = (static_cast<uint16_t>(code_ptr[3]) << 8) | static_cast<uint16_t>(code_ptr[2]);

                instruction_size += 2;

                const std::string register_name = get_register_name(REG, W == std::byte{0b0});
                const std::string memory_address = get_effective_address_calculation(R_M);

                switch (static_cast<uint8_t>(D))
                {
                    case 0b0:
                        output_stream << "[" << memory_address << " + " << std::to_string(displacement) << "]" << ", " << register_name;
                        break;

                    case 0b1:
                        output_stream << register_name << ", " << "[" << memory_address << " + " << std::to_string(displacement) << "]";
                        break;

                    default:
                        assert(!"Invalid D value.");
                }
            }
            else if (MOD == std::byte{0b11})
            {
                // NOTE: It just so happens that for MOD 11 (register-to-register mode) both REG and R/M fields are mapped to the same register names, thus
                // we can use the same lambda.
                const std::string R_M_register_name = get_register_name(R_M, W == std::byte{0b0});
                const std::string REG_register_name = get_register_name(REG, W == std::byte{0b0});

                switch (static_cast<uint8_t>(D))
                {
                    case 0b0:
                        output_stream << R_M_register_name << ", " << REG_register_name;
                        break;

                    case 0b1:
                        output_stream << REG_register_name << ", " << R_M_register_name;
                        break;

                    default:
                        assert(!"Invalid D value.");
                }
            }
            else
            {
                std::cout << "[ERROR]: Unknown MOD value." << std::endl;
                return -1;
            }
        }
        else if (((code_ptr[0] >> 1) & std::byte{0b1111111}) == std::byte{0b1100011}) // Immediate to register/memory
        {
            assert(false);
        }
        else if (((code_ptr[0] >> 4) & std::byte{0b1111})== std::byte{0b1011}) // Immediate to register
        {
            instruction_size += 2;

            const uint8_t W = static_cast<uint8_t>((code_ptr[0] >> 3) & std::byte{0b1});
            const uint8_t REG = static_cast<uint8_t>(code_ptr[0] & std::byte{0b111});

            const std::string register_name = get_register_name(static_cast<std::byte>(REG), W == 0);

            uint16_t immediate_value = 0;

            switch (W)
            {
                case 0b0:
                {
                    immediate_value = static_cast<uint16_t>(code_ptr[1]);
                } break;

                case 0b1:
                {
                    immediate_value = (static_cast<uint16_t>(code_ptr[2]) << 8) | static_cast<uint16_t>(code_ptr[1]);
                    instruction_size += 1;
                } break;

                default:
                    assert(!"Invalid W value.");
            }

            output_stream << register_name << ", " << std::to_string(immediate_value);
        }
        else if (((code_ptr[0] >> 1) & std::byte{0b1111111}) == std::byte{0b1010000}) // Memory to accumulator
        {
            assert(false);
        }
        else if (((code_ptr[0] >> 1) & std::byte{0b1111111}) == std::byte{0b1010001}) // Accumulator to memory
        {
            assert(false);
        }
        else
        {
            std::cout << "[ERROR]: Unknown instruction. Can't derive the opcode." << std::endl;
            return -1;
        }

        output_stream << "\n";
        code_ptr += instruction_size;
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