
#include "Misc.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

namespace vinfony
{
    void DumpMIDITimedBigMessage( const jdksmidi::MIDITimedBigMessage *msg )
    {
        if ( msg )
        {
            char msgbuf[1024];

            // note that Sequencer generate SERVICE_BEAT_MARKER in files dump,
            // but files themselves not contain this meta event...
            // see MIDISequencer::beat_marker_msg.SetBeatMarker()
            if ( msg->IsBeatMarker() )
            {
                //fprintf( stdout, "%8ld : %s <------------------>", msg->GetTime(), msg->MsgToText( msgbuf, 1024 ) );
                fmt::print("{}: {} <------------------>", msg->GetTime(), msg->MsgToText(msgbuf, 1024));
            }
            else if ( msg->IsSystemExclusive() )
            {
                //fprintf( stdout, "SYSEX length: %d", msg->GetSysEx()->GetLengthSE() );
                fmt::print("SYSEX length: {}", msg->GetSysEx()->GetLengthSE());
            }
            else if ( msg->IsKeySig() ) {
                fmt::print("Key Signature  ");
                msg->GetKeySigSharpFlats();
                const char * keynmes[] = {
                    "F", "G♭", "G", "A♭", "A", "B♭", "B" ,"C", "C♯", "D", "D♯", "E", "F", "F#", "G",
                };
                int i = msg->GetKeySigSharpFlats();
                if (i >= -7 && i <= 7) {
                    fmt::print(keynmes[i+7]);
                }
                if (msg->GetKeySigMajorMinor() == 1)
                    fmt::print(" Minor");
                else
                    fmt::print(" Major");
            } else if ( msg->IsTimeSig() ) {
                fmt::print("Time Signature  {}/{}  Clks/Metro.={} 32nd/Quarter={}",
                    (int)msg->GetTimeSigNumerator(),
                    (int)msg->GetTimeSigDenominator(),
                    (int)msg->GetTimeSigMidiClocksPerMetronome(),
                    (int)msg->GetTimeSigNum32ndPerMidiQuarterNote()
                );

            } else if (msg->IsTempo()) {
                fmt::print("Tempo    {} BPM ({} usec/beat)", msg->GetTempo32()/32.0, msg->GetTempo());
            } else if (msg->IsEndOfTrack()) {
                fmt::print("End of Track");
            } else {
                //fprintf( stdout, "%8ld : %s", msg->GetTime(), msg->MsgToText( msgbuf, 1024 ) );
                fmt::print("{} : {}", msg->GetTime(), msg->MsgToText( msgbuf, 1024 ) );

                if (msg->GetSysEx()) {
                    const unsigned char *buf = msg->GetSysEx()->GetBuf();
                    int len = msg->GetSysEx()->GetLengthSE();
                    std::string str;
                    for ( int i = 0; i < len; ++i ) {
                        if (buf[i] >= 0x20 && buf[i] <= 0x7F)
                            str.push_back( (char)buf[i] );
                    }
                    fmt::print(" Data: {}", str);
                }
            }

            //fprintf( stdout, "\n" );
        }
    }
}