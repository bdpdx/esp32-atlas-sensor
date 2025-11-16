//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#include "named.h"

Named::~Named() {
    _free(name);
}

const char *Named::getName() const {
    return name ? name : "";
}

err_t Named::setName(const char *name) {
    err_t err = 0;
    char *newName;

    if (name && *name) {
        if ((newName = strdup(name)) == nullptr) setErr(ENOMEM);
    } else {
        newName = nullptr;
    }

    if (!err) {
        _free(this->name);
        this->name = newName;
    }

    return err;
}
