#pragma once

#include "commonC.h"
#include "commonCPP.h"
#include "cJSONAdditions.h"
#include "log.h"

// First-arg extractor (covers lambdas/functors + plain function pointers)
template<class F> struct CJIterateArrayCallbackFirstArgTypeT;

template<class C, class R, class A0, class... A>
struct CJIterateArrayCallbackFirstArgTypeT<R (C::*)(A0, A...) const> {
    using type = std::decay_t<A0>;
};

template<class C, class R, class A0, class... A>
struct CJIterateArrayCallbackFirstArgTypeT<R (C::*)(A0, A...)> {
    using type = std::decay_t<A0>;
};

template<class R, class A0, class... A>
struct CJIterateArrayCallbackFirstArgTypeT<R (*)(A0, A...)> {
    using type = std::decay_t<A0>;
};

template<class F>
using CJIterateArrayCallbackFirstArg_t = typename CJIterateArrayCallbackFirstArgTypeT<decltype(&std::remove_reference_t<F>::operator())>::type;


template<typename F, typename T>
concept CJIterateArrayCallback = requires(F f, T t, int i, bool &b) {
    { f(t, i, b) } -> std::same_as<err_t>;
};

// MARK: - CJ

class CJ {

public:

    explicit CJ() = default;
    explicit CJ(cJSON *object, bool takeOwnership = false);
    ~CJ();

    // for numeric types int64_t and uint64_t, values exceeding 53 bits of precision will be encoded/decoded as strings.
    // otherwise lossless round-tripping of integers up to 2^53-1 is supported (values stored as double in cJSON number).

    // appendArray() appends value to the array at key, creating the array if needed.
    template<typename T>
    err_t                       appendArray(const char *key, T value);

    // Specializations for appending cJSON nodes from CJ instances.
    // value.root is released and transfered to the array @ key on success.
    err_t                       appendArray(const char *key, CJ &value);
    err_t                       appendArray(const char *key, CJ &&value);

    // createRoot() creates a root object if none exists.
    // generally it's not necessary to call this manually, but
    // can be useful if you need an empty root object.
    // if a root object already exists EALREADY is returned.
    err_t                       createRoot();

    // copyArray() allocates an array of T and copies the values from the JSON array into it.
    // if copyArray() returns 0 and count == 0 then values is set to nullptr
    // if copyArray() returns 0 and count > 0 then caller must free(values)
    template<typename T>
    err_t                       copyArray(const char *key, T *&values, int &count) const;

    // get() returns the value for key in out.
    // if out is a pointer type (e.g. const char * or cJSON *) the returned value is non-owning
    template<typename T>
    err_t                       get(const char *key, T &out, bool suppressItemNotFoundErrorLogging = false) const;

    // getArray() returns the array for key in array and its size in count.
    err_t                       getArray(const char *key, cJSON *&array, int &count) const;
    err_t                       getArraySize(const char *key, int &count) const;

    // get() returns the value for key or defaultValue if key is not present or an error occurs.
    template<typename T>
    std::decay_t<T>             getWithDefault(const char *key, T defaultValue) const;

    err_t                       init(const char *jsonObjectText);

    // callback is of the form err_t callback(const T &value, int index, bool &shouldContinue);
    // if shouldContinue is set to false iteration stops (iterateArray returns whatever error callback returned).
    // iterateArray() may fail prior to starting iteration if key is not found or is not an array.
    // If callback returns an error iteration stops and that error is returned by iterateArray().
    template<typename T, typename Callback>
    requires CJIterateArrayCallback<Callback, T>
    err_t                       iterateArray(const char *key, Callback &&callback) const;

    template<typename Callback>
    requires CJIterateArrayCallback<Callback, CJIterateArrayCallbackFirstArg_t<Callback>>
    err_t                       iterateArray(const char *key, Callback &&callback) const;

    // Iterate a raw cJSON array (public + static).
    // Explicit-T version:
    template<typename T, typename Callback>
    requires CJIterateArrayCallback<Callback, T>
    static err_t                iterateArray(const cJSON *array, Callback &&callback);

    // Type-inferred version (infers T from callback's first parameter):
    template<typename Callback>
    requires CJIterateArrayCallback<Callback, CJIterateArrayCallbackFirstArg_t<Callback>>
    static err_t                iterateArray(const cJSON *array, Callback &&callback);

    operator                    cJSON *() const { return root; }

    // release() releases ownership of the root object and returns it.
    // After calling release() the CJ object no longer has a root object.
    cJSON *                     release();

    // set() sets key to value. If value is a pointer type (e.g. const char * or cJSON *)
    // ownership of the value is transferred to the CJ object.
    template<typename T>        
    err_t                       set(const char *key, T value);

