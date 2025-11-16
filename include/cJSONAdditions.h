//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <cJSON.h>

#include "err_t.h"

#define boolString(boolValue)   ((boolValue) ? "true" : "false")
#define _cJSON_free(pointer)    do { if (pointer) { cJSON_free((void *) pointer); pointer = nullptr; } } while (0)

// cJSON stores all numbers as double
// lossless int requirement: doubles can exactly represent integers only up to 2^53-1
#define DBL_LOSSLESS_INT_MAX    9007199254740991.0  // 2^53 - 1
#define DBL_LOSSLESS_INT_MIN   -(DBL_LOSSLESS_INT_MAX)

cJSON *cJSON_AddFixedNumberToObject(cJSON *object, const char * const name, const double number, const int decimalPlaces);

bool cJSON_AddDoubleArrayToArrayInObject(const char *key, cJSON *object, const double *values, int valuesCount);
bool cJSON_AddIntArrayToArrayInObject(const char *key, cJSON *object, const int *values, int valuesCount);
bool cJSON_AddItemToArrayInObject(const char *key, cJSON *object, cJSON *item);

bool cJSON_AddStringToArray(const char *string, cJSON *array);

// caller must free value
bool cJSON_CopyStringValueForKey(const char *&value, cJSON *root, const char *key);
// caller must free value
bool cJSON_CopyStringValueFromArray(const char *&value, cJSON *array, int index);
// caller must free each values[0..<count] as well as values
bool cJSON_CopyStringValuesForArrayKey(const char **&values, int &count, cJSON *root, const char *key);

cJSON *cJSON_CreateOrGetArrayItemInObject(const char *key, cJSON *object, bool &wasCreated);

err_t cJSON_Find(cJSON *object, const char *key, cJSON *&value);

bool cJSON_GetBoolValueFromArray(bool *value, cJSON *array, int index);
bool cJSON_GetDoubleValueFromArray(double *value, cJSON *array, int index);
bool cJSON_GetIntValueFromArray(int *value, cJSON *array, int index);
bool cJSON_GetStringValueFromArray(const char **value, cJSON *array, int index);

bool cJSON_GetBoolValueForKey(bool &value, cJSON *root, const char *key);
bool cJSON_GetDoubleValueForKey(double &value, cJSON *root, const char *key);
bool cJSON_GetIntValueForKey(int &value, cJSON *root, const char *key);
bool cJSON_GetStringValueForKey(const char *&value, cJSON *root, const char *key);

// if cJSON_Get*ValuesForArrayKey returns true and count > 0 caller must free(values)
bool cJSON_GetBoolValuesForArrayKey(bool *&values, int &count, cJSON *root, const char *key);
bool cJSON_GetDoubleValuesForArrayKey(double *&values, int &count, cJSON *root, const char *key);
bool cJSON_GetIntValuesForArrayKey(int *&values, int &count, cJSON *root, const char *key);
bool cJSON_GetStringValuesForArrayKey(const char **&values, int &count, cJSON *root, const char *key);

err_t cJSON_GetLosslessInt64(const cJSON *item, int64_t &value);
err_t cJSON_GetLosslessUInt64(const cJSON *item, uint64_t &value);

// CS (constant string) variants avoid strdup of key; caller ensures key lifetime
err_t cJSON_UpsertCS(cJSON *object, const char *key, cJSON *node);

cJSON *cJSONWithFormat(const char *format, ...);
cJSON *vcJSONWithFormat(const char *format, va_list args);
