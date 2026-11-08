#pragma once
#include <juce_core/juce_core.h>
namespace rg {
// Tiny append-only diagnostic log: Documents/Trill Tron/RAYGUN/raygun_log.txt  (capped; safe to delete)
inline void dbg(const juce::String& msg) {
    static juce::CriticalSection cs; const juce::ScopedLock sl(cs);
    auto f = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Trill Tron").getChildFile("RAYGUN");
    f.createDirectory(); f = f.getChildFile("raygun_log.txt");
    if (f.getSize() > 300000) f.deleteFile();
    auto line = juce::Time::getCurrentTime().formatted("%H:%M:%S") + " [thr " + juce::String::toHexString((juce::int64) (juce::pointer_sized_int) juce::Thread::getCurrentThreadId()) + "] " + msg + "\n";
    f.appendText(line);
    juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("raygun_log.txt").appendText(line);   // mirror, in case Documents is not writable
}
}
