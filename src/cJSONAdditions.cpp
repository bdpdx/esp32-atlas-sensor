#include "cJSONAdditions.h"
#include "common.h"
#include "utility.h"

cJSON *cJSON_AddFixedNumberToObject(cJSON *object, const char * const name, double number, int decimalPlaces) {
    number = roundToPrecision(number, decimalPlaces);
    cJSON *item = cJSON_AddNumberToObject(object, name, number);

    return item;
}

bool cJSON_AddDoubleArrayToArrayInObject(const char *key, cJSON *object, const double *values, int valuesCount) {
    err_t err = 0;
    cJSON *valuesArray;

    if ((valuesArray = cJSON_CreateDoubleArray(values, valuesCount)) == nullptr) err = ENOMEM;
    if (!err && !cJSON_AddItemToArrayInObject(key, object, valuesArray)) err = ENOMEM;
    if (!err) valuesArray = nullptr;

    cJSON_Delete(valuesArray);

    return !err;
}

bool cJSON_AddIntArrayToArrayInObject(const char *key, cJSON *object, const int *values, int valuesCount) {
    err_t err = 0;
    cJSON *valuesArray;

    if ((valuesArray = cJSON_CreateIntArray(values, valuesCount)) == nullptr) err = ENOMEM;
    if (!err && !cJSON_AddItemToArrayInObject(key, object, valuesArray)) err = ENOMEM;
    if (!err) valuesArray = nullptr;

    cJSON_Delete(valuesArray);

    return !err;
}

bool cJSON_AddItemToArrayInObject(const char *key, cJSON *object, cJSON *item) {
    err_t err = 0;
    cJSON *array;
    bool wasCreated = false;

    if ((array = cJSON_CreateOrGetArrayItemInObject(key, object, wasCreated)) == nullptr) err = ENOMEM;
    if (!err && !cJSON_AddItemToArray(array, item)) err = ENOMEM; 

    if (err && wasCreated) cJSON_Delete(array);

    return !err;
}

bool cJSON_AddStringToArray(const char *string, cJSON *array) {
    err_t err = 0;
    cJSON *item;

    if ((item = cJSON_CreateString(string)) == nullptr) setErr(ENOMEM);
    if (!err) setErr(!cJSON_AddItemToArray(array, item));

    if (err) cJSON_Delete(item);

    return !err;
}

bool cJSON_CopyStringValueForKey(const char *&value, cJSON *root, const char *key) {
    err_t err;
    const char *v = nullptr;

    err = !cJSON_GetStringValueForKey(v, root, key);
    if (!err && (value = strdup(v)) == nullptr) err = ENOMEM;

    return err;
}

bool cJSON_CopyStringValueFromArray(const char *&value, cJSON *array, int index) {
    err_t err;
    const char *v = nullptr;

    err = !cJSON_GetStringValueFromArray(&v, array, index);
    if (!err && (value = strdup(v)) == nullptr) err = ENOMEM;

    return err;
}

bool cJSON_CopyStringValuesForArrayKey(const char **&values, int &count, cJSON *root, const char *key) {
    err_t err;
    const char **array = nullptr;
    int i, n;

    err = !cJSON_GetStringValuesForArrayKey(array, n, root, key);
    if (!err) {
        for (i = 0; i < n; ++i) {
            char *p;

            if (err) {
                array[i] = nullptr;
            } else if ((p = strdup(array[i]))) {
                array[i] = p;
            } else {
                array[i] = nullptr;
                err = ENOMEM;
            }
        }

        if (err) {
            for (i = 0; i < n; ++i) {
                if (array[i]) free((void *) array[i]);
            }
        }
    }
    if (!err) values = array;
    else _free(array);

    return err;
}

cJSON *cJSON_CreateOrGetArrayItemInObject(const char *key, cJSON *object, bool &wasCreated) {
    cJSON *array;
    
    wasCreated = false;

    if ((array = cJSON_GetObjectItemCaseSensitive(object, key)) == nullptr) {
        err_t err = 0;

        if ((array = cJSON_CreateArray()) == nullptr) err = ENOMEM;
        if (!err && !cJSON_AddItemToObject(object, key, array)) err = ENOMEM;
        if (!err) {
            wasCreated = true;
        } else {
            cJSON_Delete(array);
            array = nullptr;
        }
    }

    return array;
}

err_t cJSON_Find(cJSON *object, const char *key, cJSON *&value) {
    if (!(object && key && *key)) return EINVAL;

    err_t err = 0;
    cJSON *item;

    if (!(item = cJSON_GetObjectItemCaseSensitive(object, key))) setErr(ENOENT);
    if (!err) value = item;

    return err;
}

