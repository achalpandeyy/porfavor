#include "haversine_common.h"
#include "platform_metrics.h"

// #define READ_SCOPE_TIMER ReadOSTimer
#define ENABLE_PROFILER 1
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

struct HaversinePair
{
    f64 x0, y0;
    f64 x1, y1;
};

static inline b32 IsPairComplete(ParsedJSONLine *line)
{
    b32 result = (line->x0 != DBL_MAX) && (line->y0 != DBL_MAX) && (line->x1 != DBL_MAX) && (line->y1 != DBL_MAX);
    return result;
}

static f64 ParseF64FromString(char *string, u32 *bytes_parsed)
{
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

// TODO(achal): This facilitates an incorrect way of parsing JSON and I will get rid of this
// in the future when I introduce lexing-based parsing.
// NOTE(achal): Reads until a newline character is encountered.
static u64 ReadLine(char *line, u32 max_line_size, u8 *json_data)
{
    u32 char_count = 0;
    u8 *src = json_data;
    
    while (*src != '\r')
    {
        *line++ = *src++;
        ++char_count;
    }
    
    src++; // eat up the '\r' character
    ++char_count;
    
    // NOTE(achal): We still have to read the newline char at the end.
    *line++ = *src++;
    ++char_count;
    
    assert(char_count <= max_line_size);
    return char_count;
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3)
    {
        fprintf(stderr, "Usage:\n\thaversine.exe [haversine_input_<pair_count>.json]\n\thaversine.exe [haversine_input_<pair_count>.json] [haversine_answers_<pair_count>.f64]");
        return -1;
    }
    
#if ENABLE_PROFILER
    printf("Profiler: Enabled\n");
#else
    printf("Profiler: Disabled\n");
#endif
    
    BeginProfiler();
    
    char *input_path = argv[1];
    char *answers_path = 0;
    if (argc == 3)
        answers_path = argv[2];
    
    assert(input_path);
    
    u64 pair_count = 0;
    {
        u64 input_path_len = strlen(input_path);
        
        char input_path_prefix[] = "haversine_input_";
        u64 input_path_prefix_len = strlen(input_path_prefix);
        assert(input_path_len >= input_path_prefix_len);
        
        char *pair_count_str_begin = input_path + input_path_prefix_len;
        char *pair_count_str_end = strchr(pair_count_str_begin, '.');
        assert(pair_count_str_end);
        
        u64 len = pair_count_str_end-pair_count_str_begin;
        char temp[32];
        memcpy(temp, pair_count_str_begin, len);
        temp[len] = '\0';
        pair_count = ParseU64FromString(temp);
    }
    assert(pair_count != 0);
    printf("Pair Count: %llu\n", pair_count);
    
    fprintf(stdout, "input_path: %s\n", input_path);
    if (answers_path)
        fprintf(stdout, "answers_path: %s\n", answers_path);
    
    u8 *json_data = 0;
    u64 json_size = 0;
    {
        PROFILE_SCOPE("Read");
        
        FILE *file = fopen(input_path, "rb");
        assert(file);
        
        struct __stat64 stat;
        {
            int retval = _stat64(input_path, &stat);
            assert(retval == 0);
        }
        
        json_data = (u8 *)malloc(stat.st_size);
        assert(json_data);
        
        {
            PROFILE_SCOPE_BANDWIDTH("fread", stat.st_size);
            json_size = fread(json_data, 1, stat.st_size, file);
        }
        
        fclose(file);
        assert(json_size <= (size_t)stat.st_size);
    }
    
    HaversinePair *haversine_pairs = (HaversinePair *)malloc(pair_count*sizeof(HaversinePair));
    assert(haversine_pairs);
    
    char line[1024];
    ParsedJSONLine parsed_line = { DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX };
    
    u64 parsed_pair_count = 0;
    {
        PROFILE_SCOPE("Parse");
        
        memset(line, 0, sizeof(line));
        u64 json_byte_offset = 0;
        while (json_byte_offset < json_size)
        {
            u64 parsed_char_count = ReadLine(line, sizeof(line), json_data+json_byte_offset);
            b32 success = ParseJSONLine(line, &parsed_line);
            if (success)
            {
                if (IsPairComplete(&parsed_line))
                {
                    haversine_pairs[parsed_pair_count++] = {parsed_line.x0, parsed_line.y0, parsed_line.x1, parsed_line.y1};
                }
            }
            else
            {
                fprintf(stderr, "ERROR: Failed to parse line: %s\n", line);
            }
            
            json_byte_offset += parsed_char_count;
            
            memset(line, 0, sizeof(line));
            
            parsed_line.x0 = DBL_MAX;
            parsed_line.y0 = DBL_MAX;
            parsed_line.x1 = DBL_MAX;
            parsed_line.y1 = DBL_MAX;
        }
        assert(json_byte_offset == json_size);
        assert(parsed_pair_count == pair_count);
    }
    
    FILE *answers_file = 0;
    if (answers_path)
    {
        answers_file = fopen(answers_path, "rb");
        assert(answers_file);
    }
    
    f64 average = 0.0;
    {
        PROFILE_SCOPE_BANDWIDTH("Sum Haversine Pairs", pair_count*sizeof(HaversinePair));
        
        for (u64 i = 0; i < pair_count; ++i)
        {
            HaversinePair *pair = haversine_pairs + i;
            
            f64 haversine_distance = ReferenceHaversine(pair->x0, pair->y0, pair->x1, pair->y1, g_EarthRadius);
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
    average /= pair_count;
    
    {
        PROFILE_SCOPE("Cleanup");
        
        if (answers_file)
            fclose(answers_file);
        
        free(haversine_pairs);
        free(json_data);
    }
    
    {
        PROFILE_SCOPE("Misc Output");
        
        fprintf(stdout, "Haversine average: %.15f\n", average);
        
        fprintf(stdout, "\nValidation:\n");
        fprintf(stdout, "Reference average: %.15f\n", parsed_line.expected_average);
        fprintf(stdout, "Difference: %.15f\n", fabs(parsed_line.expected_average - average));
    }
    
    EndProfiler();
    PrintPerformanceProfile();
    
    return 0;
}
PROFILER_END_OF_COMPILATION_UNIT;