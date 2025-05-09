#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <mpi.h>
#include <ctime>
#include <cctype>
#include <cstring>
#include <numeric> 

#define ASCII_START 32
#define ASCII_END 126
#define NUM_CHARS (ASCII_END - ASCII_START + 1)
#define MAX_WORD_LEN 63

struct CharCount {
    char character;
    int count;
};

struct WordInfo {
    std::string word;
    int count;
    int first_occurrence;
};

bool compare_char(const CharCount &a, const CharCount &b) {
    if (a.count == b.count) {
        return a.character < b.character;
    }
    return a.count > b.count;
}

bool compare_word(const WordInfo &a, const WordInfo &b) {
    if (a.count == b.count) {
        return a.first_occurrence < b.first_occurrence;
    }
    return a.count > b.count;
}

void count_characters(const std::vector<char> &buffer, std::vector<int> &local_counts) {
    for (char c : buffer) {
        if (c >= ASCII_START && c <= ASCII_END) {
            local_counts[c - ASCII_START]++;
        }
    }
}

void count_words(const std::vector<char> &buffer, std::unordered_map<std::string, WordInfo> &local_word_counts, int &word_id, int start_offset) {
    std::string word;
    for (size_t i = 0; i < buffer.size(); ++i) {
        char c = buffer[i];
        if (isalnum(c)) {
            word += std::tolower(c);
        } else if (!word.empty()) {
            if (local_word_counts.find(word) == local_word_counts.end()) {
                local_word_counts[word] = {word, 1, word_id++ + start_offset};
            } else {
                local_word_counts[word].count++;
            }
            word.clear();
        }
    }
}

