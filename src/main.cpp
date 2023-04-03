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

        if (((code_ptr[0] >> 2) & std::byte{0b111111}) == std::byte{0b100010}) // Register/memory to/from register
        {
            // If D = 0, then REG gives the src register and R/M gives the dst register.
            // If D = 1, then REG gives the dst register and R/M gives the src register.

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

            auto insert_register_name = [&output_stream](const std::byte predicate, const std::byte w) -> bool
            {
                if (predicate == std::byte{0b000})
                    w == std::byte{0b0} ? output_stream << "al" : output_stream << "ax";
                else if (predicate == std::byte{0b001})
                    w == std::byte{0b0} ? output_stream << "cl" : output_stream << "cx";
                else if (predicate == std::byte{0b010})
                    w == std::byte{0b0} ? output_stream << "dl" : output_stream << "dx";
                else if (predicate == std::byte{0b011})
                    w == std::byte{0b0} ? output_stream << "bl" : output_stream << "bx";
                else if (predicate == std::byte{0b100})
                    w == std::byte{0b0} ? output_stream << "ah" : output_stream << "sp";
                else if (predicate == std::byte{0b101})
                    w == std::byte{0b0} ? output_stream << "ch" : output_stream << "bp";
                else if (predicate == std::byte{0b110})
                    w == std::byte{0b0} ? output_stream << "dh" : output_stream << "si";
                else if (predicate == std::byte{0b111})
                    w == std::byte{0b0} ? output_stream << "bh" : output_stream << "di";
                else
                    return false;

                return true;
            };

            if (MOD == std::byte{0b00})
            {
                instruction_size += 2;

                uint16_t direct_address = UINT16_MAX; // Assuming this won't be a direct address, but it could be, who knows.

                if (R_M == std::byte{0b110})
                {
                    direct_address = (static_cast<uint16_t>(code_ptr[3]) << 8) | static_cast<uint16_t>(code_ptr[2]);
                    instruction_size += 2; // add the bytes for encoding displacement
                }

                auto insert_address_calculation = [&output_stream, direct_address](const std::byte R_M_) -> bool
                {
                    if (R_M_ == std::byte{0b000})
                        output_stream << "[bx + si]";
                    else if (R_M_ == std::byte{0b001})
                        output_stream << "[bx + di]";
                    else if (R_M_ == std::byte{0b010})
                        output_stream << "[bp + si]";
                    else if (R_M_ == std::byte{0b011})
                        output_stream << "[bp + di]";
                    else if (R_M_ == std::byte{0b100})
                        output_stream << "[si]";
                    else if (R_M_ == std::byte{0b101})
                        output_stream << "[di]";
                    else if (R_M_ == std::byte{0b110})
                        output_stream << "[" << direct_address << "]";
                    else if (R_M_ == std::byte{0b111})
                        output_stream << "[bx]";
                    else
                        return false;
                    
                    return true;
                };

                if (!((D == std::byte{0b0}) ? insert_address_calculation(R_M) : insert_register_name(REG, W)))
                {
                    std::cout << "[ERROR]: Invalid " << ((D == std::byte{0b0}) ? "R/M" : "REG") << " value." << std::endl;
                    return -1;
                }

                output_stream << ", ";

                if (!((D == std::byte{0b1}) ? insert_address_calculation(R_M) : insert_register_name(REG, W)))
                {
                    std::cout << "[ERROR]: Invalid " << ((D == std::byte{0b1}) ? "R/M" : "REG") << " value." << std::endl;
                    return -1;
                }
            }
            else if (MOD == std::byte{0b01})
            {
                // 8-bit displacement.
            }
            else if (MOD == std::byte{0b10})
            {
                // 16-bit displacement.
            }
            else if (MOD == std::byte{0b11})
            {
                // NOTE: It just so happens that for MOD 11 (register-to-register mode) both REG and R/M fields are mapped to the same register names.
                assert(D == std::byte{0b0} || D == std::byte{0b1});

                if (!insert_register_name((D == std::byte{0b0} ? R_M : REG), W))
                {
                    std::cout << "Invalid " << ((D == std::byte{0b0}) ? "R/M" : "REG") << " value." << std::endl;
                    return -1;
                }

                output_stream << ", ";

                if (!insert_register_name((D == std::byte{0b1}) ? R_M : REG, W))
                {
                    std::cout << "Invalid " << (D == std::byte{0b1} ? "R/M" : "REG") << " value." << std::endl;
                    return -1;
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

        }
        else if (((code_ptr[0] >> 4) & std::byte{0b1111})== std::byte{0b1011}) // Immediate to register
        {

        }
        else if (((code_ptr[0] >> 1) & std::byte{0b1111111}) == std::byte{0b1010000}) // Memory to accumulator
        {

        }
        else if (((code_ptr[0] >> 1) & std::byte{0b1111111}) == std::byte{0b1010001}) // Accumulator to memory
        {

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