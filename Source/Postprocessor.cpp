#include "Postprocessor.h"
#include <limits>

Postprocessor::Postprocessor(float fps) : fps_(fps) {}

// Helper for deduplicate_peaks
std::vector<int> Postprocessor::deduplicate_peaks(const std::vector<int>& peaks, int width) {
    std::vector<int> result;
    if (peaks.empty()) {
        return result;
    }

    double p = peaks[0];
    int c = 1;

    for (size_t i = 1; i < peaks.size(); ++i) {
        int p2 = peaks[i];
        if (p2 - p <= width) {
            c += 1;
            p += (static_cast<double>(p2) - p) / c; // update mean
        } else {
            result.push_back(static_cast<int>(std::round(p)));
            p = p2;
            c = 1;
        }
    }
    result.push_back(static_cast<int>(std::round(p)));
    return result;
}

// Helper for max_pool1d equivalent
// This is a simplified version that assumes kernel_size is odd and padding is (kernel_size - 1) / 2
std::vector<float> Postprocessor::max_pool1d_equivalent(const std::vector<float>& input, int kernel_size, int stride, int padding) {
    std::vector<float> output(input.size());
    int half_kernel = kernel_size / 2;

    for (size_t i = 0; i < input.size(); ++i) {
        float max_val = -std::numeric_limits<float>::infinity();
        for (int k = -half_kernel; k <= half_kernel; ++k) {
            int idx = i + k;
            if (idx >= 0 && idx < input.size()) {
                max_val = std::max(max_val, input[idx]);
            }
        }
        output[i] = max_val;
    }
    return output;
}

std::pair<std::vector<float>, std::vector<float>> Postprocessor::process(
    const std::vector<float>& beat_logits,
    const std::vector<float>& downbeat_logits
) {
    // 1. Max pooling equivalent and thresholding
    // Combine beat and downbeat for parallel processing as in Python
    std::vector<float> combined_logits(beat_logits.size() * 2);
    for (size_t i = 0; i < beat_logits.size(); ++i) {
        combined_logits[i] = beat_logits[i];
        combined_logits[i + beat_logits.size()] = downbeat_logits[i];
    }

    std::vector<float> pred_peaks_combined(combined_logits.size());
    std::vector<float> max_pooled_combined = max_pool1d_equivalent(combined_logits, 7, 1, 3);

    for (size_t i = 0; i < combined_logits.size(); ++i) {
        if (combined_logits[i] == max_pooled_combined[i] && combined_logits[i] > 0) {
            pred_peaks_combined[i] = 1.0f; // True
        } else {
            pred_peaks_combined[i] = 0.0f; // False
        }
    }

    // Separate beat and downbeat peaks
    std::vector<float> beat_peaks_bool(beat_logits.size());
    std::vector<float> downbeat_peaks_bool(downbeat_logits.size());
    for (size_t i = 0; i < beat_logits.size(); ++i) {
        beat_peaks_bool[i] = pred_peaks_combined[i];
        downbeat_peaks_bool[i] = pred_peaks_combined[i + beat_logits.size()];
    }

    // Convert boolean arrays to frame indices
    std::vector<int> beat_frame;
    for (size_t i = 0; i < beat_peaks_bool.size(); ++i) {
        if (beat_peaks_bool[i] > 0.5f) { // Check for true
            beat_frame.push_back(i);
        }
    }

    std::vector<int> downbeat_frame;
    for (size_t i = 0; i < downbeat_peaks_bool.size(); ++i) {
        if (downbeat_peaks_bool[i] > 0.5f) { // Check for true
            downbeat_frame.push_back(i);
        }
    }

    // 2. Deduplicate peaks
    beat_frame = deduplicate_peaks(beat_frame, 1);
    downbeat_frame = deduplicate_peaks(downbeat_frame, 1);

    // 3. Convert from frame to seconds
    std::vector<float> beat_time(beat_frame.size());
    for (size_t i = 0; i < beat_frame.size(); ++i) {
        beat_time[i] = static_cast<float>(beat_frame[i]) / fps_;
    }

    std::vector<float> downbeat_time(downbeat_frame.size());
    for (size_t i = 0; i < downbeat_frame.size(); ++i) {
        downbeat_time[i] = static_cast<float>(downbeat_frame[i]) / fps_;
    }

    // 4. Move downbeat to the nearest beat
    if (!beat_time.empty()) {
        for (size_t i = 0; i < downbeat_time.size(); ++i) {
            float d_time = downbeat_time[i];
            float min_diff = std::numeric_limits<float>::max();
            int beat_idx = -1;
            for (size_t j = 0; j < beat_time.size(); ++j) {
                float diff = std::abs(beat_time[j] - d_time);
                if (diff < min_diff) {
                    min_diff = diff;
                    beat_idx = j;
                }
            }
            if (beat_idx != -1) {
                downbeat_time[i] = beat_time[beat_idx];
            }
        }
    }
    
    // Remove duplicate downbeat times (if some db were moved to the same position)
    std::sort(downbeat_time.begin(), downbeat_time.end());
    downbeat_time.erase(std::unique(downbeat_time.begin(), downbeat_time.end()), downbeat_time.end());

    return {beat_time, downbeat_time};
}
