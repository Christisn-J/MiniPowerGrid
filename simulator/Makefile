# Compiler
CXX := g++
CXXFLAGS := -std=c++17 -Wall -pthread

# Directories
SRCDIR := ./src
INCDIR := ./include
BINDIR := ./bin
ENVIRONMENT := /opt/homebrew

# Include and Library paths
INC := -I$(INCDIR) \
       -I$(ENVIRONMENT)/include

# Libraries
LIB := -L$(ENVIRONMENT)/lib -lboost_json

TARGET := $(BINDIR)/power_network
SOURCES := $(SRCDIR)/main.cpp

# Build target
all: $(TARGET)

$(TARGET): $(SOURCES)
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) $(SOURCES) $(INC) $(LIB) -o $(TARGET)

clean:
	@rm -rf $(BINDIR)/*