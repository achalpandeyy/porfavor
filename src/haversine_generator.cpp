#include "haversine_common.h"

#include <random>

int main(int argc, char **argv)
{
    u64 max_pair_count = 100000000;
    
    if (argc != 4)
    {
        fprintf(stdout, "Usage: haversine_generator.exe [uniform | clustered] [seed] [number of pairs <= %llu]\n", max_pair_count);
        return -1;
    }
    
    char json_path[] = "haversine.json";
    FILE *json_file = fopen(json_path, "w");
    if (!json_file)
        fprintf(stdout, "WARNING: Failed to open file %s\n", json_path);
    
    char *method_name = argv[1];
    
    u32 cluster_count_x;
    u32 cluster_count_y;
    if (strcmp(method_name, "clustered") == 0)
    {
        cluster_count_x = 16;
        cluster_count_y = 16;
    }
    else if (strcmp(method_name, "uniform") == 0)
    {
        cluster_count_x = 1;
        cluster_count_y = 1;
    }
    else
    {
        fprintf(stderr, "ERROR: Invalid sampling method: %s\n", method_name);
        return -1;
    }
    
    u64 seed = ParseU64FromString(argv[2]);
    u64 pair_count = ParseU64FromString(argv[3]);
    if (pair_count > max_pair_count)
    {
        fprintf(stderr, "Max pair count exceeded\n");
        return -1;
    }
    
    fprintf(stdout, "Method: %s\n", method_name);
    fprintf(stdout, "Seed: %llu\n", seed);
    fprintf(stdout, "Pair count: %llu\n", pair_count);
    
    std::mt19937_64 prng(seed);
    
    char answers_path[] = "haversine_answers.f64";
    FILE *answers_file = fopen(answers_path, "wb");
    if (!answers_file)
        fprintf(stdout, "WARNING: Failed to open file %s\n", answers_path);
    
    f64 earth_radius = 6372.8;
    
    fprintf(json_file, "{\n");
    fprintf(json_file, "\t\"pairs\": [\n");
    
    f64 average = 0.0;
    
    u64 total_cluster_count = cluster_count_x * cluster_count_y;
    u64 pairs_per_cluster = (max_pair_count+total_cluster_count-1)/total_cluster_count;
    
    f64 x_min = -180.0;
    f64 x_max = 180.0;
    f64 y_min = -90.0;
    f64 y_max = 90.0;
    
    f64 x_span = x_max-x_min;
    f64 y_span = y_max-y_min;
    
    f64 x_cluster_span = x_span/cluster_count_x;
    f64 y_cluster_span = y_span/cluster_count_y;
    
    u64 pairs_picked = 0;
    
    for (u32 y_cluster = 0; y_cluster < cluster_count_y; ++y_cluster)
    {
        std::uniform_real_distribution<f64> dist_y(y_min + y_cluster*y_cluster_span, y_min + (y_cluster+1)*y_cluster_span);
        for (u32 x_cluster = 0; x_cluster < cluster_count_x; ++x_cluster)
        {
            std::uniform_real_distribution<f64> dist_x(x_min + x_cluster*x_cluster_span, x_min + (x_cluster+1)*x_cluster_span);
            
            for (u32 pair_idx = 0; pair_idx < pairs_per_cluster; ++pair_idx)
            {
                f64 x0 = dist_x(prng);
                f64 y0 = dist_y(prng);
                f64 x1 = dist_x(prng);
                f64 y1 = dist_y(prng);
                
                f64 haversine_distance = ReferenceHaversine(x0, y0, x1, y1, earth_radius);
                if (answers_file)
                    fwrite(&haversine_distance, sizeof(f64), 1, answers_file);
                
                average += haversine_distance;
                
                fprintf(json_file, "\t\t{\"x0\": %.18f, \"y0\": %.18f, \"x1\": %.18f, \"y1\": %.18f}", x0, y0, x1, y1);
                
                pairs_picked++;
                assert(pairs_picked <= pair_count);
                
                if ((pair_count-pairs_picked) >= 1)
                    fprintf(json_file, ",");
                fprintf(json_file, "\n");
                
                if (pairs_picked == pair_count)
                    goto end_picking;
            }
        }
    }
    
    end_picking:
    assert(pairs_picked == pair_count);
    fprintf(json_file, "\t],\n");
    
    average /= pair_count;
    fprintf(stdout, "Expected average: %.18f\n", average);
    
    fprintf(json_file, "\t\"expected_average\": %.18f\n", average);
    
    fprintf(json_file, "}\n");
    
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