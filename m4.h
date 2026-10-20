/* m4.h - interface between the movement module (m4.c, PURE C) and the C++ side
 *
 *  - movementMenu()  is PROVIDED by m4.c    and CALLED by main.cpp
 *  - bridge_*()      are PROVIDED by bridge.cpp and CALLED by m4.c
 *    (they let the C code read/update artifacts without seeing any C++ class)
 */
#ifndef M4_H
#define M4_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Movement & restoration menu for the logged-in user: move, history, full log,
 * undo (stack), restoration priority queue (binary heap). */
void movementMenu(const char* user);

/* Look up an artifact by ID. Returns 1 and fills name/location if found, else 0. */
int bridge_findArtifact(const char* id,
                        char* name, size_t nameSize,
                        char* location, size_t locationSize);

/* Change an artifact's location and save it to artifacts.txt. Returns 1 on success. */
int bridge_updateLocation(const char* id, const char* newLocation);

/* How many artifacts are loaded. */
int bridge_artifactCount(void);

/* Artifact number `index` (0 .. count-1): its ID, name and restoration priority score.
 * Returns 1 on success, 0 if index is out of range or a buffer is too small. */
int bridge_artifactAt(int index, char* id, size_t idSize,
                      char* name, size_t nameSize, double* priority);

/* Conservation advice for artifacts that can be restored (pottery, coins).
 * Returns 1 and fills `out` if the artifact has advice, else 0. */
int bridge_restorationAdvice(const char* id, char* out, size_t outSize);

#ifdef __cplusplus
}
#endif

#endif /* M4_H */
