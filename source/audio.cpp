// SPDX-License-Identifier: GPL-3.0-only
#include "audio.h"
#include "sseq/Player.h"
#include "sseq/INFOSection.h"
#include "sseq/FATSection.h"
#include <array>
#include <map>
#include <memory>
#include <cstdio>

// Gameplay retains the DS's sixteen channels. UI sounds need a separate bank
// so a pause chime neither runs out of channels nor steals a suspended note.
struct Engine {
    Channel channels[32];
    Player players[AUDIO_PLAYERS];
    std::map<unsigned, std::unique_ptr<SWAR>> waves;
    std::map<unsigned, std::unique_ptr<SBNK>> banks;
    std::map<unsigned, std::unique_ptr<SSEQ>> sequences;
    bool active[AUDIO_PLAYERS]{};
    int sequence[AUDIO_PLAYERS]{};
    double clock = 0;
    std::vector<std::vector<int16_t>> voices;
    std::vector<unsigned> voice_order;
    unsigned voice_next = 0, voice_last = ~0u, voice_position = 0;
    int voice_current = -1;
    uint32_t voice_rng = 1;

    Engine() {
        for (int i = 0; i < 32; i++)
            channels[i].chnId = i % 16;
        for (int i = 0; i < AUDIO_PLAYERS; i++) {
            auto &p = players[i];
            p.channels = channels + (i == AUDIO_UI_PLAYER ? 16 : 0);
            p.sampleRate = AUDIO_RATE;
            p.interpolation = INTERPOLATION_NONE;
        }
    }

    void read(Blob b) {
        std::vector<uint8_t> bytes(b.p, b.p + b.size);
        PseudoFile f;
        f.data = &bytes;
        INFOSection info;
        f.pos = ReadLE<uint32_t>(b.p + 24);
        info.Read(f);
        FATSection fat;
        f.pos = ReadLE<uint32_t>(b.p + 32);
        fat.Read(f);

        for (const auto &v : info.SEQrecord.entries) {
            if (v.first > 33) continue;
            auto s = std::unique_ptr<SSEQ>(new SSEQ);
            s->info = v.second;
            f.pos = fat.records.at(v.second.fileID).offset;
            s->Read(f);

            unsigned bank = v.second.bank;
            if (!banks.count(bank)) {
                auto k = std::unique_ptr<SBNK>(new SBNK);
                k->info = info.BANKrecord.entries.at(bank);
                f.pos = fat.records.at(k->info.fileID).offset;
                k->Read(f);
                for (int i = 0; i < 4; i++) {
                    unsigned id = k->info.waveArc[i];
                    if (id == 65535) continue;
                    if (!waves.count(id)) {
                        auto w = std::unique_ptr<SWAR>(new SWAR);
                        w->info = info.WAVEARCrecord.entries.at(id);
                        f.pos = fat.records.at(w->info.fileID).offset;
                        w->Read(f);
                        waves[id] = std::move(w);
                    }
                    k->waveArc[i] = waves.at(id).get();
                }
                banks[bank] = std::move(k);
            }
            s->bank = banks.at(bank).get();
            sequences[v.first] = std::move(s);
        }

        if (sequences.size() != 34) throw std::runtime_error("Missing Bird & Beans sequences");
    }

    bool running(int i) const {
        if (!active[i]) return false;
        const Player &p = players[i];
        for (int j = 0; j < p.nTracks; j++)
            if (!p.tracks[p.trackIds[j]].state[TS_END]) return true;
        for (const auto &c : channels)
            if (c.ply == &p && c.state != CS_NONE) return true;
        return false;
    }
};

int audio_init(Audio *a, Blob b) {
    a->engine = nullptr;
    a->paused = 0;
    a->mixed_frames = 0;
    a->ui_end_frame = 0;
    if (b.size < 64 || memcmp(b.p, "SDAT", 4)) return 0;
    try {
        auto e = std::unique_ptr<Engine>(new Engine);
        e->read(b);
        a->engine = e.release();
        return 1;
    } catch (const std::exception &e) {
        fprintf(stderr, "Audio data: %s\n", e.what());
        return 0;
    }
}

