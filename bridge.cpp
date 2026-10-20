// bridge.cpp - the C-callable functions that m4.c uses to reach the C++ ArtifactStore.
// m4.c is pure C and cannot see any C++ class, so every call crosses through here with
// plain C types only (char buffers, ints, doubles).
#include "bridge.h"
#include "m4.h"

#include <cstdio>

static ArtifactStore* g_store = nullptr;

void bridge_setStore(ArtifactStore* store) { g_store = store; }

// Copies text into a C buffer; returns false if it did not fit (so callers can skip the item)
static bool copyText(const std::string& s, char* out, size_t size)
{
    if (size == 0 || s.size() >= size) return false;
    std::snprintf(out, size, "%s", s.c_str());
    return true;
}

extern "C" int bridge_findArtifact(const char* id,
                                   char* name, size_t nameSize,
                                   char* location, size_t locationSize)
{
    if (!g_store) return 0;
    const Artifact* a = g_store->find(id);
    if (!a) return 0;
    std::snprintf(name, nameSize, "%s", a->getName().c_str());
    std::snprintf(location, locationSize, "%s", a->getLocation().c_str());
    return 1;
}

extern "C" int bridge_updateLocation(const char* id, const char* newLocation)
{
    if (!g_store) return 0;
    return g_store->updateLocation(id, newLocation) ? 1 : 0;
}

extern "C" int bridge_artifactCount(void)
{
    return g_store ? static_cast<int>(g_store->size()) : 0;
}

extern "C" int bridge_artifactAt(int index, char* id, size_t idSize,
                                 char* name, size_t nameSize, double* priority)
{
    if (!g_store || index < 0 || static_cast<size_t>(index) >= g_store->size()) return 0;
    const Artifact& a = *g_store->all()[static_cast<size_t>(index)];
    if (!copyText(a.getId(), id, idSize) || !copyText(a.getName(), name, nameSize)) return 0;
    *priority = a.calculatePriority();
    return 1;
}

extern "C" int bridge_restorationAdvice(const char* id, char* out, size_t outSize)
{
    if (!g_store) return 0;
    const Artifact* a = g_store->find(id);
    if (!a) return 0;
    // Only classes that also derive from Restorable (pottery, coins) can give advice
    const Restorable* r = dynamic_cast<const Restorable*>(a);
    if (!r) return 0;
    return copyText(r->restorationAdvice(), out, outSize) ? 1 : 0;
}