    template<typename T>
    err_t                       setArray(const char *key, const T *values, int count);

    // setNull() sets key to null.
    err_t                       setNull(const char *key);

    // sets or creates (if 'object' is null) a cJSON object at key.
    // On success, ownership transfers to this CJ.
    err_t                       setObject(const char *key, cJSON *object = nullptr);

    // sets or creates and sets an object at key from a CJ rvalue.
    // object.root is released and transfered to root on success.
    err_t                       setObject(const char *key, CJ &&object);

    // setRoot() sets the root object. If takeOwnership is true the CJ object takes ownership.
    // If the CJ object already has a root and it owns it, the existing root is deleted.
    err_t                       setRoot(cJSON *object, bool takeOwnership = false);

    // toString() returns a string representation of the JSON object.
    // Caller must cJSON_free() the returned string.
    err_t                       toString(const char *&out, bool pretty = false) const;

    static err_t                detachItem(CJ *parent, const char *key, cJSON *&item);
    static err_t                detachItem(cJSON *parent, const char *key, cJSON *&item);
    static err_t                detachItemFromArray(cJSON *array, int index, cJSON *&item);

private:

    template<typename T>
    err_t                       convertNode(const cJSON *node, T &out) const;

    template<typename T>
    err_t                       createNode(cJSON *&item, bool &created, T value);

    // Shared converter used by both static and non-static sites (same logic, no state).
    template<typename T>
    static err_t                convertNodeValue(const cJSON *node, T &out);

    // Specializations for creating cJSON nodes from CJ instances.
    // value.root is released and transfered to item on success.
    err_t                       createNode(cJSON *&item, bool &created, CJ &value);
    err_t                       createNode(cJSON *&item, bool &created, CJ &&value);

    bool                        isRootOwned = false;                
    cJSON *                     root = nullptr;

};

template<typename T>
err_t CJ::appendArray(const char *key, T value) {
    cJSON *array = nullptr;
    bool created = false;
    err_t err = 0;
    cJSON *item = nullptr;

    if (!(key && *key)) setErr(EINVAL);

    if (!err) setErr(createRoot());
    if (!err && !(array = cJSON_GetObjectItemCaseSensitive(root, key))) {
        if (!(array = cJSON_CreateArray())) setErr(ENOMEM);
        if (!err && !cJSON_AddItemToObject(root, key, array)) setErr(ENOMEM);
        if (err) cJSON_Delete(array);
    }
    if (!err && !cJSON_IsArray(array)) setErr(EINVAL);
    if (!err) setErr(createNode(item, created, value));
    if (!err && !cJSON_AddItemToArray(array, item)) setErr(ENOMEM);

    if (err && created && item) cJSON_Delete(item);

    return err;
}

template<typename T>
err_t CJ::convertNode(const cJSON *node, T &out) const {
    return convertNodeValue(node, out);
}

template<typename T>
err_t CJ::convertNodeValue(const cJSON *node, T &out) {
    err_t err = 0;

    if (!node) setErr(EINVAL);
    if (!err) {
        // Raw cJSON* passthrough
        if constexpr (
            std::is_pointer_v<T> &&
            std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>, cJSON>)
        {
            out = T(node);
        }
        // Booleans: accept true/false, 0/1, "true"/"false"/"0"/"1"
        else if constexpr (std::is_same_v<T, bool>) {
            if (cJSON_IsBool(node)) out = cJSON_IsTrue(node);
            else if (cJSON_IsNumber(node)) out = node->valuedouble != 0;
            else if (cJSON_IsString(node) && node->valuestring) {
                const char *s = node->valuestring;

                if (!strcasecmp(s, "true") || !strcmp(s, "1")) out = true;
                else if (!strcasecmp(s, "false") || !strcmp(s, "0")) out = false;
                else setErr(EINVAL);
            } else setErr(EINVAL);
        }
        // Strings
        else if constexpr (std::is_same_v<T, const char *>) {
            if (!(cJSON_IsString(node) && node->valuestring)) setErr(EINVAL);
            if (!err) out = node->valuestring;
        }
        // Double
        else if constexpr (std::is_same_v<T, double>) {
            if (cJSON_IsNumber(node)) out = node->valuedouble;
            else if (cJSON_IsString(node) && node->valuestring) {
                char *end = nullptr;
                double v = strtod(node->valuestring, &end);

                if (!(end && *end == '\0')) setErr(EINVAL);
                if (!err) out = v;
            } else setErr(EINVAL);
        }
        // Signed built-ins: int, long, long long
        else if constexpr (
            std::is_same_v<T, int> ||
            std::is_same_v<T, long> ||
            std::is_same_v<T, long long>)
        {
            long long v = 0;

            if (cJSON_IsNumber(node)) {
                int64_t t = 0;
                setErr(cJSON_GetLosslessInt64(node, t));
                v = (long long) t;
            } else if (cJSON_IsString(node) && node->valuestring) {
                char *end = nullptr;
                v = strtoll(node->valuestring, &end, 10);
                if (!(end && *end == '\0')) setErr(EINVAL);
            } else setErr(EINVAL);
            
            // Range check against the target type width
            if (!err) {
                if constexpr (sizeof(T) < sizeof(long long)) {
                    long long lo = (long long) std::numeric_limits<T>::min();
                    long long hi = (long long) std::numeric_limits<T>::max();

                    if (v < lo || v > hi) setErr(ERANGE);
                }
            }
            if (!err) out = T(v);
        }
        // Unsigned built-ins: unsigned, unsigned long, unsigned long long
        else if constexpr (
            std::is_same_v<T, unsigned int> ||
            std::is_same_v<T, unsigned long> ||
            std::is_same_v<T, unsigned long long>)
        {
            unsigned long long v = 0;

            if (cJSON_IsNumber(node)) {
                uint64_t t = 0;
                setErr(cJSON_GetLosslessUInt64(node, t));
                v = (unsigned long long) t;
            } else if (cJSON_IsString(node) && node->valuestring) {
                char *end = nullptr;
                v = strtoull(node->valuestring, &end, 10);
                if (!(end && *end == '\0')) setErr(EINVAL);
            } else setErr(EINVAL);

            if (!err) {
                if constexpr (sizeof(T) < sizeof(unsigned long long)) {
                    unsigned long long hi = (unsigned long long) std::numeric_limits<T>::max();

                    if (v > hi) setErr(ERANGE);
                }
            }
            if (!err) out = T(v);
        }
        else {
            static_assert(!std::is_same_v<T,T>, "Unsupported T in CJ::convertNode()");
        }
    }

    return err;
}

