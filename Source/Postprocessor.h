#ifndef POSTPROCESSOR_H
#define POSTPROCESSOR_H

#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>

/**
 * @class Postprocessor
 * @brief Converts neural network logits to beat timestamps
 * 
 * This class processes raw neural network outputs (logits) to extract
 * beat and downbeat timestamps. It performs peak detection, deduplication,
 * and converts frame indices to time values.
 */
class Postprocessor {
public:
    Postprocessor(float fps = 50.0f);

    /**
     * @brief Process neural network logits to extract beat timestamps
     * @param beat_logits Raw beat predictions from neural network
     * @param downbeat_logits Raw downbeat predictions from neural network
     * @return Pair of vectors containing beat and downbeat timestamps in seconds
     */
    std::pair<std::vector<float>, std::vector<float>> process(
        const std::vector<float>& beat_logits,
        const std::vector<float>& downbeat_logits
    );

private:
    float fps_;  // Frames per second for time conversion

    // Helper for peak deduplication
    std::vector<int> deduplicate_peaks(const std::vector<int>& peaks, int width);

    // Helper for 1D max pooling (equivalent to PyTorch's max_pool1d)
    std::vector<float> max_pool1d_equivalent(const std::vector<float>& input, int kernel_size, int stride, int padding);
};

#endif // POSTPROCESSOR_H
