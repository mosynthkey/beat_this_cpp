# Beat This! C++ Implementation

A C++ port of the "Beat This!" AI-powered beat tracking system from Johannes Kepler University Linz. 

## Overview

This is a C++ implementation of the Beat This! model, originally published at ISMIR 2024. The system uses a transformer-based neural network to detect musical beats and downbeats in audio files with high accuracy.

**Original Paper**: "Beat This! Accurate and Generalizable Beat Tracking"  
**Original Repository**: https://github.com/CPJKU/beat_this

## Features

- **Cross-Platform**: Support for macOS and Windows
- **Multiple Output Formats**: Export beats to `.beats` files, generate audio click tracks, or create mixed audio with original music
- **C++ API**: Clean, type-safe interface with RAII and move semantics
- **ONNX Runtime**: Uses optimized neural network inference
- **Flexible CLI**: Support for individual output formats (beats-only or audio-only)

## Architecture

The system consists of four main components:

1. **Audio Processing**: Load and preprocess audio files (mono conversion, resampling)
2. **Mel Spectrogram**: Convert audio to 128-dimensional Mel spectrograms
3. **AI Inference**: Run the trained transformer model using ONNX Runtime
4. **Post-processing**: Extract beat timestamps and counts from neural network outputs

## Dependencies

- **PocketFFT**: Fast Fourier Transform library
- **miniaudio**: Audio file I/O and resampling
- **ONNX Runtime**: Neural network inference engine

## Building

### Prerequisites

The pre-converted ONNX model is already included in the repository (`onnx/beat_this.onnx`). You can start building and using the application immediately.

If you need to convert a custom model, see [onnx/README.md](onnx/README.md) for detailed instructions on converting PyTorch checkpoints to ONNX format.

```bash
git clone https://github.com/mosynthkey/beat_this_cpp.git
cd beat_this_cpp
git submodule update --init --recursive
mkdir build && cd build
cmake -S . -B build
cmake --build build
```

### Build Options

- **Use system ONNX Runtime**: `cmake -DUSE_SYSTEM_ONNXRUNTIME=ON ..`
- **Specify ONNX Runtime version**: Edit `cmake/FetchONNXRuntime.cmake`

### Manual ONNX Runtime Installation

If you prefer to install ONNX Runtime manually:

1. Download the appropriate ONNX Runtime release for your platform
2. Extract to `ThirdParty/onnxruntime-[platform]/`
3. The build system will automatically detect and use it

## Usage

### Command Line Interface

The application supports flexible output options:

**Beat information only**:
```bash
./beat_this_cpp onnx/beat_this.onnx input.wav --output-beats output.beats
```

**Audio click track only**:
```bash
./beat_this_cpp onnx/beat_this.onnx input.wav --output-audio click_track.wav
```

**Mixed audio (original music + click track)**:
```bash
./beat_this_cpp onnx/beat_this.onnx input.wav --output-mixed mixed.wav
```

**Multiple outputs**:
```bash
./beat_this_cpp onnx/beat_this.onnx input.wav --output-beats output.beats --output-audio clicks.wav --output-mixed mixed.wav
```

### C++ API Usage

```cpp
#include "beat_this_api.h"

// Initialize the beat analyzer
BeatThis::BeatThis analyzer("beat_this.onnx");

// Load and process audio
std::vector<float> audio_data = /* load your audio */;
auto result = analyzer.process_audio(audio_data, 44100, 2);

// Access results directly
std::cout << "Found " << result.beats.size() << " beats\n";
for (size_t i = 0; i < result.beats.size(); ++i) {
    std::cout << "Beat " << i << ": time=" << result.beats[i] 
              << "s, count=" << result.beat_counts[i] << "\n";
}
```

## File Formats

### Input Audio
- **Supported formats**: WAV, FLAC, OGG, MP3 (via miniaudio)
- **Automatic processing**: Stereo → mono conversion, resampling

### Output Beats File
The `.beats` file format contains tab-separated values:
```
0.340    4
0.681    1
1.023    2
1.364    3
1.705    4
2.047    1
```
- **Column 1**: Beat time in seconds
- **Column 2**: Beat number (1 = downbeat, 2-4 = other beats)

