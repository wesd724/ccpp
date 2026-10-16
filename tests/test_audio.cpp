#include "test.h"
// White-box tests include the monolithic engine in one translation unit only.
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
#include "../keyboard-piano-cpp/src/audio_engine.cpp"

int main()
{
    using namespace piano;
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = 48000;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 4;
    CHECK(detect_encoding(format) == SampleEncoding::pcm16);
    format.nBlockAlign = 1;
    CHECK(detect_encoding(format) == SampleEncoding::unsupported);
    format.nBlockAlign = 4;
    format.nSamplesPerSec = 0;
    CHECK(detect_encoding(format) == SampleEncoding::unsupported);

    WAVEFORMATEXTENSIBLE extended{};
    extended.Format = format;
    extended.Format.nSamplesPerSec = 48000;
    extended.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    extended.Format.cbSize = sizeof(extended) - sizeof(WAVEFORMATEX);
    extended.Samples.wValidBitsPerSample = 16;
    extended.SubFormat = {WAVE_FORMAT_PCM, 0, 0x10, {0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71}};
    CHECK(detect_encoding(extended.Format) == SampleEncoding::pcm16);
    extended.SubFormat.Data4[7] ^= 1;
    CHECK(detect_encoding(extended.Format) == SampleEncoding::unsupported);
    extended.SubFormat.Data4[7] ^= 1;
    extended.Format.wBitsPerSample = 32;
    extended.Format.nBlockAlign = 8;
    extended.Samples.wValidBitsPerSample = 24;
    CHECK(detect_encoding(extended.Format) == SampleEncoding::pcm32);
    const PcmQuantizer quantize(24, 32);
    CHECK((quantize(0.5F) & 255) == 0);
    CHECK(quantize(1.0F) == 2147483392);
    CHECK(quantize(-1.0F) == -2147483392);
    CHECK(quantize(2.0F) == quantize(1.0F));

    for (int i = 0; i < 4; ++i) {
        Voice voice{};
        voice.timbre = static_cast<Timbre>(i);
        voice.midi_note = 60;
        voice.stage = EnvelopeStage::attack;
        double energy = 0;
        for (int sample = 0; sample < 4800; ++sample) {
            advance_envelope(voice, 1.0F / 48000.0F);
            const float sample_value = render_oscillator(voice, 48000);
            CHECK(std::isfinite(sample_value));
            energy += sample_value * sample_value;
        }
        CHECK(energy > 0);
        begin_release(voice);
        for (int sample = 0; sample < 96000; ++sample) advance_envelope(voice, 1.0F / 48000.0F);
        CHECK(voice.stage == EnvelopeStage::off);
    }
    for (int i = 0; i < 8; ++i) {
        Voice voice{};
        voice.kind = VoiceKind::drum;
        voice.drum = static_cast<DrumSound>(i);
        voice.stage = EnvelopeStage::attack;
        double energy = 0;
        for (int sample = 0; sample < 240000; ++sample) {
            if (voice.stage == EnvelopeStage::off) break;
            const float sample_value = render_drum(voice, 48000);
            CHECK(std::isfinite(sample_value));
            energy += sample_value * sample_value;
        }
        CHECK(energy > 0 && voice.stage == EnvelopeStage::off);
    }
    puts("Audio formats, valid PCM bits, four timbres, releases and eight drum tails passed.");
    return 0;
}
