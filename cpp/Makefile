# Compila os programas C++.  Uso: make -C cpp
#
# Cada arquivo src/apps/NOME.cpp vira o executável build/NOME:
#   build/grasp           GRASP dos autores (tradução literal)
#   build/check_results   verificador de viabilidade dos resultados
# O resto de src/ é código compartilhado entre eles.
#
# -ffp-contract=off impede o compilador de fundir a*b+c numa única instrução
# (FMA), o que mudaria o arredondamento e deixaria os resultados diferentes
# dos do Python/numpy.

CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CXXFLAGS += -ffp-contract=off -MMD -MP

SRC_DIR   := src
BUILD_DIR := build

APP_SOURCES    := $(wildcard $(SRC_DIR)/apps/*.cpp)
SHARED_SOURCES := $(filter-out $(SRC_DIR)/apps/%,$(shell find $(SRC_DIR) -name '*.cpp'))

APPS           := $(APP_SOURCES:$(SRC_DIR)/apps/%.cpp=$(BUILD_DIR)/%)
SHARED_OBJECTS := $(SHARED_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/obj/%.o)
ALL_OBJECTS    := $(SHARED_OBJECTS) $(APP_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/obj/%.o)

.PHONY: all clean
# mantém os .o entre compilações (senão o make apaga os intermediários)
.SECONDARY: $(ALL_OBJECTS)

all: $(APPS)

$(BUILD_DIR)/%: $(BUILD_DIR)/obj/apps/%.o $(SHARED_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/obj/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

-include $(ALL_OBJECTS:.o=.d)
