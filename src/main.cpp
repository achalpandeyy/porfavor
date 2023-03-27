#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

int main()
{
    std::vector<std::byte> assembled_code;
    {
        const char* path = "listing_0037_single_register_mov";
        std::ifstream read_file(path, std::ios::binary | std::ios::ate | std::ios::in);
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
                    std::cout << "[ERROR]: Could not read file " << path << std::endl;
            }
            else
            {
                std::cout << "[ERROR]: Could not get file size." << std::endl;
            }
            
            read_file.close();
        }
        else
        {
            std::cout << "[ERROR]: Could not open file " << path << std::endl;
            return -1;
        }
    }

    constexpr std::byte MOV_OPCODE = std::byte{0b100010};

    const std::byte opcode = (assembled_code[0] >> 2) & std::byte{0x3f};
    if (opcode != MOV_OPCODE)
    {
        std::cout << "[ERROR]: Invalid opcode. Only the mov instruction is supported." << std::endl;
        return -1;
    }

    std::stringstream output_stream;
    output_stream << "mov ";

    constexpr std::byte REGISTER_TO_REGISTER_MOD = std::byte{0b11};

    const std::byte mod = (assembled_code[1] >> 6) & std::byte{0b11};
    if (mod != REGISTER_TO_REGISTER_MOD)
    {
        std::cout << "[ERROR]: Invalid MOD value. Only register-to-register mode is supported." << std::endl;
        return -1;
    }

    const std::byte reg = (assembled_code[1] >> 3) & std::byte{0b111};
    const std::byte r_m = assembled_code[1] & std::byte{0b111};
    const std::byte w = assembled_code[0] & std::byte{0b1};

    if (w == std::byte{0b0})
    {
        if (r_m == std::byte{0b000})
            output_stream << "al, ";
        else if (r_m == std::byte{0b001})
            output_stream << "cl, ";
        else if (r_m == std::byte{0b010})
            output_stream << "dl, ";
        else if (r_m == std::byte{0b011})
            output_stream << "bl, ";
        else if (r_m == std::byte{0b100})
            output_stream << "ah, ";
        else if (r_m == std::byte{0b101})
            output_stream << "ch, ";
        else if (r_m == std::byte{0b110})
            output_stream << "dh, ";
        else if (r_m == std::byte{0b111})
            output_stream << "bh, ";
        else
        {
            std::cout << "Invalid R/M value." << std::endl;
            return -1;
        }

        if (reg == std::byte{0b000})
            output_stream << "al";
        else if (reg == std::byte{0b001})
            output_stream << "cl";
        else if (reg == std::byte{0b010})
            output_stream << "dl";
        else if (reg == std::byte{0b011})
            output_stream << "bl";
        else if (reg == std::byte{0b100})
            output_stream << "ah";
        else if (reg == std::byte{0b101})
            output_stream << "ch";
        else if (reg == std::byte{0b110})
            output_stream << "dh";
        else if (reg == std::byte{0b111})
            output_stream << "bh";
        else
        {
            std::cout << "Invalid REG value." << std::endl;
            return -1;
        }
    }
    else if (w == std::byte{0b1})
    {
        if (r_m == std::byte{0b000})
            output_stream << "ax, ";
        else if (r_m == std::byte{0b001})
            output_stream << "cx, ";
        else if (r_m == std::byte{0b010})
            output_stream << "dx, ";
        else if (r_m == std::byte{0b011})
            output_stream << "bx, ";
        else if (r_m == std::byte{0b100})
            output_stream << "sp, ";
        else if (r_m == std::byte{0b101})
            output_stream << "bp, ";
        else if (r_m == std::byte{0b110})
            output_stream << "si, ";
        else if (r_m == std::byte{0b111})
            output_stream << "di, ";
        else {
            std::cout << "Invalid R/M value." << std::endl;
            return -1;
        }

        if (reg == std::byte{0b000})
            output_stream << "ax";
        else if (reg == std::byte{0b001})
            output_stream << "cx";
        else if (reg == std::byte{0b010})
            output_stream << "dx";
        else if (reg == std::byte{0b011})
            output_stream << "bx";
        else if (reg == std::byte{0b100})
            output_stream << "sp";
        else if (reg == std::byte{0b101})
            output_stream << "bp";
        else if (reg == std::byte{0b110})
            output_stream << "si";
        else if (reg == std::byte{0b111})
            output_stream << "di";
        else {
            std::cout << "Invalid REG value." << std::endl;
            return -1;
        }
    }
    else
    {
        std::cout << "Invalid W value." << std::endl;
        return -1;
    }

    std::cout << output_stream.str() << std::endl;

    const char* out_file_path = "out_listing_0037_single_register_mov.asm";
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