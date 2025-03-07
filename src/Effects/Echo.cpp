/*
    Echo.cpp - Echo effect

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2011, Alan Calvert
    Copyright 2018-2021, Will Godfrey

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is derivative of ZynAddSubFX original code.

*/

#include "Misc/NumericFuncs.h"
#include "Misc/SynthHelper.h"
#include "Misc/SynthEngine.h"
#include "Effects/Echo.h"
#include <iostream>

using func::power;
using func::powFrac;

static const int PRESET_SIZE = 7;
static const int NUM_PRESETS = 9;
static unsigned char presets[NUM_PRESETS][PRESET_SIZE] = {
        { 67, 64, 35, 64, 30, 59, 0 },     // Echo 1
        { 67, 64, 21, 64, 30, 59, 0 },     // Echo 2
        { 67, 75, 60, 64, 30, 59, 10 },    // Echo 3
        { 67, 60, 44, 64, 30, 0, 0 },      // Simple Echo
        { 67, 60, 102, 50, 30, 82, 48 },   // Canyon
        { 67, 64, 44, 17, 0, 82, 24 },     // Panning Echo 1
        { 81, 60, 46, 118, 100, 68, 18 },  // Panning Echo 2
        { 81, 60, 26, 100, 127, 67, 36 },  // Panning Echo 3
        { 62, 64, 28, 64, 100, 90, 55 }    // Feedback Echo
    };

Echo::Echo(bool insertion_, float* efxoutl_, float* efxoutr_, SynthEngine *_synth) :
    Effect(insertion_, efxoutl_, efxoutr_, NULL, 0, _synth),
    Pdelay(60),
    Plrdelay(100),
    feedback(1, _synth->samplerate),
    hidamp(1, _synth->samplerate),
    lrdelay(0),
    ldelay(NULL),
    rdelay(NULL),
    lxfade(1, _synth->samplerate_f),
    rxfade(1, _synth->samplerate_f)
{
    setvolume(50);
    setfeedback(40);
    sethidamp(60);
    setpreset(Ppreset);
    changepar(4, 30); // lrcross
    Pchanged = false;
    maxdelay = 5 * synth->samplerate;
    ldelay = new float[maxdelay];
    rdelay = new float[maxdelay];
    realposl = realposr = 1;
    cleanup();
    initdelays();
}


Echo::~Echo()
{
    delete [] ldelay;
    delete [] rdelay;
}


// Cleanup the effect
void Echo::cleanup(void)
{
    Effect::cleanup();
    feedback.pushToTarget();
    hidamp.pushToTarget();
    lxfade.pushToTarget();
    rxfade.pushToTarget();
    memset(ldelay, 0, maxdelay * sizeof(float));
    memset(rdelay, 0, maxdelay * sizeof(float));
    oldl = oldr = 0.0f;
}


// Initialize the delays
void Echo::initdelays(void)
{
    if (Pbpm)
    {
        int olddelay = delay;

        std::pair<float, float> frac = func::LFOfreqBPMFraction((float)Pdelay / 127.0f);
        delay = roundf(60.0f * synth->samplerate_f * frac.first / (synth->getBPM() * frac.second));
        if (delay > maxdelay)
            delay = maxdelay;

        if (!synth->isBPMAccurate())
        {
            // If we don't have an accurate BPM source, we may have
            // fluctuations. Reject delay changes less than a certain amount to
            // prevent Echo artifacts.

            float ratio;
            if (delay > olddelay)
                ratio = (float)delay / (float)olddelay;
            else
                ratio = (float)olddelay / (float)delay;
            if (ratio < ECHO_INACCURATE_BPM_THRESHOLD)
                delay = olddelay;
        }
    }
    else
    {
        delay = int(Pdelay / 127.0f * synth->samplerate_f * 1.5f);
        delay += 1; // 0 .. 1.5 sec
    }

    dl = delay - lrdelay;
    if (dl < 1)
        dl = 1;
    dr = delay + lrdelay;
    if (dr < 1)
        dr = 1;
}


// Effect output
void Echo::out(float* smpsl, float* smpsr)
{
    float l, r;
    float ldl;
    float rdl;

    outvolume.advanceValue(synth->sent_buffersize);

    initdelays();

    for (int i = 0; i < synth->sent_buffersize; ++i)
    {
        lxfade.setTargetValue(dl);
        rxfade.setTargetValue(dr);

        int targetpos;

        targetpos = realposl - lxfade.getNewValue();
        if (targetpos < 0)
            targetpos += maxdelay;
        ldl = ldelay[targetpos];

        targetpos = realposr - rxfade.getNewValue();
        if (targetpos < 0)
            targetpos += maxdelay;
        rdl = rdelay[targetpos];

        if (lxfade.isInterpolating())
        {
            targetpos = realposl - lxfade.getOldValue();
            if (targetpos < 0)
                targetpos += maxdelay;

            ldl = ldelay[targetpos] * (1.0f - lxfade.factor()) + ldl * lxfade.factor();
        }

        if (rxfade.isInterpolating())
        {
            targetpos = realposr - rxfade.getOldValue();
            if (targetpos < 0)
                targetpos += maxdelay;

            rdl = rdelay[targetpos] * (1.0f - rxfade.factor()) + rdl * rxfade.factor();
        }

        ldl += float(1e-20); // anti-denormal included
        rdl += float(1e-20); // anti-denormal included

        l = ldl * (1.0 - lrcross.getValue()) + rdl * lrcross.getValue();
        r = rdl * (1.0 - lrcross.getValue()) + ldl * lrcross.getValue();
        lrcross.advanceValue();
        ldl = l;
        rdl = r;

        efxoutl[i] = ldl * 2.0f - 1e-20f; // anti-denormal - a very, very, very
        efxoutr[i] = rdl * 2.0f - 1e-20f; // small dc bias

        ldl = smpsl[i] * pangainL.getAndAdvanceValue() - ldl * feedback.getValue();
        rdl = smpsr[i] * pangainR.getAndAdvanceValue() - rdl * feedback.getValue();
        feedback.advanceValue();

        // LowPass Filter
        ldelay[realposl] = ldl = ldl * hidamp.getValue() + oldl * (1.0f - hidamp.getValue());
        rdelay[realposl] = rdl = rdl * hidamp.getValue() + oldr * (1.0f - hidamp.getValue());
        hidamp.advanceValue();
        oldl = ldl;
        oldr = rdl;

        if (++realposl >= maxdelay)
            realposl = 0;
        if (++realposr >= maxdelay)
            realposr = 0;

        lxfade.advanceValue();
        rxfade.advanceValue();
    }
}


