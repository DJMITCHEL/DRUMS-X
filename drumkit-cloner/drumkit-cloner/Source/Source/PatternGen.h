#pragma once

#include "Common.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

// Generador de patrones: Trap, Boom Bap, Hip Hop, Rap y Drill (2 compases a 1/16).
// "ratchet" = cuantos golpes caben en un paso (2 = 1/32, 3 = tresillo, 4 = 1/64): para los rolls de hi-hat.
namespace dkc
{
    enum Style { Trap = 0, BoomBap, HipHop, Rap, Drill, numStyles };

    inline const char* styleName (int s)
    {
        static const char* n[] = { "Trap", "Boom Bap", "Hip Hop", "Rap", "Drill" };
        return n[std::max (0, std::min (numStyles - 1, s))];
    }

    inline double styleBpm (int s)
    {
        static const double b[] = { 140.0, 90.0, 95.0, 88.0, 142.0 };
        return b[std::max (0, std::min (numStyles - 1, s))];
    }

    struct PatternData
    {
        std::array<uint8_t, numPads * numSteps> vel {}, ratchet {};
    };

    struct GenPads
    {
        int kick = 0, snare = 2, clap = 3, hat = 6, openHat = 10;
    };

    namespace gen
    {
        struct Ctx
        {
            Ctx (PatternData& d, float inten, unsigned seed) : pd (d), intensity (inten), rng (seed) {}

            PatternData& pd;
            float intensity;
            std::mt19937 rng;

            bool chance (float p) { return std::uniform_real_distribution<float> (0.0f, 1.0f) (rng) < p; }
            int rint (int a, int b) { return std::uniform_int_distribution<int> (a, b) (rng); }

            void set (int pad, int step, int v, int r = 1)
            {
                if (pad < 0 || pad >= numPads || step < 0 || step >= numSteps) return;
                pd.vel[(size_t) (pad * numSteps + step)] = (uint8_t) std::max (1, std::min (127, v));
                pd.ratchet[(size_t) (pad * numSteps + step)] = (uint8_t) std::max (1, std::min (4, r));
            }

            void erase (int pad, int step)
            {
                if (pad < 0 || pad >= numPads || step < 0 || step >= numSteps) return;
                pd.vel[(size_t) (pad * numSteps + step)] = 0;
                pd.ratchet[(size_t) (pad * numSteps + step)] = 1;
            }

            void clearRow (int pad) { for (int s = 0; s < numSteps; ++s) erase (pad, s); }

            std::vector<int> pick (std::vector<int> cand, int k)
            {
                std::shuffle (cand.begin(), cand.end(), rng);
                if (k < (int) cand.size()) cand.resize ((size_t) k);
                return cand;
            }
        };

        inline void hats (Ctx& c, const GenPads& g, int style)
        {
            const float I = c.intensity;
            c.clearRow (g.hat);
            if (g.openHat != g.hat) c.clearRow (g.openHat);

            for (int bar = 0; bar < 2; ++bar)
            {
                const int o = bar * 16;

                switch (style)
                {
                    case Trap:
                    {
                        for (int st = 0; st < 16; st += 2)
                            c.set (g.hat, o + st, st % 4 == 0 ? 100 : 72);
                        for (int st = 1; st < 16; st += 2)                       // notas fantasma
                            if (c.chance (0.10f + 0.30f * I)) c.set (g.hat, o + st, 46 + c.rint (0, 14));

                        const int nRolls = c.chance (0.35f + 0.55f * I) ? (c.chance (0.15f + 0.35f * I) ? 2 : 1) : 0;
                        for (int p : c.pick ({ 6, 7, 10, 11, 13, 14, 15 }, nRolls))   // rolls tipo trap
                        {
                            const int r = c.chance (0.45f) ? 2 : (c.chance (0.5f) ? 3 : 4);
                            c.set (g.hat, o + p, 86, r);
                        }
                        if (c.chance (0.45f))
                        {
                            const int st = c.chance (0.5f) ? 14 : 15;
                            c.erase (g.hat, o + st);
                            c.set (g.openHat, o + st, 92);
                        }
                        break;
                    }
                    case BoomBap:
                    {
                        for (int st = 0; st < 16; st += 2)
                            c.set (g.hat, o + st, st % 4 == 0 ? 88 : 66);
                        for (int st = 1; st < 16; st += 2)
                            if (c.chance (0.05f + 0.20f * I)) c.set (g.hat, o + st, 44);
                        if (c.chance (0.35f))
                        {
                            const int st = bar == 1 ? 14 : 6;
                            c.erase (g.hat, o + st);
                            c.set (g.openHat, o + st, 90);
                        }
                        break;
                    }
                    case HipHop:
                    {
                        for (int st = 0; st < 16; st += 2)
                            c.set (g.hat, o + st, st % 4 == 0 ? 92 : 70);
                        for (int st = 1; st < 16; st += 2)
                            if (c.chance (0.40f + 0.30f * I)) c.set (g.hat, o + st, 52 + c.rint (0, 10));
                        if (c.chance (0.35f))
                        {
                            const int st = c.chance (0.5f) ? 6 : 14;
                            c.erase (g.hat, o + st);
                            c.set (g.openHat, o + st, 90);
                        }
                        break;
                    }
                    case Rap:
                    {
                        for (int st = 0; st < 16; st += 2)
                            c.set (g.hat, o + st, st % 4 == 0 ? 82 : 64);
                        for (int st : { 3, 11, 15 })                              // pares de 16avos antes de la caja
                            if (c.chance (0.20f + 0.30f * I)) c.set (g.hat, o + st, 56);
                        if (c.chance (0.25f))
                        {
                            c.erase (g.hat, o + 14);
                            c.set (g.openHat, o + 14, 88);
                        }
                        break;
                    }
                    case Drill:
                    default:
                    {
                        for (int st = 0; st < 16; st += 2)
                            c.set (g.hat, o + st, st % 4 == 0 ? 88 : 68, c.chance (0.20f + 0.40f * I) ? 3 : 1);   // tresillos
                        break;
                    }
                }
            }
        }

