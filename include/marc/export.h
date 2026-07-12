#ifndef MARC_EXPORT_H
#define MARC_EXPORT_H

#if defined(_WIN32) && defined(MARC_BUILDING_SHARED)
#define MARC_API __declspec(dllexport)
#elif defined(_WIN32) && defined(MARC_USING_SHARED)
#define MARC_API __declspec(dllimport)
#elif defined(__GNUC__) || defined(__clang__)
#define MARC_API __attribute__((visibility("default")))
#else
#define MARC_API
#endif

#endif
