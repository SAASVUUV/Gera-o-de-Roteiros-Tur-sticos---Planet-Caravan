# Compila o GRASP em C++.  Uso: make -C cpp   (gera cpp/build/grasp)
#
# -ffp-contract=off impede o compilador de fundir a*b+c numa única instrução
# (FMA), o que mudaria o arredondamento e deixaria os resultados diferentes
# dos do Python/numpy.

CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CXXFLAGS += -ffp-contract=off -MMD -MP

SRC_DIR   := src
BUILD_DIR := build
TARGET    := $(BUILD_DIR)/grasp

SOURCES := $(shell find $(SRC_DIR) -name '*.cpp')
OBJECTS := $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/obj/%.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/obj/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

-include $(OBJECTS:.o=.d)
