#include <benchmark/benchmark.h>

#include <string>

#include <Base/Parameter.h>

static auto makeConfig()
{
    auto cfg = ParameterManager::Create();
    cfg->CreateDocument();

    auto grp = cfg->GetGroup("TopLevelGroup/Grp1/Grp2/Grp3/Grp4/Grp5");
    auto grp2 = cfg->GetGroup("TopLevelGroup/Grp1/Grp2/Grp4");

    grp->SetASCII("Parameter1", "Value1");
    grp2->SetASCII("Parameter1", "Value1");

    return cfg;
}

// -----------------------------------------------------------------------------
// Group access
// -----------------------------------------------------------------------------

static void BM_GetGroup_DeepPath(benchmark::State& state)
{
    auto cfg = makeConfig();

    for (auto _ : state) {
        auto grp = cfg->GetGroup(
            "TopLevelGroup/Grp1/Grp2/Grp3/Grp4/Grp5"
        );

        benchmark::DoNotOptimize(grp);
    }
}
BENCHMARK(BM_GetGroup_DeepPath);

static void BM_GetGroup_TwoGroups(benchmark::State& state)
{
    auto cfg = makeConfig();

    for (auto _ : state) {
        auto grp = cfg->GetGroup(
            "TopLevelGroup/Grp1/Grp2/Grp3/Grp4/Grp5"
        );
        auto grp2 = cfg->GetGroup(
            "TopLevelGroup/Grp1/Grp2/Grp4"
        );

        benchmark::DoNotOptimize(grp);
        benchmark::DoNotOptimize(grp2);
    }
}
BENCHMARK(BM_GetGroup_TwoGroups);

// -----------------------------------------------------------------------------
// Value read/write
// -----------------------------------------------------------------------------

static void BM_SetASCII(benchmark::State& state)
{
    auto cfg = makeConfig();
    auto grp = cfg->GetGroup(
        "TopLevelGroup/Grp1/Grp2/Grp3/Grp4/Grp5"
    );

    for (auto _ : state) {
        grp->SetASCII("Parameter1", "Value1");
    }
}
BENCHMARK(BM_SetASCII);

static void BM_GetBool(benchmark::State& state)
{
    auto cfg = makeConfig();
    auto grp = cfg->GetGroup(
        "TopLevelGroup/Grp1/Grp2/Grp3/Grp4/Grp5"
    );

    grp->SetASCII("Parameter1", "1");

    for (auto _ : state) {
        bool value = grp->GetBool("Parameter1", false);
        benchmark::DoNotOptimize(value);
    }
}
BENCHMARK(BM_GetBool);

// -----------------------------------------------------------------------------
// Combined realistic access
// -----------------------------------------------------------------------------

static void BM_GetGroup_Set_Get(benchmark::State& state)
{
    auto cfg = makeConfig();

    for (auto _ : state) {
        auto grp = cfg->GetGroup(
            "TopLevelGroup/Grp1/Grp2/Grp3/Grp4/Grp5"
        );

        grp->SetASCII("Parameter1", "Value1");

        auto value = grp->GetBool("Parameter1", false);

        benchmark::DoNotOptimize(grp);
        benchmark::DoNotOptimize(value);
    }
}
BENCHMARK(BM_GetGroup_Set_Get);

// -----------------------------------------------------------------------------
// Cached group comparison
// -----------------------------------------------------------------------------

static void BM_CachedGroup_Set_Get(benchmark::State& state)
{
    auto cfg = makeConfig();

    auto grp = cfg->GetGroup(
        "TopLevelGroup/Grp1/Grp2/Grp3/Grp4/Grp5"
    );

    for (auto _ : state) {
        grp->SetASCII("Parameter1", "Value1");

        auto value = grp->GetBool("Parameter1", false);

        benchmark::DoNotOptimize(value);
    }
}
BENCHMARK(BM_CachedGroup_Set_Get);

BENCHMARK_MAIN();