        inline void drums (Ctx& c, const GenPads& g, int style)
        {
            const float I = c.intensity;
            c.clearRow (g.kick);
            c.clearRow (g.snare);
            if (g.clap != g.snare) c.clearRow (g.clap);

            auto backbeat = [&] (int o, int st, int v)
            {
                c.set (g.snare, o + st, v);
                if (g.clap != g.snare && g.clap != g.kick) c.set (g.clap, o + st, v - 12);
            };

            for (int bar = 0; bar < 2; ++bar)
            {
                const int o = bar * 16;

                switch (style)
                {
                    case Trap:
                    {
                        backbeat (o, 8, 112);                                      // caja a medio tiempo
                        c.set (g.kick, o, 120);
                        const int extra = 1 + (int) std::lround (I * 2.0f) + bar;
                        for (int p : c.pick ({ 3, 6, 7, 10, 11, 13, 14, 15 }, extra))
                            c.set (g.kick, o + p, 90 + c.rint (0, 22));
                        if (c.chance (I * 0.3f)) c.set (g.snare, o + 15, 40);
                        break;
                    }
                    case BoomBap:
                    {
                        backbeat (o, 4, 112);
                        backbeat (o, 12, 112);
                        c.set (g.kick, o, 120);
                        c.set (g.kick, o + 10, 106);
                        if (bar == 1) c.set (g.kick, o + (c.chance (0.5f) ? 6 : 7), 96);
                        if (c.chance (I * 0.35f)) c.set (g.kick, o + (c.chance (0.5f) ? 3 : 13), 80);
                        if (c.chance (I * 0.30f)) c.set (g.snare, o + (c.chance (0.5f) ? 7 : 15), 40);
                        break;
                    }
                    case HipHop:
                    {
                        backbeat (o, 4, 110);
                        backbeat (o, 12, 110);
                        c.set (g.kick, o, 120);
                        if (c.chance (0.6f)) c.set (g.kick, o + (c.chance (0.5f) ? 6 : 7), 100);
                        c.set (g.kick, o + 10, 104);
                        if (bar == 1 && c.chance (0.5f + 0.3f * I)) c.set (g.kick, o + (c.chance (0.5f) ? 11 : 13), 92);
                        if (c.chance (I * 0.30f)) c.set (g.snare, o + 15, 40);
                        break;
                    }
                    case Rap:
                    {
                        backbeat (o, 4, 106);
                        backbeat (o, 12, 106);
                        c.set (g.kick, o, 120);
                        const int extra = 1 + (int) std::lround (I * 2.0f);
                        for (int p : c.pick ({ 3, 6, 7, 10, 11, 14 }, extra + bar))
                            c.set (g.kick, o + p, 92 + c.rint (0, 16));
                        break;
                    }
                    case Drill:
                    default:
                    {
                        backbeat (o, 8, 114);
                        c.set (g.kick, o, 120);
                        const int extra = 2 + (int) std::lround (I * 2.0f);
                        for (int p : c.pick ({ 3, 6, 10, 11, 13, 15 }, extra))
                            c.set (g.kick, o + p, 90 + c.rint (0, 20));
                        break;
                    }
                }
            }
            hats (c, g, style);
        }
    }

    inline void generateFull (PatternData& pd, const GenPads& g, int style, float intensity, unsigned seed)
    {
        gen::Ctx c (pd, std::max (0.0f, std::min (1.0f, intensity)), seed);
        gen::drums (c, g, std::max (0, std::min (numStyles - 1, style)));
    }

    inline void generateHats (PatternData& pd, const GenPads& g, int style, float intensity, unsigned seed)
    {
        gen::Ctx c (pd, std::max (0.0f, std::min (1.0f, intensity)), seed);
        gen::hats (c, g, std::max (0, std::min (numStyles - 1, style)));
    }
}