template<typename T>
err_t CJ::createNode(cJSON *&item, bool &created, T value) {
    bool createdNode = true;
    err_t err = 0;
    cJSON *node = nullptr;

    // Booleans
    if constexpr (std::is_same_v<T, bool>) {
        if (!(node = cJSON_CreateBool(value ? 1 : 0))) setErr(ENOMEM);
    }
    // Raw cJSON node: pass through (ownership transfers only if add/upsert succeeds)
    else if constexpr (std::is_same_v<T, cJSON *>) {
        createdNode = false;
        node = value;
    }
    // Strings
    else if constexpr (
        std::is_same_v<T, char *> ||
        std::is_same_v<T, const char *>)
    {
        if (!(node = cJSON_CreateString(value ? value : ""))) setErr(ENOMEM);
    }
    // Double
    else if constexpr (std::is_same_v<T, double>) {
        if (!(node = cJSON_CreateNumber(value))) setErr(ENOMEM);
    }
    // Signed built-ins
    else if constexpr (
        std::is_same_v<T, int> ||
        std::is_same_v<T, long> ||
        std::is_same_v<T, long long>)
    {
        long long v = (long long) value;

        if (v >= (long long) DBL_LOSSLESS_INT_MIN && v <= (long long) DBL_LOSSLESS_INT_MAX) {
            if (!(node = cJSON_CreateNumber(double(v)))) setErr(ENOMEM);
        } else {
            char buffer[32];
            int n = snprintf(buffer, sizeof(buffer), "%lld", v);
            if (n < 0 || n >= (int) sizeof(buffer)) setErr(ERANGE);
            if (!err && !(node = cJSON_CreateString(buffer))) setErr(ENOMEM);
        }
    }
    // Unsigned built-ins
    else if constexpr (
        std::is_same_v<T, unsigned int> ||
        std::is_same_v<T, unsigned long> ||
        std::is_same_v<T, unsigned long long>)
    {
        unsigned long long v = (unsigned long long) value;

        if (v <= (unsigned long long) DBL_LOSSLESS_INT_MAX) {
            if (!(node = cJSON_CreateNumber(double(v)))) setErr(ENOMEM);
        } else {
            char buffer[32];
            int n = snprintf(buffer, sizeof(buffer), "%llu", v);
            if (n < 0 || n >= (int) sizeof(buffer)) setErr(ERANGE);
            if (!err && !(node = cJSON_CreateString(buffer))) setErr(ENOMEM);
        }
    } else {
        static_assert(!std::is_same_v<T,T>, "Unsupported T in CJ::createNode()");
    }

    if (!err) {
        created = createdNode;
        item = node;
    }

    if (err && createdNode && node) cJSON_Delete(node);

    return err;
}

