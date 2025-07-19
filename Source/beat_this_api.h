#pragma once

#include <vector>
#include <string>
#include <memory>
#include <utility>

namespace BeatThis {

struct BeatResult {
    std::vector<float> beats;
    std::vector<float> downbeats;
    std::vector<int> beat_counts; // Beat numbers for each beat (1 = downbeat, 2,3,4... = other beats)
};

class BeatThis {
public:
    explicit BeatThis(const std::string& onnx_model_path);
    ~BeatThis();

    // Move semantics
    BeatThis(BeatThis&&) = default;
    BeatThis& operator=(BeatThis&&) = default;

    // Delete copy semantics
    BeatThis(const BeatThis&) = delete;
    BeatThis& operator=(const BeatThis&) = delete;

    // Process audio and get beat/downbeat times as vector data
    BeatResult process_audio(
        const std::vector<float>& audio_data,
        int samplerate,
        int channels = 1
    );

    // Overload for raw pointer interface
    BeatResult process_audio(
        const float* audio_data,
        size_t num_samples,
        int samplerate,
        int channels = 1
    );


private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace BeatThis