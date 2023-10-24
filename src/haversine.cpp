#include "haversine_common.h"

// #define ENABLE_PROFILER
#include "haversine_profiler.h"

#include <float.h>

static b32 StringsEqual(char *non_null_terminated, char *null_terminated, u32 len)
{
    char nt[32];
    assert((u32)sizeof(nt) >= len+1);
    memcpy(nt, non_null_terminated, len);
    nt[len] = '\0';
    return (b32)(strcmp(nt, null_terminated) == 0);
}

struct ParsedJSONLine
{
    f64 x0;
    f64 y0;
    f64 x1;
    f64 y1;
    f64 expected_average;
};

static inline b32 IsPairComplete(ParsedJSONLine *line)
{
    b32 result = (line->x0 != DBL_MAX) && (line->y0 != DBL_MAX) && (line->x1 != DBL_MAX) && (line->y1 != DBL_MAX);
    return result;
}

static f64 ParseF64FromString(char *string, u32 *bytes_parsed)
{
    PROFILE_FUNCTION;
    
    f64 result = 0;
    char *ch = string;
    
    assert((*ch != ' ') && "My generator does not put spaces here");
    
    b32 is_negative = 0;
    if (*ch == '-')
    {
        is_negative = 1;
        ++ch;
    }
    
    u32 pre_decimal_digit_count = 0;
    {
        char *temp = ch;
        while (*ch != '.')
            ++ch;
        pre_decimal_digit_count = (u32)(ch-temp);
    }
    
    f64 digit_scale = 1.0;
    for (u32 i = 0; i < pre_decimal_digit_count; ++i)
    {
        char digit = *(ch - (i + 1));
        assert(isdigit(digit));
        
        f64 digit_f64 = (f64)(digit - '0');
        result += digit_scale*digit_f64;
        
        digit_scale *= 10.0;
    }
    
    // move from the decimal to the first digit after decimal
    ++ch;
    
    digit_scale = 0.1;
    while ((*ch != ',') && (*ch != '}') && (*ch != '\n'))
    {
        assert(isdigit(*ch));
        
        f64 digit_f64 = (f64)(*ch - '0');
        result += digit_scale*digit_f64;
        
        digit_scale *= 0.1;
        ++ch;
    }
    
    if (bytes_parsed)
        *bytes_parsed = (u32)(ch-string);
    
    if (is_negative)
        result *= -1.0;
    
    return result;
}