### Audio Click Track (`--output-audio`)
Generated WAV files contain:
- **Downbeats**: 880 Hz sine wave
- **Other beats**: 440 Hz sine wave
- **Duration**: 0.1 seconds per beat with ADSR envelope

### Mixed Audio (`--output-mixed`)
Mixed audio files combine original music with click track:
- **Original music**: 70% volume, preserved exactly as input
- **Click track**: 30% volume, synchronized with detected beats
- **Format preservation**: Maintains original sample rate and channel configuration
- **Stereo/mono support**: Stereo inputs remain stereo, mono inputs remain mono
- **High fidelity**: No resampling or format conversion to preserve audio quality
- **Duration**: Uses the longer of original audio or beat track duration

## Project Structure

```
beat_this_cpp/
├── Source/
│   ├── beat_this_api.h/cpp       # C++ API interface
│   ├── MelSpectrogram.h/cpp      # Mel spectrogram computation
│   ├── InferenceProcessor.h/cpp  # Neural network inference
│   ├── Postprocessor.h/cpp       # Beat extraction
│   └── main.cpp                  # Command line interface with audio generation
├── onnx/
│   ├── beat_this.onnx           # Pre-converted ONNX model (ready to use)
│   ├── convert_to_onnx.py       # PyTorch to ONNX conversion script
│   ├── beat_this/               # Local beat_this submodule for conversion
│   └── README.md                # ONNX model setup and conversion guide
├── ThirdParty/                   # External dependencies
├── cmake/                        # CMake modules
└── CMakeLists.txt               # Build configuration
```

## API Reference

### BeatResult Structure
```cpp
namespace BeatThis {
    struct BeatResult {
        std::vector<float> beats;        // Beat timestamps in seconds
        std::vector<float> downbeats;    // Downbeat timestamps in seconds  
        std::vector<int> beat_counts;    // Beat numbers (1=downbeat, 2,3,4...=other beats)
    };
}
```

### BeatThis Class
```cpp
namespace BeatThis {
    class BeatThis {
    public:
        explicit BeatThis(const std::string& onnx_model_path);
        
        // Process audio from vector
        BeatResult process_audio(
            const std::vector<float>& audio_data,
            int samplerate,
            int channels = 1
        );
        
        // Process audio from raw pointer
        BeatResult process_audio(
            const float* audio_data,
            size_t num_samples,
            int samplerate,
            int channels = 1
        );
    };
}
```

## Windows-Specific Notes

### Running the Application

After a successful build, the executables will be in:
- `build\Release\beat_this_cpp.exe` - Main application
- `build\Release\beat_this_api.dll` - API library

## Model Details

- **Architecture**: Transformer-based neural network
- **Input**: 128-dimensional Mel spectrograms (batch, time, freq)
- **Output**: Beat and downbeat probability logits
- **Training**: Trained on large-scale music datasets from the original Beat This! research
- **Inference**: Chunked processing for long audio files
- **ONNX Compatibility**: Fully compatible with ONNX Runtime, opset version 14

### ONNX Model Information

The included ONNX model (`onnx/beat_this.onnx`) is converted from the official Beat This! PyTorch checkpoint:

- **Source**: https://cloud.cp.jku.at/public.php/dav/files/7ik4RrBKTS273gp/final0.ckpt
- **Input format**: `input_spectrogram` with shape `[1, time_frames, 128]`
- **Output format**: `beat` and `downbeat` logits with shape `[1, time_frames]`
- **Dynamic axes**: Variable time dimension for processing audio of any length
- **Model size**: ~97 MB (includes full transformer architecture)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

Note: This C++ implementation is separate from the original Beat This! repository. Please refer to the original project for their licensing terms.

## Acknowledgments

- **Original Beat This! Implementation**: This C++ port is based on the Beat This! model from Johannes Kepler University Linz. Please refer to the [original repository](https://github.com/CPJKU/beat_this) for the original implementation, research paper, and licensing terms.
- **ONNX Runtime**: Microsoft's high-performance inference engine
- **Dependencies**: PocketFFT, miniaudio, and ONNX Runtime developers