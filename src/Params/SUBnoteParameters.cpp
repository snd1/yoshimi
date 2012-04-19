/*
    SUBnoteParameters.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of version 2 of the GNU General Public
    License as published by the Free Software Foundation.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is derivative of original ZynAddSubFX code, modified November 2010
*/

#include "Misc/BodyDisposal.h"
#include "Misc/Config.h"
#include "Params/SUBnoteParameters.h"

SUBnoteParameters::SUBnoteParameters():Presets()
{
    setpresettype("Psubsyth");
    AmpEnvelope = new EnvelopeParams(64, 1);
    AmpEnvelope->ADSRinit_dB(0, 40, 127, 25);
    FreqEnvelope = new EnvelopeParams(64, 0);
    FreqEnvelope->ASRinit(30, 50, 64, 60);
    BandWidthEnvelope = new EnvelopeParams(64, 0);
    BandWidthEnvelope->ASRinit_bw(100, 70, 64, 60);

    GlobalFilter = new FilterParams(2, 80, 40);
    GlobalFilterEnvelope = new EnvelopeParams(0, 1);
    GlobalFilterEnvelope->ADSRinit_filter(64, 40, 64, 70, 60, 64);
    defaults();
}

void SUBnoteParameters::defaults(void)
{
    PVolume  = 96;
    PPanning = 64;
    PAmpVelocityScaleFunction = 90;

    Pfixedfreq   = 0;
    PfixedfreqET = 0;
    Pnumstages   = 2;
    Pbandwidth   = 40;
    Phmagtype    = 0;
    Pbwscale     = 64;
    Pstereo      = true;
    Pstart = 1;

    PDetune = 8192;
    PCoarseDetune = 0;
    PDetuneType   = 1;
    PFreqEnvelopeEnabled      = 0;
    PBandWidthEnvelopeEnabled = 0;

    for(int n = 0; n < MAX_SUB_HARMONICS; ++n) {
        Phmag[n]   = 0;
        Phrelbw[n] = 64;
    }
    Phmag[0] = 127;

    PGlobalFilterEnabled = 0;
    PGlobalFilterVelocityScale = 64;
    PGlobalFilterVelocityScaleFunction = 64;

    AmpEnvelope->defaults();
    FreqEnvelope->defaults();
    BandWidthEnvelope->defaults();
    GlobalFilter->defaults();
    GlobalFilterEnvelope->defaults();
}

SUBnoteParameters::~SUBnoteParameters()
{
    Runtime.deadObjects->addBody(AmpEnvelope);
    Runtime.deadObjects->addBody(FreqEnvelope);
    Runtime.deadObjects->addBody(BandWidthEnvelope);
    Runtime.deadObjects->addBody(GlobalFilter);
    Runtime.deadObjects->addBody(GlobalFilterEnvelope);
}

void SUBnoteParameters::add2XML(XMLwrapper *xml)
{
    xml->addpar("num_stages", Pnumstages);
    xml->addpar("harmonic_mag_type", Phmagtype);
    xml->addpar("start", Pstart);

    xml->beginbranch("HARMONICS");
    for(int i = 0; i < MAX_SUB_HARMONICS; i++) {
        if((Phmag[i] == 0) && (xml->minimal))
            continue;
        xml->beginbranch("HARMONIC", i);
        xml->addpar("mag", Phmag[i]);
        xml->addpar("relbw", Phrelbw[i]);
        xml->endbranch();
    }
    xml->endbranch();

    xml->beginbranch("AMPLITUDE_PARAMETERS");
    xml->addparbool("stereo", (Pstereo) ? 1 : 0);
    xml->addpar("volume", PVolume);
    xml->addpar("panning", PPanning);
    xml->addpar("velocity_sensing", PAmpVelocityScaleFunction);
    xml->beginbranch("AMPLITUDE_ENVELOPE");
    AmpEnvelope->add2XML(xml);
    xml->endbranch();
    xml->endbranch();

    xml->beginbranch("FREQUENCY_PARAMETERS");
    xml->addparbool("fixed_freq", Pfixedfreq);
    xml->addpar("fixed_freq_et", PfixedfreqET);

    xml->addpar("detune", PDetune);
    xml->addpar("coarse_detune", PCoarseDetune);
    xml->addpar("detune_type", PDetuneType);

    xml->addpar("bandwidth", Pbandwidth);
    xml->addpar("bandwidth_scale", Pbwscale);

    xml->addparbool("freq_envelope_enabled", PFreqEnvelopeEnabled);
    if((PFreqEnvelopeEnabled != 0) || (!xml->minimal)) {
        xml->beginbranch("FREQUENCY_ENVELOPE");
        FreqEnvelope->add2XML(xml);
        xml->endbranch();
    }

    xml->addparbool("band_width_envelope_enabled", PBandWidthEnvelopeEnabled);
    if((PBandWidthEnvelopeEnabled != 0) || (!xml->minimal)) {
        xml->beginbranch("BANDWIDTH_ENVELOPE");
        BandWidthEnvelope->add2XML(xml);
        xml->endbranch();
    }
    xml->endbranch();

    xml->beginbranch("FILTER_PARAMETERS");
    xml->addparbool("enabled", PGlobalFilterEnabled);
    if((PGlobalFilterEnabled != 0) || (!xml->minimal)) {
        xml->beginbranch("FILTER");
        GlobalFilter->add2XML(xml);
        xml->endbranch();

        xml->addpar("filter_velocity_sensing",
                    PGlobalFilterVelocityScaleFunction);
        xml->addpar("filter_velocity_sensing_amplitude",
                    PGlobalFilterVelocityScale);

        xml->beginbranch("FILTER_ENVELOPE");
        GlobalFilterEnvelope->add2XML(xml);
        xml->endbranch();
    }
    xml->endbranch();
}