bool cJSON_GetBoolValueFromArray(bool *value, cJSON *array, int index) {
    if (!cJSON_IsArray(array)) return false;
    if (index < 0 || index >= cJSON_GetArraySize(array)) return false;

    cJSON *item;
    
    if ((item = cJSON_GetArrayItem(array, index)) == nullptr || !cJSON_IsBool(item)) return false;

    *value = cJSON_IsTrue(item);

    return true;
}

bool cJSON_GetBoolValueForKey(bool &value, cJSON *root, const char *key) {
    cJSON *item;
    
    if ((item = cJSON_GetObjectItem(root, key)) == nullptr || !cJSON_IsBool(item)) return false;

    value = cJSON_IsTrue(item);

    return true;
}

bool cJSON_GetBoolValuesForArrayKey(bool *&values, int &count, cJSON *root, const char *key) {
    cJSON *array;
    bool err;
    bool *items = nullptr;
    int size = 0;

    err = (array = cJSON_GetObjectItem(root, key)) == nullptr;
    if (!err) err = !cJSON_IsArray(array);
    if (!err && (size = cJSON_GetArraySize(array)) > 0) {
        err = (items = (bool *) malloc(sizeof(bool) * size)) == nullptr;
    }        
    for (int i = 0; !err && i < size; ++i) {
        cJSON *item = cJSON_GetArrayItem(array, i);
        err = item == nullptr;
        if (!err) items[i] = cJSON_IsTrue(item);
    }
    if (!err) {
        count = size;
        values = items;
    } else {
        _free(items);
    }

    return !err;
}

bool cJSON_GetDoubleValueFromArray(double *value, cJSON *array, int index) {
    if (!cJSON_IsArray(array)) return false;
    if (index < 0 || index >= cJSON_GetArraySize(array)) return false;

    cJSON *item;
    
    if ((item = cJSON_GetArrayItem(array, index)) == nullptr || !cJSON_IsNumber(item)) return false;

    *value = item->valuedouble;

    return true;
}

bool cJSON_GetDoubleValueForKey(double &value, cJSON *root, const char *key) {
    cJSON *item;
    
    if ((item = cJSON_GetObjectItem(root, key)) == nullptr || !cJSON_IsNumber(item)) return false;

    value = item->valuedouble;

    return true;
}

bool cJSON_GetDoubleValuesForArrayKey(double *&values, int &count, cJSON *root, const char *key) {
    cJSON *array;
    err_t err;
    double *items = nullptr;
    int size = 0;

    setErr((array = cJSON_GetObjectItem(root, key)) == nullptr);
    if (!err) setErr(!cJSON_IsArray(array));
    if (!err && (size = cJSON_GetArraySize(array)) > 0) {
        if ((items = (double *) malloc(sizeof(double) * size)) == nullptr) setErr(errno);
    }
    for (int i = 0; !err && i < size; ++i) {
        cJSON *item = cJSON_GetArrayItem(array, i);
        setErr(item == nullptr || !cJSON_IsNumber(item));
        if (!err) items[i] = item->valuedouble;
    }
    if (!err) {
        count = size;
        values = items;
    } else {
        _free(items);
    }

    return !err;
}

bool cJSON_GetIntValueFromArray(int *value, cJSON *array, int index) {
    if (!cJSON_IsArray(array)) return false;
    if (index < 0 || index >= cJSON_GetArraySize(array)) return false;

    cJSON *item;
    
    if ((item = cJSON_GetArrayItem(array, index)) == nullptr || !cJSON_IsNumber(item)) return false;

    *value = item->valueint;

    return true;
}

bool cJSON_GetIntValueForKey(int &value, cJSON *root, const char *key) {
    cJSON *item;
    
    if ((item = cJSON_GetObjectItem(root, key)) == nullptr || !cJSON_IsNumber(item)) return false;

    value = item->valueint;

    return true;
}

bool cJSON_GetIntValuesForArrayKey(int *&values, int &count, cJSON *root, const char *key) {
    cJSON *array;
    err_t err;
    int *items = nullptr;
    int size = 0;

    setErr((array = cJSON_GetObjectItem(root, key)) == nullptr);
    if (!err) setErr(!cJSON_IsArray(array));
    if (!err && (size = cJSON_GetArraySize(array)) > 0) {
        setErr((items = (int *) malloc(sizeof(int) * size)) == nullptr);
    }        
    for (int i = 0; !err && i < size; ++i) {
        cJSON *item = cJSON_GetArrayItem(array, i);
        setErr(item == nullptr || !cJSON_IsNumber(item));
        if (!err) items[i] = item->valueint;
    }
    if (!err) {
        count = size;
        values = items;
    } else {
        _free(items);
    }

    return !err;
}

