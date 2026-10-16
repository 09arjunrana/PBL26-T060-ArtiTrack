/* m4.h - interface between Module 4 (m4.c, pure C) and the C++ hub (m1.cpp)
 *
 *  - movementMenu()  is PROVIDED by m4.c  and CALLED by m1.cpp
 *  - bridge_*()      are PROVIDED by m1.cpp and CALLED by m4.c
 *    (they let C code read/update artifacts without seeing any C++ class)
 */
#ifndef M4_H
#define M4_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Module 4 entry point: shows the movement-tracking menu for the logged-in user */
void movementMenu(const char* user);

/* Look up an artifact by ID. Returns 1 and fills name/location if found, else 0. */
int bridge_findArtifact(const char* id,
                        char* name, size_t nameSize,
                        char* location, size_t locationSize);

/* Change an artifact's location and save it to artifacts.txt. Returns 1 on success. */
int bridge_updateLocation(const char* id, const char* newLocation);

#ifdef __cplusplus
}
#endif

#endif /* M4_H */