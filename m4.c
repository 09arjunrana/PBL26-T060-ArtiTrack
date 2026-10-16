/* m4.c - Module 4: Artifact movement tracking  (PURE C - compile with gcc)
 *
 * Data structure: singly linked list (head + tail pointers) of movement records.
 * Every move is also appended to movements.txt so history survives restarts.
 *
 * Build:  gcc -std=c11 -Wall -Wextra -c m4.c -o m4.o
 */
#include "m4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define ID_LEN      64
#define NAME_LEN    128
#define LOC_LEN     128
#define USER_LEN    64
#define REASON_LEN  256
#define TS_LEN      32
#define LINE_LEN    1024
#define LOG_FILE    "movements.txt"

/* ===================== Linked list of movements ===================== */
typedef struct MovementNode {
    char artifactId[ID_LEN];
    char from[LOC_LEN];
    char to[LOC_LEN];
    char movedBy[USER_LEN];
    char reason[REASON_LEN];
    char timestamp[TS_LEN];
    struct MovementNode* next;
} MovementNode;

typedef struct {
    MovementNode* head;
    MovementNode* tail;
    int count;
} MovementList;

static void list_init(MovementList* l)
{
    l->head = l->tail = NULL;
    l->count = 0;
}

static void list_free(MovementList* l)
{
    MovementNode* cur = l->head;
    while (cur != NULL) {
        MovementNode* next = cur->next;
        free(cur);
        cur = next;
    }
    list_init(l);
}

/* Adds a record at the END of the list (oldest first). Returns 0 on failure. */
static int list_append(MovementList* l, const char* id, const char* from,
                       const char* to, const char* by, const char* reason,
                       const char* ts)
{
    if (strlen(id) >= ID_LEN || strlen(from) >= LOC_LEN || strlen(to) >= LOC_LEN ||
        strlen(by) >= USER_LEN || strlen(reason) >= REASON_LEN || strlen(ts) >= TS_LEN)
        return 0;                                   /* field too long for the node */

    MovementNode* n = (MovementNode*)malloc(sizeof(MovementNode));
    if (n == NULL) return 0;

    strcpy(n->artifactId, id);
    strcpy(n->from, from);
    strcpy(n->to, to);
    strcpy(n->movedBy, by);
    strcpy(n->reason, reason);
    strcpy(n->timestamp, ts);
    n->next = NULL;

    if (l->tail == NULL) l->head = n;               /* empty list */
    else                 l->tail->next = n;
    l->tail = n;
    l->count++;
    return 1;
}

static int list_countFor(const MovementList* l, const char* id)
{
    int n = 0;
    const MovementNode* cur;
    for (cur = l->head; cur != NULL; cur = cur->next)
        if (strcmp(cur->artifactId, id) == 0) n++;
    return n;
}

static void printNode(const MovementNode* m)
{
    printf("  [%s] %s: %s  ->  %s\n      By: %s | Reason: %s\n",
           m->timestamp, m->artifactId, m->from, m->to, m->movedBy, m->reason);
}

static void list_printHistory(const MovementList* l, const char* id)
{
    int n = list_countFor(l, id);
    const MovementNode* cur;
    if (n == 0) {
        printf("No movements recorded for %s yet.\n", id);
        return;
    }
    printf("\n=== Movement history for %s (%d move(s)) ===\n", id, n);
    for (cur = l->head; cur != NULL; cur = cur->next)
        if (strcmp(cur->artifactId, id) == 0) printNode(cur);
}

static void list_printAll(const MovementList* l)
{
    const MovementNode* cur;
    if (l->count == 0) {
        printf("No movements recorded yet.\n");
        return;
    }
    printf("\n=== Full movement log (%d move(s)) ===\n", l->count);
    for (cur = l->head; cur != NULL; cur = cur->next) printNode(cur);
}

/* ===================== movements.txt (file handling) ===================== */
/* Line format:  MOVE|id|from|to|user|reason|timestamp */

static void stripNewline(char* s)
{
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) s[--len] = '\0';
}

