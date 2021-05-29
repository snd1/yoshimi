/*
    TestInvoker.h - invoke sound synthesis for automated testing

    Copyright 2021, Ichthyostega

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#ifndef TESTINVOKER_H
#define TESTINVOKER_H

#include <string>
#include <unistd.h>
#include <iostream>
#include <memory>
#include <ctime>

#include "Misc/SynthEngine.h"

using std::cout;
using std::endl;
using std::string;

namespace { // local implementation details

    class StopWatch
    {
        timespec start;
    public:
        StopWatch()
        {
            clock_gettime(CLOCK_REALTIME, &start);
        }
        
        size_t getNanosSinceStart()
        {
            timespec now;
            clock_gettime(CLOCK_REALTIME, &now);
            
            return (now.tv_nsec - start.tv_nsec)
                 + (now.tv_sec  - start.tv_sec) * 1000*1000*1000;
        }
    };

}//(End)implementation details


class TestInvoker
{
    int chan;
    int pitch;
    float duration;
    uint repetitions;
    size_t chunksize;
    size_t smpCnt;

    using BufferHolder = std::unique_ptr<float[]>;
    
    public:
        TestInvoker() :
            chan{0},
            pitch{60},        // C4
            duration{1.0},    // 1sec
            repetitions{1},
            chunksize{128},
            smpCnt{0}
        { }


        /* Main test function: run the SynthEngine synchronous, possibly dump results into a file.
         * Note: the current audio/midi backend is not used at all.
         */
        void performSoundCalculation(SynthEngine& synth)
        {
            BufferHolder buffer;
            allocate(buffer);
            synth.reseed(0);
            cout << "TEST::Launch"<<endl;
            //////////////////////TODO maybe open output file
            smpCnt = 0;
            StopWatch timer;
            for (uint i=0; i<repetitions; ++i)
                pullSound(synth, buffer);
            //////////////////////TODO capture end time
            //////////////////////TODO calculated overall time
            size_t runtime = timer.getNanosSinceStart();
            double speed = double(runtime) / smpCnt;
            cout << "TEST::Complete"
                 << " runtime "<<runtime<<" ns"
                 << " speed "<<speed<<" ns/Sample"
                 << endl;
        }


    private:
        void allocate(BufferHolder& buffer)
        {
            size_t size = 2 * (NUM_MIDI_PARTS + 1)
                            * chunksize;
            buffer.reset(new float[size]);
        }



        void pullSound(SynthEngine& synth, BufferHolder& buffer)
        {
            float* buffL[NUM_MIDI_PARTS + 1];
            float* buffR[NUM_MIDI_PARTS + 1];
            for (uint i=0; i<=NUM_MIDI_PARTS; ++i)
            {
                buffL[i] = & buffer[(2*i  ) * chunksize];
                buffR[i] = & buffer[(2*i+1) * chunksize];
            }
            
            // find out how much buffer cycles are required to get the desired note play time
            size_t buffCnt = rintf(duration * synth.samplerate / chunksize);
            
            // calculate sound data
            synth.NoteOn(chan, pitch, 64);  //////TODO velocity
            for (size_t i=0; i<buffCnt; ++i)
                smpCnt += synth.MasterAudio(buffL,buffR, chunksize);
            synth.NoteOff(chan, pitch);
        }
};



#endif /*TESTINVOKER_H*/
