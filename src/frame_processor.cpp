#include "frame_processor.h"
#include "quicksort.h"

#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <time.h>

int process_video(const char *input_path, const char *output_path)
{
    cv::VideoCapture cap(input_path);
    if (!cap.isOpened()) {
        fprintf(stderr, "frame_processor: cannot open video '%s'\n", input_path);
        return 1;
    }

    int width  = (int)cap.get(cv::CAP_PROP_FRAME_WIDTH);
    int height = (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0.0) fps = 25.0;

    int fourcc = cv::VideoWriter::fourcc('X','V','I','D');
    cv::VideoWriter writer(output_path, fourcc, fps, cv::Size(width, height), false);
    if (!writer.isOpened()) {
        fprintf(stderr, "frame_processor: cannot open output video '%s'\n", output_path);
        return 1;
    }

    cv::Mat frame, gray;
    int frame_idx = 0;

    while (cap.read(frame)) {
        /* Convert to grayscale */
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

        /* Time the per-frame sort */
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);

        /* Sort every row in place */
        int rows = gray.rows;
        int cols = gray.cols;
        for (int r = 0; r < rows; r++) {
            uint8_t *row_ptr = gray.ptr<uint8_t>(r);
            if (cols > 1)
                quicksort_row(row_ptr, 0, cols - 1);
        }

        clock_gettime(CLOCK_MONOTONIC, &t1);
        long us = (long)((t1.tv_sec - t0.tv_sec) * 1000000L +
                         (t1.tv_nsec - t0.tv_nsec) / 1000L);

        printf("Frame %4d: %ld us\n", frame_idx, us);

        writer.write(gray);
        frame_idx++;
    }

    printf("Total frames processed: %d  ->  %s\n", frame_idx, output_path);
    return 0;
}
