#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <assert.h>

typedef uint32_t u32;
typedef uint64_t u64;
typedef double f64;

#include <random>

static f64 Square(f64 A)
{
    f64 Result = (A*A);
    return Result;
}

static f64 RadiansFromDegrees(f64 Degrees)
{
    f64 Result = 0.01745329251994329577 * Degrees;
    return Result;
}

// NOTE(casey): EarthRadius is generally expected to be 6372.8
static f64 ReferenceHaversine(f64 X0, f64 Y0, f64 X1, f64 Y1, f64 EarthRadius)
{
    /* NOTE(casey): This is not meant to be a "good" way to calculate the Haversine distance.
       Instead, it attempts to follow, as closely as possible, the formula used in the real-world
       question on which these homework exercises are loosely based.
    */
    
    f64 lat1 = Y0;
    f64 lat2 = Y1;
    f64 lon1 = X0;
    f64 lon2 = X1;
    
    f64 dLat = RadiansFromDegrees(lat2 - lat1);
    f64 dLon = RadiansFromDegrees(lon2 - lon1);
    lat1 = RadiansFromDegrees(lat1);
    lat2 = RadiansFromDegrees(lat2);
    
    f64 a = Square(sin(dLat/2.0)) + cos(lat1)*cos(lat2)*Square(sin(dLon/2));
    f64 c = 2.0*asin(sqrt(a));
    
    f64 Result = EarthRadius * c;
    
    return Result;
}

static u64 parse_u64_from_string(const char *num_string)
{
    char *endptr = 0;
    u64 result = (u64)strtoull(num_string, &endptr, 10);
    if (endptr != num_string+strlen(num_string))
    {
        fprintf(stdout, "ERROR: Invalid number\n");
        assert(false);
    }
    return result;
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        fprintf(stdout, "Usage: haversine.exe [uniform] [seed] [number of pairs]\n");
        return -1;
    }
    
    char json_path[] = "haversine.json";
    FILE *json_file = fopen(json_path, "w");
    if (!json_file)
        fprintf(stdout, "WARNING: Failed to open file %s\n", json_path);
    
    char *method_name = argv[1];
    assert(strcmp(method_name, "uniform") == 0);
    u64 seed = parse_u64_from_string(argv[2]);
    u64 pair_count = parse_u64_from_string(argv[3]);
    
    fprintf(stdout, "Method: %s\n", method_name);
    fprintf(stdout, "Seed: %llu\n", seed);
    fprintf(stdout, "Pair count: %llu\n", pair_count);
    
    std::uniform_real_distribution<f64> dist_x(-180.0, 180.0);
    std::uniform_real_distribution<f64> dist_y(-90.f, 90.f);
    std::mt19937_64 prng(seed);
    
    char answers_path[] = "haversine_answers.f64";
    FILE *answers_file = fopen(answers_path, "wb");
    if (!answers_file)
        fprintf(stdout, "WARNING: Failed to open file %s\n", answers_path);
    
    f64 earth_radius = 6372.8;
    
    fprintf(json_file, "{\"pairs\":[\n");
    
    f64 average = 0.0;
    
    for (u64 i = 0; i < pair_count; ++i)
    {
        f64 x0 = dist_x(prng);
        f64 y0 = dist_y(prng);
        f64 x1 = dist_x(prng);
        f64 y1 = dist_y(prng);
        
        f64 haversine_distance = ReferenceHaversine(x0, y0, x1, y1, earth_radius);
        if (answers_file)
            fwrite(&haversine_distance, sizeof(f64), 1, answers_file);
        
        average += haversine_distance;
        
        fprintf(json_file, "\t{x0: \"%.18f\", y0: \"%.18f\", x1: \"%.18f\", y1: \"%.18f\"}", x0, y0, x1, y1);
        if (i != (pair_count-1))
            fprintf(json_file, ",");
        fprintf(json_file, "\n");
    }
    fprintf(json_file, "]}\n");
    
    average /= pair_count;
    fprintf(stdout, "Expected average: %.18f\n", average);
    
    if (answers_file)
    {
        fprintf(stdout, "INFO: Successfully written the haversine answeres binary data file\n");
        fclose(answers_file);
    }
    
    if (json_file)
    {
        fprintf(stdout, "INFO: Successfully written the JSON file\n");
        fclose(json_file);
    }
    
    return 0;
}