/* Splits a line IN PLACE at every '|'. Returns the number of fields found. */
static int splitPipes(char* line, char* fields[], int maxFields)
{
    int n = 0;
    char* p = line;
    fields[n++] = p;
    while (*p != '\0') {
        if (*p == '|') {
            *p = '\0';
            if (n == maxFields) return maxFields + 1;   /* too many fields */
            fields[n++] = p + 1;
        }
        p++;
    }
    return n;
}

static void log_load(MovementList* l)
{
    FILE* fp = fopen(LOG_FILE, "r");
    char line[LINE_LEN];
    int skipped = 0;

    if (fp == NULL) return;                         /* first run: nothing logged yet */

    while (fgets(line, sizeof(line), fp) != NULL) {
        char* f[8];
        stripNewline(line);
        if (line[0] == '\0') continue;

        if (splitPipes(line, f, 7) == 7 && strcmp(f[0], "MOVE") == 0 &&
            list_append(l, f[1], f[2], f[3], f[4], f[5], f[6]))
            continue;
        skipped++;
    }
    fclose(fp);

    if (skipped > 0)
        printf("Warning: skipped %d unreadable line(s) in %s.\n", skipped, LOG_FILE);
}

/* Saves one record to disk FIRST; only then is it added to the list. */
static int log_record(MovementList* l, const char* id, const char* from,
                      const char* to, const char* by, const char* reason,
                      const char* ts)
{
    FILE* fp;
    int ok;

    if (strlen(id) >= ID_LEN || strlen(from) >= LOC_LEN || strlen(to) >= LOC_LEN ||
        strlen(by) >= USER_LEN || strlen(reason) >= REASON_LEN)
        return 0;

    fp = fopen(LOG_FILE, "a");
    if (fp == NULL) return 0;

    ok = fprintf(fp, "MOVE|%s|%s|%s|%s|%s|%s\n", id, from, to, by, reason, ts) > 0;
    ok = (fflush(fp) == 0) && ok;
    ok = (ferror(fp) == 0) && ok;
    if (fclose(fp) != 0) ok = 0;
    if (!ok) return 0;

    return list_append(l, id, from, to, by, reason, ts);
}

/* ===================== Input helpers (C only, no cin) ===================== */
/* Read one line. Must be non-empty, must fit in buf, must not contain '|'
 * (same rules as readLine() in m0.cpp). Exits cleanly if input is closed. */
static void readLineC(const char* prompt, char* buf, size_t size)
{
    for (;;) {
        size_t len;
        printf("%s", prompt);
        fflush(stdout);

        if (fgets(buf, (int)size, stdin) == NULL) {
            printf("\nInput closed. Exiting.\n");
            exit(0);
        }

        len = strlen(buf);
        if (len > 0 && buf[len - 1] != '\n' && !feof(stdin)) {
            int c;                                  /* line was too long: throw away the rest */
            while ((c = getchar()) != '\n' && c != EOF) { }
            printf("Input too long (max %u characters).\n", (unsigned)(size - 1));
            continue;
        }

        stripNewline(buf);
        if (buf[0] == '\0')
            printf("This field cannot be empty.\n");
        else if (strchr(buf, '|') != NULL)
            printf("The '|' character is not allowed.\n");
        else
            return;
    }
}

static int readIntC(const char* prompt, int minVal, int maxVal)
{
    char buf[64];
    for (;;) {
        char* end;
        long v;
        printf("%s", prompt);
        fflush(stdout);

        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            printf("\nInput closed. Exiting.\n");
            exit(0);
        }
        if (strchr(buf, '\n') == NULL && !feof(stdin)) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF) { }
            printf("Invalid input. Please enter a number.\n");
            continue;
        }

        v = strtol(buf, &end, 10);
        while (*end == ' ' || *end == '\t') end++;
        if (end == buf || (*end != '\n' && *end != '\r' && *end != '\0')) {
            printf("Invalid input. Please enter a number.\n");
            continue;
        }
        if (v < minVal || v > maxVal) {
            printf("Enter a number between %d and %d.\n", minVal, maxVal);
            continue;
        }
        return (int)v;
    }
}