void audio_free(Audio *a) {
    delete static_cast<Engine *>(a->engine);
    a->engine = nullptr;
}

int audio_load_voices(Audio *a, const char *path, uint32_t seed) {
    if (!a->engine) return 0;
    auto &e = *static_cast<Engine *>(a->engine);

    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    if (fseek(file, 0, SEEK_END)) {
        fclose(file);
        return 0;
    }
    long size = ftell(file);
    rewind(file);
    if (size < 8 || size > 16 * 1024 * 1024) {
        fclose(file);
        return 0;
    }

    try {
        std::vector<uint8_t> bytes(size);
        bool ok = fread(bytes.data(), 1, size, file) == (size_t)size;
        fclose(file);
        file = nullptr;
        unsigned count = le32(bytes.data() + 4);
        if (!ok || memcmp(bytes.data(), "BBV1", 4) || !count || count > (bytes.size() - 8) / 8)
            return 0;

        std::vector<std::vector<int16_t>> clips;
        for (unsigned i = 0; i < count; i++) {
            unsigned off = le32(bytes.data() + 8 + i * 8),
                     samples = le32(bytes.data() + 12 + i * 8);
            if (off < 8 + count * 8
                || off > bytes.size()
                || !samples
                || samples > (bytes.size() - off) / 2)
                return 0;
            clips.emplace_back(samples);
            for (unsigned j = 0; j < samples; j++)
                clips.back()[j] = (int16_t)le16(bytes.data() + off + j * 2);
        }

        e.voices = std::move(clips);
        e.voice_order.resize(count);
        e.voice_next = count;
        e.voice_last = ~0u;
        e.voice_current = -1;
        e.voice_rng = seed ? seed : 1;
        for (unsigned i = 0; i < count; i++)
            e.voice_order[i] = i;
        return count;
    } catch (const std::exception &error) {
        if (file) fclose(file);
        fprintf(stderr, "Voice data: %s\n", error.what());
        return 0;
    }
}

int audio_start_voice(Audio *a) {
    if (!a->engine) return 0;
    auto &e = *static_cast<Engine *>(a->engine);
    unsigned n = e.voices.size();
    if (!n) return 0;

    auto random = [&]() {
        e.voice_rng ^= e.voice_rng << 13;
        e.voice_rng ^= e.voice_rng >> 17;
        e.voice_rng ^= e.voice_rng << 5;
        return e.voice_rng;
    };
    if (e.voice_next == n) {
        for (unsigned i = n - 1; i > 0; i--)
            std::swap(e.voice_order[i], e.voice_order[random() % (i + 1)]);
        if (n > 1 && e.voice_order[0] == e.voice_last)
            std::swap(e.voice_order[0], e.voice_order[1 + random() % (n - 1)]);
        e.voice_next = 0;
    }

    e.voice_last = e.voice_order[e.voice_next++];
    e.voice_current = e.voice_last;
    e.voice_position = 0;
    a->ui_end_frame = a->mixed_frames + e.voices[e.voice_current].size();
    return 1;
}

void audio_stop(Audio *a) {
    if (!a->engine) return;
    auto &e = *static_cast<Engine *>(a->engine);
    for (int i = 0; i < AUDIO_PLAYERS; i++) {
        e.players[i].Stop(true);
        e.active[i] = false;
    }
    e.voice_current = -1;
    a->paused = 0;
    a->ui_end_frame = 0;
}

void audio_play(Audio *a, int ch, int seq) {
    if (!a->engine || ch < 0 || ch >= AUDIO_PLAYERS) return;
    auto &e = *static_cast<Engine *>(a->engine);
    auto &p = e.players[ch];
    p.Stop(true);
    e.active[ch] = false;
    if (!e.sequences.count(seq)) return;

    const auto *s = e.sequences.at(seq).get();
    p.prio = s->info.channelPriority >= 64 ? s->info.channelPriority - 64 : 0;
    p.sseqVol = Cnv_Sust(s->info.vol);
    p.externalVol = 0;
    p.externalPan = 0;
    if (p.Setup(s)) {
        e.active[ch] = true;
        e.sequence[ch] = seq;
        if (ch == AUDIO_UI_PLAYER) a->ui_end_frame = UINT64_MAX;
    }
}

