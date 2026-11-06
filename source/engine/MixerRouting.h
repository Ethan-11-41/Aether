// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#pragma once

#include <vector>

namespace aether::routing
{

/** A connection in the mixer: channel `from` plays into channel `to`, through its output or
    through a send. Channels are numbered 0..n-1; the master is number n. */
struct Connection
{
    int from = 0, to = 0;
};

struct Result
{
    /** The channels (0..n-1) in an order where every channel comes after everything that plays
        into it, so one pass over the list mixes everything. */
    std::vector<int> order;

    /** For each connection: false if it was dropped because it would make a loop (only possible
        in a hand-edited file; the editor refuses loops). */
    std::vector<bool> kept;

    /** For each connection: whether sound passes through it, given which channels are soloed.
        With nothing soloed, everything passes. */
    std::vector<bool> audible;
};

/** Works out the processing order and solo for a mixer of `numChannels` channels (plus the
    master, number numChannels).

    Solo works "in place": you hear the soloed channels, everything they play into (so a soloed
    track is still heard through its bus and the master), and everything that plays into them
    (so soloing a bus lets you hear what is in it). Sound from anything else is cut. Soloing an
    effect bus (e.g. a reverb) lets you hear just the reverb: the tracks still send into it, but
    their own output is cut. */
Result compute (int numChannels, const std::vector<Connection>& connections, const std::vector<bool>& soloed);

} // namespace aether::routing