/* ===================== Small helpers ===================== */
static void currentTimestamp(char* out, size_t size)
{
    time_t t = time(NULL);
    struct tm* lt = localtime(&t);
    if (lt == NULL || strftime(out, size, "%Y-%m-%d %H:%M:%S", lt) == 0)
        snprintf(out, size, "unknown-time");
}

/* Case-insensitive compare that ignores leading/trailing spaces. 1 = same. */
static int sameText(const char* a, const char* b)
{
    const char* ea;
    const char* eb;
    while (*a == ' ' || *a == '\t') a++;
    while (*b == ' ' || *b == '\t') b++;
    ea = a + strlen(a);
    eb = b + strlen(b);
    while (ea > a && (ea[-1] == ' ' || ea[-1] == '\t')) ea--;
    while (eb > b && (eb[-1] == ' ' || eb[-1] == '\t')) eb--;

    if ((ea - a) != (eb - b)) return 0;
    while (a < ea) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return 1;
}

/* ===================== Menu actions ===================== */
static void moveArtifact(MovementList* log, const char* user)
{
    char id[ID_LEN], name[NAME_LEN], oldLoc[LOC_LEN];
    char newLoc[LOC_LEN], reason[REASON_LEN], ts[TS_LEN];

    printf("\n--- Move Artifact ---\n");
    readLineC("Enter Artifact ID to move: ", id, sizeof(id));

    if (!bridge_findArtifact(id, name, sizeof(name), oldLoc, sizeof(oldLoc))) {
        printf("Artifact not found.\n");
        return;
    }
    printf("Current location of %s (%s): %s\n", name, id, oldLoc);

    readLineC("Enter New Location: ", newLoc, sizeof(newLoc));
    if (sameText(newLoc, oldLoc)) {
        printf("The artifact is already at that location. No movement recorded.\n");
        return;
    }
    readLineC("Enter Reason for movement: ", reason, sizeof(reason));

    /* 1) change the artifact's location (saved to artifacts.txt by the C++ side) */
    if (!bridge_updateLocation(id, newLoc)) {
        printf("ERROR: could not update the artifact save file. Nothing was changed.\n");
        return;
    }

    /* 2) log the movement; if that fails, undo step 1 so both files stay in sync */
    currentTimestamp(ts, sizeof(ts));
    if (!log_record(log, id, oldLoc, newLoc, user, reason, ts)) {
        if (bridge_updateLocation(id, oldLoc))
            printf("ERROR: could not write the movement log. Move cancelled.\n");
        else
            printf("ERROR: could not write the movement log AND could not undo the move.\n"
                   "       %s is now at \"%s\" but the move is not logged.\n", id, newLoc);
        return;
    }

    printf("Moved %s (%s) from \"%s\" to \"%s\". Movement logged.\n",
           name, id, oldLoc, newLoc);
}

static void viewHistory(const MovementList* log)
{
    char id[ID_LEN], name[NAME_LEN], loc[LOC_LEN];

    readLineC("Enter Artifact ID: ", id, sizeof(id));
    if (!bridge_findArtifact(id, name, sizeof(name), loc, sizeof(loc))) {
        printf("Artifact not found.\n");
        return;
    }
    printf("\nCurrent location of %s (%s): %s\n", name, id, loc);
    list_printHistory(log, id);
}

/* ===================== Module 4 entry point ===================== */
void movementMenu(const char* user)
{
    MovementList log;
    int choice;

    list_init(&log);
    log_load(&log);

    do {
        printf("\n===== Movement Tracking =====\n"
               "1. Move an Artifact\n"
               "2. View Movement History of an Artifact\n"
               "3. View Full Movement Log\n"
               "4. Back to Artifact Menu\n");
        choice = readIntC("Enter your choice: ", 1, 4);

        switch (choice) {
            case 1: moveArtifact(&log, user); break;
            case 2: viewHistory(&log);        break;
            case 3: list_printAll(&log);      break;
            case 4: break;
        }
    } while (choice != 4);

    list_free(&log);                                /* give all nodes back */
}