void SUBnoteParameters::getfromXML(XMLwrapper *xml)
{
    Pnumstages = xml->getpar127("num_stages", Pnumstages);
    Phmagtype  = xml->getpar127("harmonic_mag_type", Phmagtype);
    Pstart     = xml->getpar127("start", Pstart);

    if(xml->enterbranch("HARMONICS")) {
        Phmag[0] = 0;
        for(int i = 0; i < MAX_SUB_HARMONICS; i++) {
            if(xml->enterbranch("HARMONIC", i) == 0)
                continue;
            Phmag[i]   = xml->getpar127("mag", Phmag[i]);
            Phrelbw[i] = xml->getpar127("relbw", Phrelbw[i]);
            xml->exitbranch();
        }
        xml->exitbranch();
    }

    if(xml->enterbranch("AMPLITUDE_PARAMETERS")) {
        int xpar = xml->getparbool("stereo", (Pstereo) ? 1 : 0);
        Pstereo  = (xpar != 0) ? true : false;
        PVolume  = xml->getpar127("volume", PVolume);
        PPanning = xml->getpar127("panning", PPanning);
        PAmpVelocityScaleFunction = xml->getpar127("velocity_sensing",
                                                   PAmpVelocityScaleFunction);
        if(xml->enterbranch("AMPLITUDE_ENVELOPE")) {
            AmpEnvelope->getfromXML(xml);
            xml->exitbranch();
        }
        xml->exitbranch();
    }

    if(xml->enterbranch("FREQUENCY_PARAMETERS")) {
        Pfixedfreq   = xml->getparbool("fixed_freq", Pfixedfreq);
        PfixedfreqET = xml->getpar127("fixed_freq_et", PfixedfreqET);

        PDetune = xml->getpar("detune", PDetune, 0, 16383);
        PCoarseDetune = xml->getpar("coarse_detune", PCoarseDetune, 0, 16383);
        PDetuneType   = xml->getpar127("detune_type", PDetuneType);

        Pbandwidth = xml->getpar127("bandwidth", Pbandwidth);
        Pbwscale   = xml->getpar127("bandwidth_scale", Pbwscale);

        PFreqEnvelopeEnabled = xml->getparbool("freq_envelope_enabled",
                                               PFreqEnvelopeEnabled);
        if(xml->enterbranch("FREQUENCY_ENVELOPE")) {
            FreqEnvelope->getfromXML(xml);
            xml->exitbranch();
        }

        PBandWidthEnvelopeEnabled = xml->getparbool(
            "band_width_envelope_enabled",
            PBandWidthEnvelopeEnabled);
        if(xml->enterbranch("BANDWIDTH_ENVELOPE")) {
            BandWidthEnvelope->getfromXML(xml);
            xml->exitbranch();
        }
        xml->exitbranch();
    }

    if(xml->enterbranch("FILTER_PARAMETERS")) {
        PGlobalFilterEnabled = xml->getparbool("enabled", PGlobalFilterEnabled);
        if(xml->enterbranch("FILTER")) {
            GlobalFilter->getfromXML(xml);
            xml->exitbranch();
        }

        PGlobalFilterVelocityScaleFunction = xml->getpar127(
            "filter_velocity_sensing",
            PGlobalFilterVelocityScaleFunction);
        PGlobalFilterVelocityScale = xml->getpar127(
            "filter_velocity_sensing_amplitude",
            PGlobalFilterVelocityScale);

        if(xml->enterbranch("FILTER_ENVELOPE")) {
            GlobalFilterEnvelope->getfromXML(xml);
            xml->exitbranch();
        }
        xml->exitbranch();
    }
}
