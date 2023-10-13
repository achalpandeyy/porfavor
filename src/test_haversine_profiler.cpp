#include "porfavor_types.h"

#include "haversine_common.h"
#include "haversine_profiler.h"

static void Test_NestedScopes0()
{
    PROFILE_FUNCTION;
    
    {
        PROFILE_SCOPE("NestedScopeA");
        {
            PROFILE_SCOPE("NestedScopeB");
            {
                PROFILE_SCOPE("NestedScopeC");
            }
        }
    }
}

static void Test_NestedScopes1()
{
    PROFILE_FUNCTION;
    
    {
        PROFILE_SCOPE("NestedScopeA");
    }
    
    {
        PROFILE_SCOPE("NestedScopeB");
    }
}

static void Test_MultipleHitCountHelper()
{
    PROFILE_FUNCTION;
}

static void Test_MultipleHitCount(u64 hit_count)
{
    PROFILE_FUNCTION;
    for (u64 i = 0; i < hit_count; ++i)
        Test_MultipleHitCountHelper();
}

static void Test_RecursiveFunction0(u64 count)
{
    // NOTE(achal): I do not expect the profiler to consider a recursive function a child of itself.
    PROFILE_FUNCTION;
    
    if (count == 0)
        return;
    else
        return Test_RecursiveFunction0(count-1);
}

static void Test_RecursiveFunction1(u64 count)
{
    // NOTE(achal): I expect this anchor to report the ENTIRE time this function take, with and without children.
    PROFILE_FUNCTION;
    
    {
        // NOTE(achal): By virtue of this function being recursive this scope will also be recursive.
        // I expect the profiler to return the total time spent in this scope, including all the recursive
        // calls and hit count of 1.
        PROFILE_SCOPE("NestedScopeA");
        
        if (count == 0)
            return;
        else
            return Test_RecursiveFunction1(count-1);
    }
}

static ProfileAnchor * GetAnchorFromLabel(char *label)
{
    ProfileAnchor *result = 0;
    
    for (u32 i = 0; i < ArrayCount(g_Profiler.anchors); ++i)
    {
        ProfileAnchor *test_anchor = g_Profiler.anchors + i;
        if (strcmp(test_anchor->label, label) == 0)
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
    
    Test_NestedScopes0();
    Test_NestedScopes1();
    
    u64 hit_count = 10000;
    Test_MultipleHitCount(hit_count);
    
    Test_RecursiveFunction0(10);
    Test_RecursiveFunction1(5);
    
    EndProfiler();
    
    for (u32 i = 1; i < ArrayCount(g_Profiler.anchors); ++i)
    {
        ProfileAnchor *anchor = g_Profiler.anchors + i;
        if (!anchor->label)
            continue;
        
        assert(anchor->elapsed_total < g_Profiler.elapsed);
        assert(anchor->elapsed_children < g_Profiler.elapsed);
        assert(anchor->elapsed_children < anchor->elapsed_total);
    }
    
    // NOTE(achal): For Test_NestedScopes0
    {
        ProfileAnchor *Test_NestedScopes0_anchor = GetAnchorFromLabel("Test_NestedScopes0");
        ProfileAnchor *NestedScopeA_anchor = GetAnchorFromLabel("NestedScopeA");
        ProfileAnchor *NestedScopeB_anchor = GetAnchorFromLabel("NestedScopeB");
        ProfileAnchor *NestedScopeC_anchor = GetAnchorFromLabel("NestedScopeC");
        
        assert(Test_NestedScopes0_anchor->elapsed_children == NestedScopeA_anchor->elapsed_total);
        assert(NestedScopeA_anchor->elapsed_children == NestedScopeB_anchor->elapsed_total);
        assert(NestedScopeB_anchor->elapsed_children == NestedScopeC_anchor->elapsed_total);
    }
    
    // NOTE(achal): For Test_NestedScopes1
    {
        ProfileAnchor *Test_NestedScopes1_anchor = GetAnchorFromLabel("Test_NestedScopes1");
        ProfileAnchor *NestedScopeA_anchor = GetAnchorFromLabel("NestedScopeA");
        ProfileAnchor *NestedScopeB_anchor = GetAnchorFromLabel("NestedScopeB");
        
        assert(Test_NestedScopes1_anchor->elapsed_children == (NestedScopeA_anchor->elapsed_total + NestedScopeB_anchor->elapsed_total));
    }
    
    // NOTE(achal): For Test_MultipleHitCount
    {
        ProfileAnchor *Test_MultipleHitCount_anchor = GetAnchorFromLabel("Test_MultipleHitCount");
        ProfileAnchor *Test_MultipleHitCountHelper_anchor = GetAnchorFromLabel("Test_MultipleHitCountHelper");
        
        
        assert(Test_MultipleHitCount_anchor->elapsed_children == Test_MultipleHitCountHelper_anchor->elapsed_total);
        assert(Test_MultipleHitCountHelper_anchor->hit_count == hit_count);
    }
    
    // NOTE(achal): For Test_RecursiveFunction0
    {
        ProfileAnchor *Test_RecursiveFunction0_anchor = GetAnchorFromLabel("Test_RecursiveFunction0");
        assert(Test_RecursiveFunction0_anchor->hit_count == 1);
    }
    
    // NOTE(achal): For Test_RecursiveFunction1
    {
        ProfileAnchor *Test_RecursiveFunction1_anchor = GetAnchorFromLabel("Test_RecursiveFunction1");
        ProfileAnchor *NestedScopeA_anchor = GetAnchorFromLabel("NestedScopeA");
        
        assert(Test_RecursiveFunction1_anchor->hit_count == 1);
        assert(NestedScopeA_anchor->hit_count == 1);
        assert(Test_RecursiveFunction1_anchor->elapsed_children == NestedScopeA_anchor->elapsed_total);
    }
    
    return 0;
}