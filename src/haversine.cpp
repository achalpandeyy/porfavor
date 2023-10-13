// #define HAVERSINE_DEBUG

typedef int b32;

#define ArrayCount(arr) (sizeof(arr)/sizeof(arr[0]))

#include "haversine_common.h"
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

static f64 ParseF64JSON(char *non_null_terminated, u32 *bytes_parsed)
{
    char *ch = non_null_terminated;
    
    u32 len = 0;
    char nt[32];
    while ((*ch != ',') && (*ch != '}') && (*ch != '\n'))
        nt[len++] = *ch++;
    nt[len] = '\0';
    
    if (bytes_parsed)
        *bytes_parsed = len;
    
    char *endptr = 0;
    f64 result = strtod(nt, &endptr);
    if (endptr != nt+len)
    {
        fprintf(stdout, "VALUE: %s\n", nt);
        fprintf(stderr, "ERROR: Invalid floating-point number\n");
        assert(0);
    }
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
                f64 value = ParseF64JSON(ch, &bytes_parsed);
                
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
                    f64 value = ParseF64JSON(ch, &bytes_parsed);
                    
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
static inline f64 GetPercentage(u64 part, u64 whole)
{
    f64 result = ((f64)part*100.0)/(f64)whole;
    return result;
}

static void NestedScopeTest()
{
    AUTOCLOSE_PROFILE_FUNCTION;
    Sleep(250);
    
    {
        AUTOCLOSE_PROFILE_SCOPE(NestedScope1);
        Sleep(250);
        
        {
            AUTOCLOSE_PROFILE_SCOPE(NestedScope2);
            Sleep(250);
            
            {
                AUTOCLOSE_PROFILE_SCOPE(NestedScope3);
                Sleep(250);
            }
        }
    }
}

static void NestedScopeTest2()
{
    AUTOCLOSE_PROFILE_FUNCTION;
    Sleep(500);
    
    {
        AUTOCLOSE_PROFILE_SCOPE(NestedScope1);
        Sleep(250);
    }
    
    {
        AUTOCLOSE_PROFILE_SCOPE(NestedScope2);
        Sleep(250);
    }
}

static void MultipleHitCountTestHelper(u32 sleep_time)
{
    AUTOCLOSE_PROFILE_FUNCTION;
    Sleep(sleep_time);
}

static void MultipleHitCountTest(u32 count)
{
    AUTOCLOSE_PROFILE_FUNCTION;
    for (u32 i = 0; i < count; ++i)
        MultipleHitCountTestHelper(1000/count);
}

static void RecursiveFunctionTest()
{
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3)
    {
        fprintf(stderr, "Usage:\n\thaversine.exe [haversine_input.json]\n\thaversine.exe [haversine_input.json] [answers.f64]");
        return -1;
    }
    
    BeginProfiler();
    // NestedScopeTest();
    // NestedScopeTest2();
    MultipleHitCountTest(2);
    
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
        AUTOCLOSE_PROFILE_SCOPE(Read);
        
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
        AUTOCLOSE_PROFILE_SCOPE(Parse);
        
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
                        u64 retval = fread(&answer, sizeof(f64), 1, answers_file);
                        assert(retval == 1);
                        assert(haversine_distance == answer);
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
        AUTOCLOSE_PROFILE_SCOPE(Cleanup);
        
        if (answers_file)
            fclose(answers_file);
        
        fclose(file);
    }
    
    {
        AUTOCLOSE_PROFILE_SCOPE(MiscOutput);
        
        fprintf(stdout, "\nPair count: %llu\n", pair_count);
        fprintf(stdout, "Haversine average: %.18f\n", average);
        
        fprintf(stdout, "\nValidation:\n");
        fprintf(stdout, "Reference average: %.18f\n", parsed_line.expected_average);
        fprintf(stdout, "Difference: %.18f\n", fabs(parsed_line.expected_average - average));
    }
    
    EndProfiler();
    
    fprintf(stdout, "\nPerformance Profile:\n");
    
    u64 cpu_freq = EstimateCPUFrequency(10);
    
    u64 total_time = g_Profiler.elapsed;
    f64 total_ms = ((f64)total_time/(f64)cpu_freq)*1000.0;
    
    fprintf(stdout, "Total time: %.4fms (CPU Frequency Estimate: %llu)\n", total_ms, cpu_freq);
    for (u32 i = 0; i < ArrayCount(g_Profiler.anchors); ++i)
    {
        ProfileAnchor *anchor = g_Profiler.anchors + i;
        if (!anchor->label)
            continue;
        
        u64 elapsed_exclusive = anchor->elapsed_total - anchor->elapsed_children;
        fprintf(stdout, "\t%s[%llu]: %llu (%.3f%%)", anchor->label, anchor->hit_count, elapsed_exclusive, GetPercentage(elapsed_exclusive, total_time));
        if (anchor->elapsed_children)
            fprintf(stdout, ", w/children: %llu (%.3f%%)", anchor->elapsed_total, GetPercentage(anchor->elapsed_total, total_time));
        fprintf(stdout, "\n");
    }
    
    return 0;
}
static_assert(ArrayCount(g_Profiler.anchors) >= __COUNTER__+1, "Ran out of `ProfileAnchor`s");