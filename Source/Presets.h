#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <utility>

namespace after
{
struct Preset { juce::String name; std::vector<std::pair<juce::String, float>> values; };

// Presets set the knobs only: your sections (the lanes) belong to the song and are never touched by a preset.
// ids: rv_on, rv_space (index), rv_send (%), rv_decay (s), rv_predelay (ms), rv_lowcut / rv_highcut (Hz), rv_width (%),
//      rv_duck (%), rv_level (dB), dl_on, dl_sync, dl_div (index), dl_ms, dl_feedback (%), dl_mode (index),
//      dl_tone / dl_lowcut (Hz), dl_dirt (%), dl_duck (%), dl_send (%), dl_level (dB), dry (%), output (dB), lanes
// spaces: 0 CHAMBER 1 CRYPT 2 CATACOMB 3 CATHEDRAL 4 CISTERN 5 IRON PLATE 6 MAUSOLEUM 7 THE VOID
// divisions: 3 1/16  6 1/8  7 1/8.  9 1/4  10 1/4.  12 1/2      modes: 0 STEREO 1 PING-PONG 2 MONO
inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> p {
        { "Init (Chamber + Quarter)",  { } },
        { "Vocal: Iron Plate",         { { "rv_space", 5 }, { "rv_decay", 1.8f }, { "rv_predelay", 30 }, { "rv_send", 25 }, { "rv_lowcut", 250 }, { "rv_highcut", 8000 },
                                         { "dl_send", 0 } } },
        { "Vocal: Throws Only",        { { "rv_space", 5 }, { "rv_decay", 1.6f }, { "rv_send", 15 }, { "rv_lowcut", 250 },
                                         { "dl_send", 0 }, { "dl_div", 10 }, { "dl_feedback", 45 }, { "dl_mode", 1 }, { "dl_tone", 4500 }, { "dl_lowcut", 300 }, { "dl_duck", 40 } } },
        { "Haunted Hall",              { { "rv_space", 3 }, { "rv_decay", 5.5f }, { "rv_predelay", 45 }, { "rv_send", 35 }, { "rv_lowcut", 200 }, { "rv_highcut", 6500 },
                                         { "dl_send", 0 } } },
        { "Crypt Whisper",             { { "rv_space", 1 }, { "rv_decay", 1.4f }, { "rv_predelay", 5 }, { "rv_send", 40 }, { "rv_width", 70 },
                                         { "dl_send", 0 } } },
        { "Catacomb Footsteps",        { { "rv_space", 2 }, { "rv_decay", 3.2f }, { "rv_predelay", 12 }, { "rv_send", 35 }, { "rv_highcut", 5500 },
                                         { "dl_send", 0 } } },
        { "Cistern Drip",              { { "rv_space", 4 }, { "rv_decay", 7.0f }, { "rv_predelay", 25 }, { "rv_send", 30 }, { "rv_highcut", 11000 },
                                         { "dl_send", 10 }, { "dl_div", 7 }, { "dl_feedback", 30 }, { "dl_mode", 1 } } },
        { "The Void",                  { { "rv_space", 7 }, { "rv_decay", 12.0f }, { "rv_predelay", 80 }, { "rv_send", 45 }, { "rv_lowcut", 180 }, { "rv_highcut", 4500 },
                                         { "dl_send", 0 } } },
        { "Slapback Ghost",            { { "rv_send", 10 }, { "dl_sync", 0 }, { "dl_ms", 110 }, { "dl_feedback", 8 }, { "dl_send", 30 }, { "dl_mode", 2 }, { "dl_tone", 5000 }, { "dl_dirt", 25 } } },
        { "Dotted Eighth Specter",     { { "rv_space", 6 }, { "rv_decay", 2.4f }, { "rv_send", 18 },
                                         { "dl_div", 7 }, { "dl_feedback", 40 }, { "dl_send", 22 }, { "dl_mode", 1 }, { "dl_duck", 50 }, { "dl_tone", 5000 } } },
        { "Ping-Pong Poltergeist",     { { "rv_send", 12 }, { "dl_div", 6 }, { "dl_feedback", 55 }, { "dl_send", 25 }, { "dl_mode", 1 }, { "dl_duck", 35 } } },
        { "Dirty Tape Echo",           { { "rv_send", 8 }, { "dl_div", 10 }, { "dl_feedback", 60 }, { "dl_send", 25 }, { "dl_mode", 0 }, { "dl_tone", 3200 }, { "dl_lowcut", 250 }, { "dl_dirt", 70 } } },
        { "Endless Seance",            { { "rv_space", 3 }, { "rv_decay", 8.0f }, { "rv_send", 30 }, { "rv_highcut", 5000 },
                                         { "dl_div", 12 }, { "dl_feedback", 80 }, { "dl_send", 20 }, { "dl_mode", 1 }, { "dl_tone", 3500 }, { "dl_dirt", 35 }, { "dl_duck", 30 } } },
        { "FX Bus: 100% Wet",          { { "dry", 0 }, { "rv_send", 100 }, { "dl_send", 0 }, { "rv_space", 5 }, { "rv_decay", 2.0f } } },
    };
    return p;
}
} // namespace after
