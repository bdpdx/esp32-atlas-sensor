//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

#include "common.h"

class Named {

public:

    Named() = default;
    Named(Named const &) = delete;
    virtual ~Named();

    const char *                getName() const;
    virtual err_t               setName(const char *name);

private:

    char *                      name = nullptr;

};
