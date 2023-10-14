#include "porfavor_types.h"

#include "haversine_common.h"
#define ENABLE_PROFILER
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

// NOTE(achal): Recursion: AAAAAA...
static void Test3_RecursiveFunction(u64 count)
{
    // NOTE(achal): I do not expect the profiler to consider a recursive function a child of itself.
    PROFILE_FUNCTION;
    
    if (count == 0)
        return;
    else
        return Test3_RecursiveFunction(count-1);
}

// NOTE(achal): Recursion: ABABABAB...
static void Test4_RecursiveFunction(u64 count)
{
    PROFILE_FUNCTION;
    
    {
        PROFILE_SCOPE("4A");
        
        if (count == 0)
            return;
        else
            return Test4_RecursiveFunction(count-1);
    }
}

// NOTE(achal): Recursion: ABCABCABC...
static void Test5_RecursiveFunction(u64 count);

static void Test5_RecursiveFunctionSecondHelper(u64 count)
{
    PROFILE_FUNCTION;
    return Test5_RecursiveFunction(count-1);
}

static void Test5_RecursiveFunctionFirstHelper(u64 count)
{
    PROFILE_FUNCTION;
    return Test5_RecursiveFunctionSecondHelper(count-1);
}

static void Test5_RecursiveFunction(u64 count)
{
    PROFILE_FUNCTION;
    if (count == 0)
        return;
    else
        return Test5_RecursiveFunctionFirstHelper(count-1);
}

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
        
        assert((anchor_Test0_NestedScopes->elapsed_inclusive-anchor_Test0_NestedScopes->elapsed_exclusive) == anchor_0A->elapsed_inclusive);
        assert((anchor_0A->elapsed_inclusive - anchor_0A->elapsed_exclusive) == anchor_0B->elapsed_inclusive);
        assert((anchor_0B->elapsed_inclusive - anchor_0B->elapsed_exclusive) == anchor_0C->elapsed_inclusive);
    }
    
    Test1_NestedScopes();
    {
        ProfileAnchor *anchor_Test1_NestedScopes = GetAnchorFromLabel("Test1_NestedScopes");
        ProfileAnchor *anchor_1A = GetAnchorFromLabel("1A");
        ProfileAnchor *anchor_1B = GetAnchorFromLabel("1B");
        
        assert((anchor_Test1_NestedScopes->elapsed_inclusive - anchor_Test1_NestedScopes->elapsed_exclusive) == (anchor_1A->elapsed_inclusive + anchor_1B->elapsed_inclusive));
    }
    
    {
        u64 hit_count = 10000;
        Test2_MultipleHitCount(hit_count);
        
        ProfileAnchor *anchor_Test2_MultipleHitCount = GetAnchorFromLabel("Test2_MultipleHitCount");
        ProfileAnchor *anchor_Test2_MultipleHitCountHelper = GetAnchorFromLabel("Test2_MultipleHitCountHelper");
        
        assert((anchor_Test2_MultipleHitCount->elapsed_inclusive-anchor_Test2_MultipleHitCount->elapsed_exclusive) == anchor_Test2_MultipleHitCountHelper->elapsed_inclusive);
        assert(anchor_Test2_MultipleHitCountHelper->hit_count == hit_count);
    }
    
    {
        u64 recurse_count = 10;
        Test3_RecursiveFunction(recurse_count);
        ProfileAnchor *anchor_Test3_RecursiveFunction = GetAnchorFromLabel("Test3_RecursiveFunction");
        assert(anchor_Test3_RecursiveFunction->hit_count == recurse_count+1);
    }
    
    {
        u64 recurse_count = 5;
        Test4_RecursiveFunction(recurse_count);
        
        ProfileAnchor *anchor_Test4_RecursiveFunction = GetAnchorFromLabel("Test4_RecursiveFunction");
        ProfileAnchor *anchor_4A = GetAnchorFromLabel("4A");
        
        assert(anchor_Test4_RecursiveFunction->hit_count == recurse_count+1);
        assert(anchor_4A->hit_count == recurse_count+1);
        
        assert((anchor_Test4_RecursiveFunction->elapsed_inclusive-anchor_Test4_RecursiveFunction->elapsed_exclusive) == anchor_4A->elapsed_exclusive);
        
        if (recurse_count > 0)
            assert((anchor_4A->elapsed_inclusive - anchor_4A->elapsed_exclusive) != 0);
    }
    
    {
        const u64 recurse_count = 3000;
        static_assert((recurse_count % 3) == 0, "Recursion would not terminate!");
        Test5_RecursiveFunction(recurse_count);
        
        ProfileAnchor *anchor_Test5_RecursiveFunction = GetAnchorFromLabel("Test5_RecursiveFunction");
        ProfileAnchor *anchor_Test5_RecursiveFunctionFirstHelper = GetAnchorFromLabel("Test5_RecursiveFunctionFirstHelper");
        ProfileAnchor *anchor_Test5_RecursiveFunctionSecondHelper = GetAnchorFromLabel("Test5_RecursiveFunctionSecondHelper");
        
        assert(anchor_Test5_RecursiveFunction->hit_count == (recurse_count/3)+1);
        assert(anchor_Test5_RecursiveFunctionFirstHelper->hit_count == recurse_count/3);
        assert(anchor_Test5_RecursiveFunctionSecondHelper->hit_count == recurse_count/3);
        
        assert((anchor_Test5_RecursiveFunction->elapsed_inclusive - anchor_Test5_RecursiveFunction->elapsed_exclusive) == (anchor_Test5_RecursiveFunctionFirstHelper->elapsed_exclusive + anchor_Test5_RecursiveFunctionSecondHelper->elapsed_exclusive));
    }
    
    EndProfiler();
    
    // NOTE(achal): Sanity-check all anchors
    for (u32 i = 1; i < ArrayCount(g_Profiler.anchors); ++i)
    {
        ProfileAnchor *anchor = g_Profiler.anchors + i;
        if (!anchor->label)
            continue;
        
        assert(anchor->elapsed_inclusive < g_Profiler.elapsed);
        assert(anchor->elapsed_exclusive < g_Profiler.elapsed);
        assert(anchor->elapsed_exclusive <= anchor->elapsed_inclusive);
    }
    
    fprintf(stdout, "All Tests Passed\n");
    
    return 0;
}