#ifndef GAME_GAME_STATE_H
#define GAME_GAME_STATE_H

#include "core/my_c_runtime.h"

// NOTE(Liam): For the shit you need to access anywhere, realistically, everything would go in it lol.
// Sidenote: This is the much simpler, more scalable way to program games (and everything, e.g. my renderer is doing this).
// It's so simple but then people preached design patterns and ruined everyones ability to simplify the right problems in the right way.
// No passing data through object nests, no struggling to get the information from one system to another.
// I'm dying with the rest of code just to count the number of zombies killed so I can show it in the UI and
// track my runs.
// blah blah blah

typedef struct InternalGameState
{
    uint32_t num_zombies_killed;
}
InternalGameState;

extern InternalGameState gamestate;

#endif  // GAME_GAME_STATE_H