bool cJSON_GetStringValueFromArray(const char **value, cJSON *array, int index) {
    if (!cJSON_IsArray(array)) return false;
    if (index < 0 || index >= cJSON_GetArraySize(array)) return false;

    cJSON *item;
    
    if ((item = cJSON_GetArrayItem(array, index)) == nullptr || !cJSON_IsString(item)) return false;

    *value = item->valuestring;

    return true;
}

bool cJSON_GetStringValueForKey(const char *&value, cJSON *root, const char *key) {
    cJSON *item;
    
    if ((item = cJSON_GetObjectItem(root, key)) == nullptr || !cJSON_IsString(item)) return false;

    value = item->valuestring;

    return true;
}

bool cJSON_GetStringValuesForArrayKey(const char **&values, int &count, cJSON *root, const char *key) {
    cJSON *array;
    err_t err;
    const char **items = nullptr;
    int size = 0;

    setErr((array = cJSON_GetObjectItem(root, key)) == nullptr);
    if (!err) setErr(!cJSON_IsArray(array));
    if (!err && (size = cJSON_GetArraySize(array)) > 0) {
        setErr((items = (const char **) malloc(sizeof(const char *) * size)) == nullptr);
    }        
    for (int i = 0; !err && i < size; ++i) {
        cJSON *item = cJSON_GetArrayItem(array, i);
        setErr(item == nullptr || !cJSON_IsString(item));
        if (!err) items[i] = item->valuestring;
    }
    if (!err) {
        count = size;
        values = items;
    } else {
        _free(items);
    }

    return !err;
}

err_t cJSON_GetLosslessInt64(const cJSON *item, int64_t &value) {
    double d = 0;
    err_t err = 0;

    if (!cJSON_IsNumber(item)) setErr(ENOTSUP);
    if (!err) {
        d = item->valuedouble;
        // ensure d is not NaN (d != d is true for NaN), is finite, and is within int64_t range
        if (d != d || d < DBL_LOSSLESS_INT_MIN || d > DBL_LOSSLESS_INT_MAX) setErr(ERANGE);
    }
    if (!err) {
        // ensure d is an integer (no fractional part)
        int64_t tmp = (int64_t)d;

        if (double(tmp) == d) value = tmp;
        else setErr(ERANGE);
    }

    return err;
}

err_t cJSON_GetLosslessUInt64(const cJSON *item, uint64_t &value) {
    double d = 0;
    err_t err = 0;

    if (!cJSON_IsNumber(item)) setErr(ENOTSUP);
    if (!err) {
        d = item->valuedouble;
        // ensure d is not NaN (d != d is true for NaN), is finite, and is within uint64_t range
        if (d != d || d < 0 || d > DBL_LOSSLESS_INT_MAX) setErr(ERANGE);
    }
    if (!err) {
        // ensure d is an integer (no fractional part)
        uint64_t tmp = (uint64_t)d;

        if (double(tmp) == d) value = tmp;
        else setErr(ERANGE);
    }

    return err;
}

err_t cJSON_UpsertCS(cJSON *object, const char *key, cJSON *node) {
    err_t err = 0;
    cJSON *item;

    if (!(object && key && *key && node)) setErr(EINVAL);

    if (!err) {
        if ((item = cJSON_GetObjectItemCaseSensitive(object, key))) {
            if (!cJSON_ReplaceItemInObjectCaseSensitive(object, key, node)) setErr(ENOMEM);
        } else {
            if (!cJSON_AddItemToObjectCS(object, (char *)key, node)) setErr(ENOMEM);
        }
    }

    return err;
}

cJSON *cJSONWithFormat(const char *format, ...) {
    va_list args;
    cJSON *root;

    va_start(args, format);
    root = vcJSONWithFormat(format, args);
    va_end(args);

    return root;
}

cJSON *vcJSONWithFormat(const char *format, va_list args) {
    char *string = nullptr;
    cJSON *root = nullptr;

    if (vasprintf(&string, format, args) >= 0) {
        if ((root = cJSON_Parse(string)) == nullptr) {
            _loge("cJSON_Parse('%s') failed", string);
        }
        free(string);
    }

    return root;
}
