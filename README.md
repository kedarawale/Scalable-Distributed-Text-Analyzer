# Scalable Distributed Text Analyzer 

## Project Overview

This project implements a parallel MPI program that analyzes word and character frequency in large text files using 1 to 8 MPI processes. It is designed as part of Project 1 for the COP5611 Parallel and Distributed Systems course. The program can be executed both on native MPI environments and Docker containers and is tested up to 1GB file sizes.

---

## File Descriptions

- `parcount.cpp` – C++ implementation of the parallel counter using MPI.
- `Makefile` – Build file to compile the MPI program.
- `Analysis.docx` – Detailed performance analysis and benchmarking.

---

## Key Features

- Parallel file chunk reading using MPI I/O.
- Efficient word and character frequency counting.
- Handles word boundary issues across chunk splits.
- Aggregates results using MPI Reduce and Gatherv.
- Outputs the top 10 characters and words based on frequency.
- Measures execution time on root process.

---

## Algorithm Summary

1. **File Chunking:** File is divided into even chunks per process using MPI file operations.
2. **Local Counting:** Each process counts characters and words in its chunk.
3. **Boundary Handling:** Word fragments at chunk boundaries are communicated between processes.
4. **Aggregation:** Character counts are reduced using MPI_Reduce; word data is gathered and unpacked using MPI_Gatherv.
5. **Sorting & Output:** Results are sorted and displayed based on defined criteria.

---

## Sample Execution

```bash
make
mpirun -np 4 ./parcount test1.txt
```

Sample Output:
```
========= Top 10 Characters =========
Ch	Freq
---------------------------
e	2030
t	1814
...

=========== Top 10 Words ===========
Word             ID      Freq
---------------------------
the              1       120
and              4       98
...

Execution time: 3.84 seconds
```

---


## Compilation and Execution

### Compile
```bash
make
```

### Execute
```bash
mpirun -np <num_processes> ./parcount <input_file>
```

---


## References

- OpenMPI Docs: https://www.open-mpi.org/faq/
- Docker MPI Guide: https://github.com/guswns531/mpi-docker

---