template<typename T>
err_t CJ::copyArray(const char *key, T *&values, int &count) const {
    cJSON *array = nullptr;
    T *buffer = nullptr;
    err_t err = 0;
    int n = 0;

    if (!(root && key && *key)) setErr(EINVAL);
    if (!err) setErr(getArray(key, array, n));
    if (!err) {
        if (n == 0) {
            count = n;
            values = nullptr;
            return err;
        }

        if (!(buffer = static_cast<T *>(malloc(sizeof(T) * n)))) setErr(ENOMEM);
    }
    if (!err) {
        int i = 0;
        
        setErr(iterateArray<T>(array, [&](const T &value, int, bool &) -> err_t {
            buffer[i++] = value;
            return 0;
        }));
    }
    if (!err) {
        count = n;
        values = buffer;
    } else {
        _free(buffer);
    }

    return err;
}

template<typename T>
err_t CJ::get(const char *key, T &out, bool suppressItemNotFoundErrorLogging) const {
    err_t err = 0;
    cJSON *node = nullptr;

    if (!(root && key && *key)) setErr(EINVAL);
    if (!err && !(node = cJSON_GetObjectItemCaseSensitive(root, key))) {
        if (suppressItemNotFoundErrorLogging) err = ENOENT;
        else setErr(ENOENT);
    }
    if (!err) setErr(convertNode(node, out));

    return err;
}

template<typename T>
std::decay_t<T> CJ::getWithDefault(const char *key, T defaultValue) const {
    using U = std::decay_t<T>;

    U tmp{};

    return this->get<U>(key, tmp, true) ? static_cast<U>(defaultValue) : tmp;
}

template<typename T, typename Callback>
requires CJIterateArrayCallback<Callback, T>
err_t CJ::iterateArray(const char *key, Callback &&callback) const {
    cJSON *array = nullptr;
    err_t err = 0;
    int n = 0;

    if (!(root && key && *key)) setErr(EINVAL);
    if (!err) setErr(getArray(key, array, n));
    if (!err) setErr(CJ::iterateArray<T>(array, std::forward<Callback>(callback)));
    
    return err;
}

template<typename Callback>
requires CJIterateArrayCallback<Callback, CJIterateArrayCallbackFirstArg_t<Callback>>
err_t CJ::iterateArray(const char *key, Callback &&callback) const {
    using T = CJIterateArrayCallbackFirstArg_t<Callback>;

    return iterateArray<T>(key, std::forward<Callback>(callback));
}

// static, explicit-T, computes count via cJSON
template<typename T, typename Callback>
requires CJIterateArrayCallback<Callback, T>
err_t CJ::iterateArray(const cJSON *array, Callback &&callback) {
    int count = 0;
    err_t err = 0;
    bool shouldContinue = true;

    if (!(array && cJSON_IsArray(array))) setErr(EINVAL);

    if (!err) count = cJSON_GetArraySize(array);

    for (int i = 0; !err && i < count; ++i) {
        cJSON *item = nullptr;
        T value{};

        if (!(item = cJSON_GetArrayItem(array, i))) setErr(EINVAL);
        if (!err) setErr(convertNodeValue(item, value));
        if (!err) setErr(callback(value, i, shouldContinue));

        if (!shouldContinue) break;
    }

    return err;
}

template<typename Callback>
requires CJIterateArrayCallback<Callback, CJIterateArrayCallbackFirstArg_t<Callback>>
err_t CJ::iterateArray(const cJSON *array, Callback &&callback) {
    using T = CJIterateArrayCallbackFirstArg_t<Callback>;

    return iterateArray<T>(array, std::forward<Callback>(callback));
}

template<typename T>
err_t CJ::set(const char *key, T value) {
    bool created = false;
    err_t err = 0;
    cJSON *item = nullptr;

    if (!(key && *key)) setErr(EINVAL);

    if (!err) setErr(createRoot());
    if (!err) setErr(createNode(item, created, value));
    if (!err) setErr(cJSON_UpsertCS(root, key, item));

    if (err && created && item) cJSON_Delete(item);

    return err;
}

template<typename T>
err_t CJ::setArray(const char *key, const T *values, int count) {
    cJSON *array = nullptr;
    err_t err = 0;

    if (!(key && *key) || count < 0 || (count > 0 && values == nullptr)) setErr(EINVAL);

    if (!err) setErr(createRoot());
    if (!err && !(array = cJSON_CreateArray())) setErr(ENOMEM);

    for (int i = 0; !err && i < count; ++i) {
        bool created = false;
        cJSON *item = nullptr;

        setErr(createNode(item, created, values[i]));
        if (!err && !cJSON_AddItemToArray(array, item)) setErr(ENOMEM);

        if (err && created && item) cJSON_Delete(item);
    }

    if (!err) setErr(cJSON_UpsertCS(root, key, array));
    
    if (err) { cJSON_Delete(array); }

    return err;
}
