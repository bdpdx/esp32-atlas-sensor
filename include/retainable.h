//
// Copyright © 2025 Brian Doyle. All rights reserved.
// MIT License
//

#pragma once

template<typename T>
concept Retainable = requires(T t) {
    { t.release() } -> std::same_as<size_t>;
    { t.retain() } -> std::same_as<size_t>;
};

typedef size_t (*ReleaseFunc)(void *);
typedef size_t (*RetainFunc)(void *);
