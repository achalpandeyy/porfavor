#include "porfavor_types.h"

#include "haversine_common.h"
#include "haversine_profiler.h"

static void Test0_NestedScopes()
{
    PROFILE_FUNCTION;
    
    {
        PROFILE_SCOPE("0A");
        {
            PROFILE_SCOPE("0B");
            {
                PROFILE_SCOPE("0C");
            }
        }
    }
}

static void Test1_NestedScopes()
{
    PROFILE_FUNCTION;
    
    {
        PROFILE_SCOPE("1A");
    }
    
    {
        PROFILE_SCOPE("1B");
    }
}

static void Test2_MultipleHitCountHelper()
{
    PROFILE_FUNCTION;
}

static void Test2_MultipleHitCount(u64 hit_count)
{
    PROFILE_FUNCTION;
    for (u64 i = 0; i < hit_count; ++i)
        Test2_MultipleHitCountHelper();
}

// NOTE(achal): AAAAAA... recursion
static void Test3_RecursiveFunction(u64 count)
{
    // NOTE(achal): I do not expect the profiler to consider a recursive function a child of itself.
    PROFILE_FUNCTION;
    
    if (count == 0)
        return;
    else
        return Test3_RecursiveFunction(count-1);
}

// NOTE(achal): ABABABAB... recursion
static void Test4_RecursiveFunction(u64 count)
{
    // NOTE(achal): I expect this anchor to report the ENTIRE time this function take, with and without children.
    PROFILE_FUNCTION;
    
    {
        // NOTE(achal): By virtue of this function being recursive this scope will also be recursive.
        // I expect the profiler to return the total time spent in this scope, including all the recursive
        // calls and hit count of 1.
        PROFILE_SCOPE("4A");
        
        if (count == 0)
            return;
        else
            return Test4_RecursiveFunction(count-1);
    }
}

// TODO(achal): ABCABCABC... recursion
// TODO(achal): AABCABACA.. or some random recurion
// TODO(achal): A recursive function which has multiple (non-nested) scopes

// NOTE(achal): In general, I cannot rely on the fact that no two anchors will never have
// the same label name, because they can! I am artificially enforcing this condition in this
// file for testing purposes.
static ProfileAnchor * GetAnchorFromLabel(char *label)
{
    ProfileAnchor *result = 0;
    
    for (u32 i = 0; i < ArrayCount(g_Profiler.anchors); ++i)
    {
        ProfileAnchor *test_anchor = g_Profiler.anchors + i;
        // TODO(achal): Why does my program silently fails if test_anchor->label is NULL. I expected
        // it crash somewhere in strcmp.. but it didn't :(
        if (test_anchor->label && (strcmp(test_anchor->label, label) == 0))
        {
            result = test_anchor;
            break;
        }
    }
    
    assert(result);
    return result;
}

int main()
{
    BeginProfiler();
    
    Test0_NestedScopes();
    {
        ProfileAnchor *anchor_Test0_NestedScopes = GetAnchorFromLabel("Test0_NestedScopes");
        ProfileAnchor *anchor_0A = GetAnchorFromLabel("0A");
        ProfileAnchor *anchor_0B = GetAnchorFromLabel("0B");
        ProfileAnchor *anchor_0C = GetAnchorFromLabel("0C");
        
        assert(anchor_Test0_NestedScopes->elapsed_children == anchor_0A->elapsed_total);
        assert(anchor_0A->elapsed_children == anchor_0B->elapsed_total);
        assert(anchor_0B->elapsed_children == anchor_0C->elapsed_total);
    }
    
    Test1_NestedScopes();
    {
        ProfileAnchor *anchor_Test1_NestedScopes = GetAnchorFromLabel("Test1_NestedScopes");
        ProfileAnchor *anchor_1A = GetAnchorFromLabel("1A");
        ProfileAnchor *anchor_1B = GetAnchorFromLabel("1B");
        
        assert(anchor_Test1_NestedScopes->elapsed_children == (anchor_1A->elapsed_total + anchor_1B->elapsed_total));
    }
    
    u64 hit_count = 10000;
    Test2_MultipleHitCount(hit_count);
    {
        ProfileAnchor *anchor_Test2_MultipleHitCount = GetAnchorFromLabel("Test2_MultipleHitCount");
        ProfileAnchor *anchor_Test2_MultipleHitCountHelper = GetAnchorFromLabel("Test2_MultipleHitCountHelper");
        
        assert(anchor_Test2_MultipleHitCount->elapsed_children == anchor_Test2_MultipleHitCountHelper->elapsed_total);
        assert(anchor_Test2_MultipleHitCountHelper->hit_count == hit_count);
    }
    
    // TODO(achal): This is the correct behaviour that we want for recursive blocks:
    // 1. A recursive function is NOT its own child.
    // 2. By the above, elapsed_children should NOT contain the time for the recursive call -- that time
    // should be included in the time for the main call.
    Test3_RecursiveFunction(10);
    {
        ProfileAnchor *anchor_Test3_RecursiveFunction = GetAnchorFromLabel("Test3_RecursiveFunction");
        assert(anchor_Test3_RecursiveFunction->hit_count == 1);
    }
    
    u64 recurse_count = 5;
    Test4_RecursiveFunction(recurse_count);
    {
        ProfileAnchor *anchor_Test4_RecursiveFunction = GetAnchorFromLabel("Test4_RecursiveFunction");
        ProfileAnchor *anchor_4A = GetAnchorFromLabel("4A");
        
        // TODO(achal): Should we maintain the hit counts of the recursive function to be 1? I mean,
        // they appear to be getting hit multiple times.
        assert(anchor_Test4_RecursiveFunction->hit_count == 1);
        assert(anchor_4A->hit_count == 1);
        
        assert(anchor_Test4_RecursiveFunction->elapsed_children == (anchor_4A->elapsed_total - anchor_4A->elapsed_children));
        
        if (recurse_count > 0)
            assert(anchor_4A->elapsed_children > 0);
    }
    
    EndProfiler();
    
    // NOTE(achal): Sanity-check all anchors
    for (u32 i = 1; i < ArrayCount(g_Profiler.anchors); ++i)
    {
        ProfileAnchor *anchor = g_Profiler.anchors + i;
        if (!anchor->label)
            continue;
        
        assert(anchor->elapsed_total < g_Profiler.elapsed);
        assert(anchor->elapsed_children < g_Profiler.elapsed);
        assert(anchor->elapsed_children < anchor->elapsed_total);
    }
    
    fprintf(stdout, "Tests Passing\n");
    
    return 0;
}