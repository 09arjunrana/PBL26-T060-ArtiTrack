// bridge.h - connects the pure-C movement module (m4.c) to the C++ ArtifactStore
// The C functions themselves are declared in m4.h; this header only lets the hub
// say which store they should work on.
#pragma once
#include "m1.h"

// While a store is set, m4.c's bridge_* calls read and update it. Pass nullptr to detach.
void bridge_setStore(ArtifactStore* store);
