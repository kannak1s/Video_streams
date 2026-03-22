CXX      = g++
CC       = gcc
CFLAGS   = -O2 -Wall -pthread
CXXFLAGS = -O2 -Wall -pthread $(shell pkg-config --cflags opencv4 2>/dev/null)
LDFLAGS  = -pthread -lrt -lm $(shell pkg-config --libs opencv4 2>/dev/null)

TARGET   = video_sort
SRCS_C   = src/quicksort.c src/metrics.c \
           src/scheduler_bounded_prolific.c \
           src/scheduler_bounded_collective.c \
           src/scheduler_bounded_sko.c \
           src/scheduler_bounded_subreaper_prolific.c \
           src/scheduler_mmap_subreaper_collective.c \
           src/scheduler_mmap_subreaper_collective_barrier_affinity.c
SRCS_CXX = src/main.cpp src/frame_processor.cpp

OBJS     = $(SRCS_C:.c=.o) $(SRCS_CXX:.cpp=.o)

.PHONY: all clean

all: results $(TARGET)

results:
	mkdir -p results

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