void receive_and_merge_last_word(std::vector<char> &buffer, int rank) {
    int len;
    MPI_Recv(&len, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (len > 0) {
        std::vector<char> received_word(len);
        MPI_Recv(received_word.data(), len, MPI_CHAR, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        buffer.insert(buffer.begin(), received_word.begin(), received_word.end());
    }
}

void send_last_word(const std::string &last_word, int rank) {
    int len = last_word.size();
    MPI_Send(&len, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
    if (len > 0) {
        MPI_Send(last_word.c_str(), len, MPI_CHAR, rank + 1, 0, MPI_COMM_WORLD);
    }
}

std::vector<int> flatten_word_counts(const std::unordered_map<std::string, WordInfo> &word_counts) {
    std::vector<int> flat_data;
    for (const auto &[word, info] : word_counts) {
        int len = word.length();
        flat_data.push_back(len); // length of the word
        flat_data.insert(flat_data.end(), word.begin(), word.end()); // word
        flat_data.push_back(info.count); // word count
        flat_data.push_back(info.first_occurrence); // first occurrence
    }
    return flat_data;
}

void unflatten_and_aggregate_word_counts(const std::vector<int> &flat_data, std::unordered_map<std::string, WordInfo> &global_word_counts) {
    size_t i = 0;
    while (i < flat_data.size()) {
        int len = flat_data[i++]; 
        std::string word(flat_data.begin() + i, flat_data.begin() + i + len);
        i += len;  // Skip over the word
        int count = flat_data[i++];
        int first_occurrence = flat_data[i++];

        if (global_word_counts.find(word) == global_word_counts.end()) {
            global_word_counts[word] = {word, count, first_occurrence};
        } else {
            global_word_counts[word].count += count;
            global_word_counts[word].first_occurrence = std::min(global_word_counts[word].first_occurrence, first_occurrence);
        }
    }
}

int main(int argc, char *argv[]) {
    int rank, size;
    clock_t start_time = clock();
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2) {
        if (rank == 0) {
            std::cerr << "Usage: " << argv[0] << " <input_file>" << std::endl;
        }
        MPI_Finalize();
        return 1;
    }

    const char *filename = argv[1];
    MPI_File file;
    MPI_Offset file_size;
    MPI_Status status;

    std::vector<int> local_counts(NUM_CHARS, 0);
    std::vector<int> global_counts(NUM_CHARS, 0);

    MPI_File_open(MPI_COMM_WORLD, filename, MPI_MODE_RDONLY, MPI_INFO_NULL, &file);
    MPI_File_get_size(file, &file_size);

    MPI_Offset chunk_size = file_size / size;
    MPI_Offset offset = rank * chunk_size;

    if (rank == size - 1) {
        chunk_size += file_size % size;
    }

    std::vector<char> buffer(chunk_size);
    MPI_File_read_at_all(file, offset, buffer.data(), chunk_size, MPI_CHAR, &status);

    count_characters(buffer, local_counts);

    if (rank != 0) {
        receive_and_merge_last_word(buffer, rank);
    }

    std::unordered_map<std::string, WordInfo> local_word_counts;
    int word_id = 1;
    count_words(buffer, local_word_counts, word_id, rank * chunk_size);

    if (rank != size - 1) {
        std::string last_word;
        for (int i = buffer.size() - 1; i >= 0; --i) {
            if (isalnum(buffer[i])) {
                last_word.insert(last_word.begin(), std::tolower(buffer[i]));
            } else if (!last_word.empty()) {
                break;
            }
        }
        send_last_word(last_word, rank);
    }

    MPI_Reduce(local_counts.data(), global_counts.data(), NUM_CHARS, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    std::vector<int> local_word_counts_flat = flatten_word_counts(local_word_counts);
    int local_word_count_size = local_word_counts_flat.size();

    // Step 1: Gather the sizes of each process's flattened word count data
    std::vector<int> word_counts_sizes(size);
    MPI_Gather(&local_word_count_size, 1, MPI_INT, word_counts_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Calculate displacements for MPI_Gatherv
    std::vector<int> displs(size, 0);
    int total_word_data_size = 0;
    if (rank == 0) {
        total_word_data_size = std::accumulate(word_counts_sizes.begin(), word_counts_sizes.end(), 0);
        for (int i = 1; i < size; i++) {
            displs[i] = displs[i - 1] + word_counts_sizes[i - 1];
        }
    }

    // Step 2: Gather the actual word count data using MPI_Gatherv
    std::vector<int> global_word_counts_flat;
    if (rank == 0) {
        global_word_counts_flat.resize(total_word_data_size);
    }

    MPI_Gatherv(local_word_counts_flat.data(), local_word_count_size, MPI_INT,
                global_word_counts_flat.data(), word_counts_sizes.data(), displs.data(), MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        std::unordered_map<std::string, WordInfo> global_word_counts;
        unflatten_and_aggregate_word_counts(global_word_counts_flat, global_word_counts);

        std::vector<CharCount> char_counts(NUM_CHARS);
        for (int i = 0; i < NUM_CHARS; i++) {
            char_counts[i].character = static_cast<char>(i + ASCII_START);
            char_counts[i].count = global_counts[i];
        }
        std::sort(char_counts.begin(), char_counts.end(), compare_char);

        std::cout << "========= Top 10 Characters =========" << std::endl;
        std::cout << "Ch\tFreq" << std::endl;
        std::cout << "-------------------------------------" << std::endl;
        for (int i = 0; i < 10 && char_counts[i].count > 0; i++) {
            std::cout << char_counts[i].character << "\t" << char_counts[i].count << std::endl;
        }

        std::vector<WordInfo> sorted_word_list;
        sorted_word_list.reserve(global_word_counts.size());
        for (const auto& entry : global_word_counts) {
            sorted_word_list.push_back(entry.second);
        }
        std::sort(sorted_word_list.begin(), sorted_word_list.end(), compare_word);

        std::cout << "\n=========== Top 10 Words ============" << std::endl;
        std::cout << "Word             \tID\tFreq" << std::endl;
        std::cout << "-------------------------------------" << std::endl;
        for (int i = 0; i < 10 && i < sorted_word_list.size(); i++) {
            std::cout << std::left << std::setw(23) << sorted_word_list[i].word << std::setw(10) << sorted_word_list[i].first_occurrence << std::setw(10) << sorted_word_list[i].count << std::endl;
        }
    }

    MPI_File_close(&file);
    MPI_Finalize();
    
    clock_t end_time = clock();
    if (rank == 0) {
        double time_taken = double(end_time - start_time) / CLOCKS_PER_SEC;
        std::cout << "Execution time: " << time_taken << " seconds" << std::endl;
    }

    return 0;
}
