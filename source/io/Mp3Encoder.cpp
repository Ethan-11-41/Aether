// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "Mp3Encoder.h"

#include <vector>

namespace aether
{

namespace
{
    // The few LAME functions Aether uses (from lame.h; LAME is LGPL and loaded at run time).
    using lame_t = void*;
    using InitFn = lame_t (*)();
    using SetIntFn = int (*) (lame_t, int);
    using InitParamsFn = int (*) (lame_t);
    using EncodeFloatFn = int (*) (lame_t, const float*, const float*, int, unsigned char*, int);
    using FlushFn = int (*) (lame_t, unsigned char*, int);
    using CloseFn = int (*) (lame_t);
    using VersionFn = const char* (*)();

    constexpr int lameJointStereo = 1, lameMono = 3;

    juce::File& libraryOverride()
    {
        static juce::File file;
        return file;
    }

    juce::String libraryName()
    {
       #if JUCE_WINDOWS
        return "libmp3lame.dll";
       #else
        return "libmp3lame.so";
       #endif
    }

    /** Loads the library and finds the functions. */
    struct Library
    {
        bool load()
        {
            const auto file = Mp3Encoder::getLibraryFile();

            if (! dll.open (file.getFullPathName()))
                return false;

            init = (InitFn) dll.getFunction ("lame_init");
            setInRate = (SetIntFn) dll.getFunction ("lame_set_in_samplerate");
            setChannels = (SetIntFn) dll.getFunction ("lame_set_num_channels");
            setBitRate = (SetIntFn) dll.getFunction ("lame_set_brate");
            setQuality = (SetIntFn) dll.getFunction ("lame_set_quality");
            setMode = (SetIntFn) dll.getFunction ("lame_set_mode");
            setWriteTag = (SetIntFn) dll.getFunction ("lame_set_bWriteVbrTag");
            initParams = (InitParamsFn) dll.getFunction ("lame_init_params");
            encodeFloat = (EncodeFloatFn) dll.getFunction ("lame_encode_buffer_ieee_float");
            flush = (FlushFn) dll.getFunction ("lame_encode_flush");
            close = (CloseFn) dll.getFunction ("lame_close");
            version = (VersionFn) dll.getFunction ("get_lame_version");

            return init && setInRate && setChannels && setBitRate && setQuality && setMode && setWriteTag
                && initParams && encodeFloat && flush && close;
        }

        juce::DynamicLibrary dll;
        InitFn init = nullptr;
        SetIntFn setInRate = nullptr, setChannels = nullptr, setBitRate = nullptr, setQuality = nullptr,
                 setMode = nullptr, setWriteTag = nullptr;
        InitParamsFn initParams = nullptr;
        EncodeFloatFn encodeFloat = nullptr;
        FlushFn flush = nullptr;
        CloseFn close = nullptr;
        VersionFn version = nullptr;
    };
}

struct Mp3Encoder::Lame
{
    Library library;
    lame_t handle = nullptr;
    std::unique_ptr<juce::FileOutputStream> out;
    std::vector<unsigned char> buffer;
    int numChannels = 2;
};

Mp3Encoder::Mp3Encoder() = default;

Mp3Encoder::~Mp3Encoder()
{
    close();
}

void Mp3Encoder::setLibraryFile (const juce::File& file)   { libraryOverride() = file; }

juce::File Mp3Encoder::getLibraryFile()
{
    if (libraryOverride() != juce::File())
        return libraryOverride();

    return juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile (libraryName());
}

bool Mp3Encoder::isAvailable()
{
    Library library;
    return library.load();
}

juce::String Mp3Encoder::getLameVersion()
{
    Library library;
    return library.load() && library.version != nullptr ? juce::String (library.version()) : juce::String();
}

juce::Result Mp3Encoder::open (const juce::File& file, double sampleRate, int numChannels, int kbps)
{
    close();
    lame = std::make_unique<Lame>();

    if (! lame->library.load())
    {
        lame = nullptr;
        return juce::Result::fail ("MP3 export needs the LAME encoder (" + libraryName() + "), which wasn't found next to "
                                   "Aether. Reinstalling Aether puts it back.");
    }

    auto& l = lame->library;
    lame->handle = l.init();
    lame->numChannels = juce::jlimit (1, 2, numChannels);

    if (lame->handle == nullptr)
        return juce::Result::fail ("The MP3 encoder couldn't start.");

    l.setInRate (lame->handle, juce::roundToInt (sampleRate));
    l.setChannels (lame->handle, lame->numChannels);
    l.setMode (lame->handle, lame->numChannels == 1 ? lameMono : lameJointStereo);
    l.setBitRate (lame->handle, kbps);
    l.setQuality (lame->handle, 2);    // 2 = high quality (0 best/slowest, 9 worst/fastest)
    l.setWriteTag (lame->handle, 0);   // constant bit rate: no VBR header needed

    if (l.initParams (lame->handle) < 0)
        return juce::Result::fail ("The MP3 encoder doesn't support these settings (" + juce::String (kbps) + " kbps at "
                                   + juce::String (sampleRate / 1000.0, 1) + " kHz).");

    file.deleteFile();
    lame->out = std::make_unique<juce::FileOutputStream> (file);

    if (lame->out->failedToOpen())
        return juce::Result::fail ("Couldn't create " + file.getFullPathName() + ".");

    return juce::Result::ok();
}

juce::Result Mp3Encoder::write (const float* left, const float* right, int numSamples)
{
    if (lame == nullptr || lame->handle == nullptr || lame->out == nullptr)
        return juce::Result::fail ("The MP3 file isn't open.");

    // LAME's own worst case for the encoded size.
    lame->buffer.resize ((size_t) (numSamples + numSamples / 4 + 7200));
    const auto bytes = lame->library.encodeFloat (lame->handle, left, lame->numChannels == 2 ? right : left, numSamples,
                                                  lame->buffer.data(), (int) lame->buffer.size());

    if (bytes < 0 || ! lame->out->write (lame->buffer.data(), (size_t) bytes))
        return juce::Result::fail ("Couldn't write the MP3 file (is the disk full?).");

    return juce::Result::ok();
}

juce::Result Mp3Encoder::close()
{
    if (lame == nullptr)
        return juce::Result::ok();

    auto result = juce::Result::ok();

    if (lame->handle != nullptr)
    {
        if (lame->out != nullptr)
        {
            lame->buffer.resize (7200);
            const auto bytes = lame->library.flush (lame->handle, lame->buffer.data(), (int) lame->buffer.size());

            const auto written = bytes >= 0 && lame->out->write (lame->buffer.data(), (size_t) bytes);
            lame->out->flush();

            if (! written || lame->out->getStatus().failed())
                result = juce::Result::fail ("Couldn't finish the MP3 file.");
        }

        lame->library.close (lame->handle);
    }

    lame = nullptr;
    return result;
}

} // namespace aether
