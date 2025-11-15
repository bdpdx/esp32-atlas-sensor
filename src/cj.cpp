#include "cj.h"
#include "cJSONAdditions.h"

CJ::CJ(cJSON *object, bool takeOwnership) : isRootOwned(takeOwnership) {
    root = cJSON_IsObject(object) ? object : nullptr;
}

CJ::~CJ() {
    if (root && isRootOwned) cJSON_Delete(root);
}

err_t CJ::appendArray(const char *key, CJ &value) {
    err_t err = 0;

    if (value.root == nullptr) setErr(EINVAL);
    if (!err) setErr(appendArray(key, value.release()));

    return err;
}

err_t CJ::appendArray(const char *key, CJ &&value) {
    return appendArray(key, value);
}

err_t CJ::createNode(cJSON *&item, bool &created, CJ &value) {
    err_t err = 0;

    if (value.root == nullptr) setErr(EINVAL);
    if (!err) {
        created = true;
        item = value.release();
    }

    return err;
}

err_t CJ::createNode(cJSON *&item, bool &created, CJ &&value) {
    return createNode(item, created, value);
}

err_t CJ::createRoot() {
    err_t err = 0;

    if (!root) {
        if (!(root = cJSON_CreateObject())) setErr(ENOMEM);
        if (!err) isRootOwned = true;
    }

    return err;
}

err_t CJ::detachItem(CJ *parent, const char *key, cJSON *&item) {
    err_t err = 0;

    if (!(parent && key && *key)) setErr(EINVAL);

    if (!err) setErr(detachItem(parent->root, key, item));

    return err;
}

err_t CJ::detachItem(cJSON *parent, const char *key, cJSON *&item) {
    err_t err = 0;
    cJSON *node = nullptr;

    if (!(parent && key && *key)) setErr(EINVAL);

    if (!err) setErr(cJSON_Find(parent, key, node));
    if (!err && !cJSON_DetachItemViaPointer(parent, node)) setErr(EINVAL);
    if (!err) item = node;

    return err;
}

err_t CJ::detachItemFromArray(cJSON *array, int index, cJSON *&item) {
    err_t err = 0;
    cJSON *node = nullptr;

    if (!(array && cJSON_IsArray(array)) || index < 0 || index >= cJSON_GetArraySize(array)) setErr(EINVAL);

    if (!err && !(node = cJSON_DetachItemFromArray(array, index))) setErr(ENOENT);
    if (!err) item = node;

    return err;
}

err_t CJ::getArray(const char *key, cJSON *&array, int &count) const {
    cJSON *arr = nullptr;
    err_t err = 0;

    if (!(root && key && *key)) setErr(EINVAL);
    if (!err && (arr = cJSON_GetObjectItemCaseSensitive(root, key)) == nullptr) setErr(ENOENT);
    if (!err && !cJSON_IsArray(arr)) setErr(EINVAL);
    if (!err) {
        array = arr;
        count = cJSON_GetArraySize(arr);
    }

    return err;
}

err_t CJ::getArraySize(const char *key, int &count) const {
    cJSON *array = nullptr;

    return getArray(key, array, count);
}

err_t CJ::init(const char *jsonObjectText) {
    err_t err = 0;
    cJSON *object = nullptr;

    if (!(jsonObjectText && *jsonObjectText)) setErr(EINVAL);

    if (!err && !(object = cJSON_Parse(jsonObjectText))) setErr(EINVAL);
    if (!err && !cJSON_IsObject(object)) setErr(EINVAL);
    if (!err) setErr(setRoot(object, true));

    if (err && object) cJSON_Delete(object);

    return err;
}

cJSON *CJ::release() {
    cJSON *obj = root;

    root = nullptr;
    isRootOwned = false;

    return obj;
}

err_t CJ::setNull(const char *key) {
    err_t err = 0;
    cJSON *item = nullptr;

    if (!(key && *key)) setErr(EINVAL);
    if (!err && !root) {
        if (!(root = cJSON_CreateObject())) setErr(ENOMEM);
        if (!err) isRootOwned = true;
    }
    if (!err && !(item = cJSON_CreateNull())) setErr(ENOMEM);
    if (!err) setErr(cJSON_UpsertCS(root, key, item));

    return err;
}

err_t CJ::setObject(const char *key, cJSON *object) {
    bool created = false;
    err_t err = 0;
    cJSON *node = object;

    if (!(key && *key)) setErr(EINVAL);

    if (!err) setErr(createRoot());
    if (!err && !node) {
        if ((node = cJSON_CreateObject())) created = true;
        else setErr(ENOMEM);
    }
    if (!err) setErr(cJSON_UpsertCS(root, key, node));

    if (err && created && node) cJSON_Delete(node);

    return err;
}

err_t CJ::setObject(const char *key, CJ &&object) {
    bool created = false;
    err_t err = 0;
    cJSON *node = object.release();

    if (!(key && *key)) setErr(EINVAL);

    if (!err) setErr(createRoot());
    if (!err && !node) {
        if ((node = cJSON_CreateObject())) created = true;
        else setErr(ENOMEM);
    }
    if (!err) setErr(cJSON_UpsertCS(root, key, node));

    if (err && created && node) cJSON_Delete(node);

    return err;
}

err_t CJ::setRoot(cJSON *object, bool takeOwnership) {
    if (root && isRootOwned) cJSON_Delete(root);

    root = cJSON_IsObject(object) ? object : nullptr;
    isRootOwned = takeOwnership;

    return root ? 0 : EINVAL;
}

err_t CJ::toString(const char *&out, bool pretty) const {
    err_t err = 0;
    const char *string;

    if (!root) setErr(EINVAL);
    if (!err && !(string = pretty ? cJSON_Print(root) : cJSON_PrintUnformatted(root))) setErr(ENOMEM);
    if (!err) out = string;

    return err;
}
