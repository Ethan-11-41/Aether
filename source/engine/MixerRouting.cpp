// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "MixerRouting.h"

#include <algorithm>
#include <deque>

namespace aether::routing
{

Result compute (int numChannels, const std::vector<Connection>& connections, const std::vector<bool>& soloed)
{
    const auto n = std::max (0, numChannels);
    const auto master = n;
    Result result;
    result.kept.assign (connections.size(), true);
    result.audible.assign (connections.size(), true);

    auto valid = [&] (const Connection& c)
    {
        return c.from >= 0 && c.from < n && c.to >= 0 && c.to <= master && c.from != c.to;
    };

    for (size_t i = 0; i < connections.size(); ++i)
        result.kept[i] = valid (connections[i]);

    // Processing order (Kahn's algorithm). Channels that nothing plays into go first; a channel
    // is ready once everything playing into it is done. Keeping the original order among ready
    // channels keeps the mix the same from one edit to the next.
    std::vector<int> waitingFor ((size_t) n, 0);

    for (size_t i = 0; i < connections.size(); ++i)
        if (result.kept[i] && connections[i].to != master)
            ++waitingFor[(size_t) connections[i].to];

    std::vector<bool> done ((size_t) n, false);

    while ((int) result.order.size() < n)
    {
        int next = -1;

        for (int c = 0; c < n && next < 0; ++c)
            if (! done[(size_t) c] && waitingFor[(size_t) c] == 0)
                next = c;

        if (next < 0)
        {
            // Everything left is waiting on a loop: drop a connection into the first one, and go on.
            for (int c = 0; c < n && next < 0; ++c)
                if (! done[(size_t) c])
                    next = c;

            for (size_t i = 0; i < connections.size(); ++i)
            {
                if (result.kept[i] && connections[i].to == next && ! done[(size_t) connections[i].from])
                {
                    result.kept[i] = false;
                    --waitingFor[(size_t) next];
                }
            }

            continue;
        }

        done[(size_t) next] = true;
        result.order.push_back (next);

        for (size_t i = 0; i < connections.size(); ++i)
            if (result.kept[i] && connections[i].from == next && connections[i].to != master)
                --waitingFor[(size_t) connections[i].to];
    }

    // Solo.
    std::vector<bool> isSoloed ((size_t) n + 1, false);
    bool anySolo = false;

    for (int c = 0; c < n && c < (int) soloed.size(); ++c)
        if (soloed[(size_t) c])
            isSoloed[(size_t) c] = anySolo = true;

    if (! anySolo)
        return result;

    // What the soloed channels play into (downstream), and what plays into them (upstream).
    auto spread = [&] (bool forwards)
    {
        std::vector<bool> reached ((size_t) n + 1, false);
        std::deque<int> queue;

        for (int c = 0; c < n; ++c)
            if (isSoloed[(size_t) c])
                queue.push_back (c);

        while (! queue.empty())
        {
            const auto current = queue.front();
            queue.pop_front();

            for (size_t i = 0; i < connections.size(); ++i)
            {
                if (! result.kept[i])
                    continue;

                const auto& c = connections[i];
                const auto from = forwards ? c.from : c.to;
                const auto to = forwards ? c.to : c.from;

                if (from == current && ! reached[(size_t) to])
                {
                    reached[(size_t) to] = true;
                    queue.push_back (to);
                }
            }
        }

        return reached;
    };

    const auto downstream = spread (true);
    const auto upstream = spread (false);

    for (size_t i = 0; i < connections.size(); ++i)
    {
        const auto& c = connections[i];

        if (! result.kept[i])
        {
            result.audible[i] = false;
            continue;
        }

        const auto from = (size_t) c.from, to = (size_t) c.to;

        // Sound flowing on from a soloed channel, or flowing into one.
        const auto fromSoloSide = (isSoloed[from] || downstream[from]) && (downstream[to] || c.to == master);
        const auto intoSolo = upstream[from] && (isSoloed[to] || upstream[to]);
        result.audible[i] = fromSoloSide || intoSolo;
    }

    return result;
}

} // namespace aether::routing