void audio_commands(Audio *a, Game *g) {
    if (!a->engine) return;
    auto &e = *static_cast<Engine *>(a->engine);
    for (int i = 0; i < g->sound_count; i++) {
        int ch = g->sound_channel[i], cmd = g->sound_queue[i];
        if (ch < 0 || ch >= AUDIO_PLAYERS) continue;
        if (cmd < 34)
            audio_play(a, ch, cmd);
        else if (cmd & 0x40000) {
            e.players[ch].externalPan = (int8_t)(cmd & 255);
            for (auto &t : e.players[ch].tracks)
                t.updateFlags.set(TUF_PAN);
        } else if (cmd & 0x20000)
            e.players[ch].tempoRate = cmd & 65535;
        else if (cmd & 0x10000) {
            e.players[ch].externalVol = Cnv_Sust(cmd & 127);
            for (auto &t : e.players[ch].tracks)
                t.updateFlags.set(TUF_VOL);
        }
    }

    g->sound_count = 0;
    for (int i = 0; i < AUDIO_PLAYERS; i++) {
        g->audio_ticks[i] = e.players[i].sequenceTicks;
        g->audio_active[i] = e.running(i);
        g->audio_sequence[i] = e.sequence[i];
    }
}

static int mul127(int v, int m) {
    return m == 127 ? v : (v * m) >> 7;
}

void audio_mix(Audio *a, int16_t *out, unsigned frames) {
    if (!a->engine) {
        memset(out, 0, frames * 4);
        return;
    }
    auto &e = *static_cast<Engine *>(a->engine);
    for (int i = 0; i < AUDIO_PLAYERS; i++)
        e.players[i].suspended = a->paused && i != AUDIO_UI_PLAYER;

    for (unsigned f = 0; f < frames; f++) {
        int left = 0, right = 0;
        for (auto &c : e.channels) {
            if (c.state == CS_NONE || !c.ply || c.ply->suspended) continue;
            int sample = c.GenerateSample();
            c.IncrementSample();
            unsigned shift = c.reg.volumeDiv == 3 ? 4 : c.reg.volumeDiv;
            sample = mul127(sample, c.reg.volumeMul) >> shift;
            left += mul127(sample, 127 - c.reg.panning);
            right += mul127(sample, c.reg.panning);
        }
        clamp(left, -32768, 32767);
        clamp(right, -32768, 32767);

        if (e.voice_current >= 0) {
            const auto &clip = e.voices[e.voice_current];
            int voice = clip[e.voice_position++];
            unsigned edge = std::min(e.voice_position, (unsigned)clip.size() - e.voice_position);
            unsigned duck = std::min(edge, 320u);
            left = left * (640 - (int)duck) / 640 + voice;
            right = right * (640 - (int)duck) / 640 + voice;
            if (e.voice_position == clip.size()) e.voice_current = -1;
            clamp(left, -32768, 32767);
            clamp(right, -32768, 32767);
        }

        out[f * 2] = left;
        out[f * 2 + 1] = right;
        e.clock += 1.0 / AUDIO_RATE;
        if (e.clock >= SecondsPerClockCycle) {
            e.clock -= SecondsPerClockCycle;
            for (int i = 0; i < AUDIO_PLAYERS; i++)
                if (e.active[i] && !e.players[i].suspended) e.players[i].UpdateTracks();
            for (auto &c : e.channels)
                if (c.ply && !c.ply->suspended) c.Update();
            for (int i = 0; i < AUDIO_PLAYERS; i++)
                if (e.active[i] && !e.players[i].suspended) e.players[i].Run();
        }

        if (a->ui_end_frame == UINT64_MAX && !e.running(AUDIO_UI_PLAYER))
            a->ui_end_frame = a->mixed_frames + f + 1;
    }
    a->mixed_frames += frames;
}