static b32 ParseJSONLine(char *ch, ParsedJSONLine *parsed_line)
{
    switch (*ch)
    {
        case '{':
        case '\t':
        case '"':
        case ':':
        case ' ':
        case '[':
        case ',':
        case '}':
        case ']':
        {
            ++ch;
            return ParseJSONLine(ch, parsed_line);
        } break;
        
        case '\n':
        {
            return 1;
        } break;
        
        case 'p':
        {
            char pairs_str[] = "pairs";
            u32 len = (u32)strlen(pairs_str);
            if (StringsEqual(ch, pairs_str, len))
            {
                ch += len;
                return ParseJSONLine(ch, parsed_line);
            }
            else
            {
                assert(0);
                return 0;
            }
        } break;
        
        case 'x':
        case 'y':
        {
            b32 is_x = (b32)(*ch == 'x');
            ++ch;
            
            u64 idx;
            {
                char nt[4];
                nt[0] = *ch;
                nt[1] = '\0';
                idx = ParseU64FromString(nt);
            }
            assert(idx <= 1);
            
            ++ch;
            
            char next_chars[] = "\": ";
            u32 len = (u32)strlen(next_chars);
            if (StringsEqual(ch, next_chars, len))
            {
                ch += len;
                
                u32 bytes_parsed;
                f64 value = ParseF64FromString(ch, &bytes_parsed);
                
                if (is_x)
                {
                    if (idx == 0)
                        parsed_line->x0 = value;
                    else if (idx == 1)
                        parsed_line->x1 = value;
                    else
                        assert(0);
                }
                else
                {
                    if (idx == 0)
                        parsed_line->y0 = value;
                    else if (idx == 1)
                        parsed_line->y1 = value;
                    else
                        assert(0);
                }
                
                ch += bytes_parsed;
                
                return ParseJSONLine(ch, parsed_line);
            }
            else
            {
                assert(0);
                return 0;
            }
        } break;
        
        case 'e':
        {
            char expected_average_str[] = "expected_average";
            u32 len = (u32)strlen(expected_average_str);
            if (StringsEqual(ch, expected_average_str, len))
            {
                ch += len;
                
                char next_chars[] = "\": ";
                len = (u32)strlen(next_chars);
                if (StringsEqual(ch, next_chars, len))
                {
                    ch += len;
                    
                    u32 bytes_parsed;
                    f64 value = ParseF64FromString(ch, &bytes_parsed);
                    
                    parsed_line->expected_average = value;
                    ch += bytes_parsed;
                    
                    return ParseJSONLine(ch, parsed_line);
                }
                else
                {
                    assert(0);
                    return 0;
                }
            }
            else
            {
                assert(0);
                return 0;
            }
        } break;
        
        default:
        return 0;
    }
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3)
    {
        fprintf(stderr, "Usage:\n\thaversine.exe [haversine_input.json]\n\thaversine.exe [haversine_input.json] [answers.f64]");
        return -1;
    }
    
    BeginProfiler();
    
    char *input_path = argv[1];
    char *answers_path = 0;
    if (argc == 3)
        answers_path = argv[2];
    
    assert(input_path);
    
    fprintf(stdout, "input_path: %s\n", input_path);
    if (answers_path)
        fprintf(stdout, "answers_path: %s\n", answers_path);
    
    FILE *file = 0;
    FILE *answers_file = 0;
    {
        PROFILE_SCOPE("Read");
        
        file = fopen(input_path, "r");
        assert(file);
        
        if (answers_path)
        {
            answers_file = fopen(answers_path, "rb");
            assert(answers_file);
        }
    }
    
    f64 average = 0.0;
    u64 pair_count = 0;
    char line[1024];
    ParsedJSONLine parsed_line = { DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX };
    
    {
        PROFILE_SCOPE("Parse");
        
        while (fgets(line, sizeof(line), file))
        {
            b32 success = ParseJSONLine(line, &parsed_line);
            if (success)
            {
                if (IsPairComplete(&parsed_line))
                {
                    ++pair_count;
                    
                    f64 haversine_distance = ReferenceHaversine(parsed_line.x0, parsed_line.y0, parsed_line.x1, parsed_line.y1, g_EarthRadius);
                    average += haversine_distance;
                    
                    if (answers_file)
                    {
                        f64 answer;
                        fread(&answer, sizeof(f64), 1, answers_file);
                        
                        f64 threshold = 1e-10;
                        f64 abs_diff = fabs(answer-haversine_distance);
                        assert(abs_diff <= threshold);
                    }
                }
            }
            else
            {
                fprintf(stderr, "ERROR: Failed to parse line: %s\n", line);
            }
            
            memset(line, 0, sizeof(line));
            
            parsed_line.x0 = DBL_MAX;
            parsed_line.y0 = DBL_MAX;
            parsed_line.x1 = DBL_MAX;
            parsed_line.y1 = DBL_MAX;
        }
        average /= pair_count;
    }
    
    {
        PROFILE_SCOPE("Cleanup");
        
        if (answers_file)
            fclose(answers_file);
        
        fclose(file);
    }
    
    {
        PROFILE_SCOPE("Misc Output");
        
        fprintf(stdout, "\nPair count: %llu\n", pair_count);
        fprintf(stdout, "Haversine average: %.15f\n", average);
        
        fprintf(stdout, "\nValidation:\n");
        fprintf(stdout, "Reference average: %.15f\n", parsed_line.expected_average);
        fprintf(stdout, "Difference: %.15f\n", fabs(parsed_line.expected_average - average));
    }
    
    EndProfiler();
    
    fprintf(stdout, "\nPerformance Profile:\n");
    
    u64 cpu_freq = EstimateCPUFrequency(10);
    u64 total_time = g_Profiler.elapsed;
    f64 total_ms = ((f64)total_time/(f64)cpu_freq)*1000.0;
    
    fprintf(stdout, "Total time: %llu | %.4fms (CPU Frequency Estimate: %llu)\n", total_time, total_ms, cpu_freq);
    PrintPerformanceProfile();
    
    return 0;
}
PROFILER_END_OF_COMPILATION_UNIT;