// Parameter control
void Echo::setvolume(unsigned char Pvolume_)
{
    Pvolume = Pvolume_;
    if (insertion == 0)
    {
        outvolume.setTargetValue(4.0f * powFrac<100>(1.0f - Pvolume / 127.0f));
        volume.setTargetValue(1.0f);
    }
    else
    {
        float tmp = Pvolume / 127.0f;
        volume.setTargetValue(tmp);
        outvolume.setTargetValue(tmp);
    }
    if (Pvolume == 0)
        cleanup();
}


void Echo::setdelay(const unsigned char Pdelay_)
{
    Pdelay = Pdelay_;
}


void Echo::setlrdelay(unsigned char Plrdelay_)
{
    float tmp;
    Plrdelay = Plrdelay_;
    tmp = (power<2>(fabsf(Plrdelay - 64.0f) / 64.0f * 9.0f) -1.0f) / 1000.0f * synth->samplerate_f;
    if (Plrdelay < 64.0f)
        tmp = -tmp;
    lrdelay = (int)tmp;
}


void Echo::setfeedback(unsigned char Pfb_)
{
    Pfb = Pfb_;
    feedback.setTargetValue(Pfb / 128.0f);
}


void Echo::sethidamp(unsigned char Phidamp_)
{
    Phidamp = Phidamp_;
    hidamp.setTargetValue(1.0 - Phidamp / 127.0f);
}


void Echo::setpreset(unsigned char npreset)
{
    if (npreset < 0xf)
    {
        if (npreset >= NUM_PRESETS)
            npreset = NUM_PRESETS - 1;
        for (int n = 0; n < PRESET_SIZE; ++n)
            changepar(n, presets[npreset][n]);
        if (insertion)
            changepar(0, presets[npreset][0] / 2); // lower the volume if this is insertion effect
        // All presets use no BPM syncing.
        changepar(EFFECT::control::bpm, 0);
        Ppreset = npreset;
    }
    else
    {
        unsigned char preset = npreset & 0xf;
        unsigned char param = npreset >> 4;
        if (param == 0xf)
            param = 0;
        changepar(param, presets[preset][param]);
        if (insertion && (param == 0))
            changepar(0, presets[preset][0] / 2);
    }
    Pchanged = false;
}


void Echo::changepar(int npar, unsigned char value)
{
    if (npar == -1)
    {
        Pchanged = (value != 0);
        return;
    }
    Pchanged = true;
    switch (npar)
    {
        case 0:
            setvolume(value);
            break;

        case 1:
            setpanning(value);
            break;

        case 2:
            setdelay(value);
            break;

        case 3:
            setlrdelay(value);
            break;

        case 4:
            setlrcross(value);
            break;

        case 5:
            setfeedback(value);
            break;

        case 6:
            sethidamp(value);
            break;

        case EFFECT::control::bpm:
            Pbpm = value;
            break;

        default:
            Pchanged = false;
            break;
    }
}


unsigned char Echo::getpar(int npar)
{
    switch (npar)
    {
        case -1: return Pchanged;
        case 0: return Pvolume;
        case 1: return Ppanning;
        case 2: return Pdelay;
        case 3: return Plrdelay;
        case 4: return Plrcross;
        case 5: return Pfb;
        case 6: return Phidamp;
        case EFFECT::control::bpm: return Pbpm;
        default: break;
    }
    return 0; // in case of bogus parameter number
}


float Echolimit::getlimits(CommandBlock *getData)
{
    int value = getData->data.value;
    int control = getData->data.control;
    int request = getData->data.type & TOPLEVEL::type::Default; // clear flags
    int npart = getData->data.part;
    int presetNum = getData->data.engine;
    int min = 0;
    int max = 127;

    int def = presets[presetNum][control];
    unsigned char canLearn = TOPLEVEL::type::Learnable;
    unsigned char isInteger = TOPLEVEL::type::Integer;
    switch (control)
    {
        case 0:
            if (npart != TOPLEVEL::section::systemEffects) // system effects
                def /= 2;
            break;
        case 1:
            break;
        case 2:
            break;
        case 3:
            break;
        case 4:
            break;
        case 5:
            break;
        case 6:
            break;
        case EFFECT::control::bpm:
            max = 1;
            canLearn = 0;
            break;
        case EFFECT::control::preset:
            max = 8;
            canLearn = 0;
            break;
        default:
            getData->data.type |= TOPLEVEL::type::Error;
            return 1.0f;
            break;
    }

    switch (request)
    {
        case TOPLEVEL::type::Adjust:
            if (value < min)
                value = min;
            else if (value > max)
                value = max;
            break;
        case TOPLEVEL::type::Minimum:
            value = min;
            break;
        case TOPLEVEL::type::Maximum:
            value = max;
            break;
        case TOPLEVEL::type::Default:
            value = def;
            break;
    }
    getData->data.type |= (canLearn + isInteger);
    return float(value);
}
