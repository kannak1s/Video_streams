#ifndef FRAME_PROCESSOR_H
#define FRAME_PROCESSOR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load a video from input_path, convert each frame to grayscale, sort every
 * pixel row using quicksort_row(), and write the result to output_path (XVID
 * codec, .avi).
 *
 * Per-frame timing (microseconds) is printed to stdout.
 *
 * @param input_path   Path to the source video file.
 * @param output_path  Path for the row-sorted output video.
 * @return 0 on success, non-zero on error.
 */
int process_video(const char *input_path, const char *output_path);

#ifdef __cplusplus
}
#endif

#endif /* FRAME_PROCESSOR_H */
