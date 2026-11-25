#pragma once
// AFTERLIFE - SECTIONS (the two lanes)
// A section is a stretch of the song, in seconds on the DAW's timeline, where a lane does something different
// from its knobs: more reverb on one word, none on another, a delay throw on the end of a line.
//
//   REVERB lane   a section sets how much goes INTO the reverb (SEND) while it plays
//   DELAY lane    a section sets the SEND into the delay, and its own TIME and FEEDBACK
//
// Outside a section the lane follows the SEND knob. Sections fade in and out (FADE) so nothing clicks.
// They only control what goes INTO the effect: when a section ends, whatever already went in rings out.
// A delay section's echoes keep that section's TIME and FEEDBACK as they fade away (the THROW echo).
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <algorithm>
#include <array>
#include <vector>

namespace after
{
enum LaneId { kReverbLane = 0, kDelayLane = 1 };

// delay times in quarter notes (index -1 on a section = follow the TIME knob)
struct Division { const char* name; double quarters; };
inline const std::vector<Division>& divisions()
{
    static const std::vector<Division> d {
        { "1/64", 0.0625 }, { "1/32", 0.125 }, { "1/16T", 1.0 / 6.0 }, { "1/16", 0.25 }, { "1/16.", 0.375 },
        { "1/8T", 1.0 / 3.0 }, { "1/8", 0.5 }, { "1/8.", 0.75 }, { "1/4T", 2.0 / 3.0 }, { "1/4", 1.0 },
        { "1/4.", 1.5 }, { "1/2T", 4.0 / 3.0 }, { "1/2", 2.0 }, { "1/2.", 3.0 }, { "1/1", 4.0 },
    };
    return d;
}
inline juce::StringArray divisionNames() { juce::StringArray n; for (auto& d : divisions()) n.add (d.name); return n; }
inline int defaultDivision() { return 9; }   // 1/4

struct Section
{
    int id = 0;
    double start = 0, end = 0;   // seconds on the timeline
    float send = 1.0f;           // 0..1 into the effect while the section plays
    float feedback = 0.5f;       // delay lane: the throw's feedback
    int division = -1;           // delay lane: the throw's time (-1 = the TIME knob)
    float fadeMs = 25.0f;        // fade in at the start, fade out at the end

    double length() const { return end - start; }
    bool contains (double t) const { return t >= start && t < end; }
    // 0 outside, 1 inside, ramps at the edges
    float weightAt (double t) const
    {
        if (t < start || t >= end) return 0.0f;
        const double f = std::min ((double) fadeMs * 0.001, (end - start) * 0.5);
        if (f <= 1.0e-6) return 1.0f;
        return (float) std::min (1.0, std::min ((t - start) / f, (end - t) / f));
    }
};

using Lane = std::vector<Section>;   // sorted by start, never overlapping

struct SectionSet
{
    std::array<Lane, 2> lanes;
    int nextId = 1;

    static void sortLane (Lane& l) { std::sort (l.begin(), l.end(), [] (const Section& a, const Section& b) { return a.start < b.start; }); }

    const Section* find (int lane, int id) const
    {
        for (auto& s : lanes[(size_t) lane]) if (s.id == id) return &s;
        return nullptr;
    }
    // the section playing at t (nullptr between sections)
    const Section* at (int lane, double t) const
    {
        const auto& l = lanes[(size_t) lane];
        auto it = std::upper_bound (l.begin(), l.end(), t, [] (double v, const Section& s) { return v < s.start; });
        if (it == l.begin()) return nullptr;
        --it;
        return it->contains (t) ? &*it : nullptr;
    }
    // the last section that started at or before t (it may have ended): its throw is still ringing
    const Section* latest (int lane, double t) const
    {
        const auto& l = lanes[(size_t) lane];
        auto it = std::upper_bound (l.begin(), l.end(), t, [] (double v, const Section& s) { return v < s.start; });
        return it == l.begin() ? nullptr : &*(it - 1);
    }

    // where a section may live: between its neighbours
    juce::Range<double> roomFor (int lane, int id, double around) const
    {
        double lo = 0.0, hi = 1.0e9;
        for (auto& s : lanes[(size_t) lane])
        {
            if (s.id == id) continue;
            if (s.end <= around) lo = std::max (lo, s.end);
            else if (s.start >= around) hi = std::min (hi, s.start);
            else return { around, around };   // 'around' is inside another section
        }
        return { lo, hi };
    }

    // ---- saved with the song (the plugin state)
    juce::ValueTree toTree() const
    {
        juce::ValueTree t ("SECTIONS");
        t.setProperty ("nextId", nextId, nullptr);
        for (int ln = 0; ln < 2; ++ln)
            for (auto& s : lanes[(size_t) ln])
            {
                juce::ValueTree c ("S");
                c.setProperty ("lane", ln, nullptr);
                c.setProperty ("id", s.id, nullptr);
                c.setProperty ("start", s.start, nullptr);
                c.setProperty ("end", s.end, nullptr);
                c.setProperty ("send", s.send, nullptr);
                c.setProperty ("fb", s.feedback, nullptr);
                c.setProperty ("div", s.division, nullptr);
                c.setProperty ("fade", s.fadeMs, nullptr);
                t.appendChild (c, nullptr);
            }
        return t;
    }
    static SectionSet fromTree (const juce::ValueTree& t)
    {
        SectionSet set;
        if (! t.isValid()) return set;
        int maxId = 0;
        for (auto c : t)
        {
            if (! c.hasType ("S")) continue;
            const int ln = juce::jlimit (0, 1, (int) c.getProperty ("lane", 0));
            Section s;
            s.id = c.getProperty ("id", 0);
            s.start = c.getProperty ("start", 0.0);
            s.end = c.getProperty ("end", 0.0);
            s.send = juce::jlimit (0.0f, 1.0f, (float) c.getProperty ("send", 1.0));
            s.feedback = juce::jlimit (0.0f, 0.95f, (float) c.getProperty ("fb", 0.5));
            s.division = juce::jlimit (-1, (int) divisions().size() - 1, (int) c.getProperty ("div", -1));
            s.fadeMs = juce::jlimit (0.0f, 500.0f, (float) c.getProperty ("fade", 25.0));
            if (s.end - s.start < 0.001 || s.start < 0) continue;
            maxId = std::max (maxId, s.id);
            set.lanes[(size_t) ln].push_back (s);
        }
        for (auto& l : set.lanes) sortLane (l);
        set.nextId = std::max ((int) t.getProperty ("nextId", 1), maxId + 1);
        return set;
    }
};
} // namespace after
