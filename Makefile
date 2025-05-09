# Define the compiler and flags
CXX = mpic++

# Define the target executable name
TARGET = parcount

# Source file
SRC = parcount.cpp

# Default target to build the executable
all: $(TARGET)

# Build the target executable by compiling the source file directly
$(TARGET): $(SRC)
	$(CXX) -o $(TARGET) $(SRC)

# Clean up the generated executable
clean:
	rm -f $(TARGET)

# Run the program with 4 MPI processes (change -np as needed)
run:
	mpirun -np 4 ./$(TARGET) input.txt

# Run the program with a specified number of MPI processes
run-np:
	mpirun -np $(NP) ./$(TARGET) $(FILE)

# Phony targets
.PHONY: all clean run run-np
