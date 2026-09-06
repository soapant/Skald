#ifndef FEATURES_API_H
#define FEATURES_API_H

// keep feature files as close as now - just add one export
// user defines void FlintFeatureInit(void) called on startup + every reload
#ifdef _WIN32
#define FLINT_FEATURE_EXPORT __declspec(dllexport)
#else
#define FLINT_FEATURE_EXPORT __attribute__((visibility("default")))
#endif

#define FLINT_FEATURE_INIT FLINT_FEATURE_EXPORT void FlintFeatureInit(void)

#endif
