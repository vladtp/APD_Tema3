#include <iostream>
#include <fstream>
#include <pthread.h>
#include <vector>
#include <mpi.h>
#include <cstring>
#include <algorithm>
#include <sys/sysinfo.h>
#include <cmath>

#define N_PROC 5
#define MASTER 0
#define MASTER_NUM_THREADS 4
#define MASTER_THREAD_HORROR 0
#define MASTER_THREAD_COMEDY 1
#define MASTER_THREAD_FANTASY 2
#define MASTER_THREAD_SF 3
#define HORROR_RANK 1
#define COMEDY_RANK 2
#define FANTASY_RANK 3
#define SF_RANK 4
#define LINES_PER_WOKER 20

struct arg_struct {
    int id;
    char *input;
};

struct worker_arg_struct {
    int id;
    int rank;
};

struct para_struct {
    std::vector<std::string> text;
    int pos;
    int n_lines;
};

// global variables
pthread_mutex_t mutex;
pthread_barrier_t barrier;
int global_turn;
std::ofstream output_file;
int worker_n_threads;
std::vector<para_struct> job_queue;

std::vector<std::string> getParagraph(std::ifstream &f) {
    std::string line;
    std::vector<std::string> para;
    while(getline(f, line) && line.compare("") != 0) {
        para.push_back(line);
    }
    return para;
}

void *master_thread_function(void *arg) {
    // ---INITIALIZIZE--
    arg_struct arguments = *(arg_struct*) arg;
    int id = arguments.id;
    std::string target;
    int worker_rank;

    if (id == MASTER_THREAD_HORROR) {
        target = "horror";
        worker_rank = HORROR_RANK;
    } else if (id == MASTER_THREAD_COMEDY) {
        target = "comedy";
        worker_rank = COMEDY_RANK;
    } else if (id == MASTER_THREAD_FANTASY) {
        target = "fantasy";
        worker_rank = FANTASY_RANK;
    } else if (id == MASTER_THREAD_SF) {
        target = "science-fiction";
        worker_rank = SF_RANK;
    }



    // ---PARSE INPUT AND SEND TEXT TO WORKER---
    std::string line;
    std::ifstream input_file(arguments.input);
    int para_count = 1, job_count = 0;;
    while(getline(input_file, line)) {
        if (line.compare("") == 0) {
            para_count++;
        }
        if (line.compare(target) == 0) {
            std::vector<std::string> para = getParagraph(input_file);
            int para_size = para.size();

            // send paragraph to worker
            job_count++;
            pthread_mutex_lock(&mutex);
            MPI_Send(&para_count, 1, MPI_INT, worker_rank, 0, MPI_COMM_WORLD);
            MPI_Send(&para_size, 1, MPI_INT, worker_rank, 0, MPI_COMM_WORLD);
            for (int i = 0; i < para_size; ++i) {
                MPI_Send(para[i].c_str(), para[i].size() + 1, MPI_CHAR, worker_rank, 0, MPI_COMM_WORLD);
            }
            pthread_mutex_unlock(&mutex);

            para_count++;
        }
    }
    input_file.close();

    // signal that no more paragraphs are coming
    int stop = -1;
    MPI_Send(&stop, 1, MPI_INT, worker_rank, 0, MPI_COMM_WORLD);



    // ---RECIVE MODIFIED TEXT---
    MPI_Status status;
    std::vector<para_struct> processed_paras;
    char buff[2000];

    for (int i = 0; i < job_count; ++i) {
        para_struct new_para;

        // get new paragraph
        std::vector<std::string> para;
        pthread_mutex_lock(&mutex);
        MPI_Recv(&new_para.pos, 1, MPI_INT, worker_rank, 0, MPI_COMM_WORLD, &status);
        MPI_Recv(&new_para.n_lines, 1, MPI_INT, worker_rank, 0, MPI_COMM_WORLD, &status);
        for (int j = 0; j < new_para.n_lines; ++j) {
            MPI_Probe(worker_rank, 0, MPI_COMM_WORLD, &status);
            int line_size;
            MPI_Get_count(&status, MPI_CHAR, &line_size);
            MPI_Recv(buff, line_size, MPI_CHAR, worker_rank, 0, MPI_COMM_WORLD, &status);
            std::string line(buff);
            para.push_back(line);
        }
        pthread_mutex_unlock(&mutex);

        // add paragraph to processed_paras
        new_para.text = para;
        processed_paras.push_back(new_para);
    }



    // ---WRITE TO OUTPUT FILE---
    for (int i = 0; i < processed_paras.size(); ++i) {
        // wait for your turn to write
        while(global_turn != processed_paras[i].pos) {}

        pthread_mutex_lock(&mutex);
        para_struct curr_para = processed_paras[i];
        // output_file.seekp(ios_base::end)
        output_file << target << "\n";
        for(int j = 0; j < curr_para.n_lines; j++) {
            output_file << curr_para.text[j] << "\n";
        }
        output_file << "\n";
        global_turn++;
        pthread_mutex_unlock(&mutex);
    }

    pthread_exit(NULL);
}

// master function
void master(char *input_filename) {
    pthread_t master_threads[MASTER_NUM_THREADS];
    int r;
    void *status;
    arg_struct arguments[MASTER_NUM_THREADS];

    // create output filename
    std::string output_filename(input_filename);
    int dot_pos = output_filename.find_last_of('.');
    output_filename.erase(output_filename.begin() + dot_pos, output_filename.end());
    output_filename += ".out";
    output_file.open(output_filename);

    r = pthread_mutex_init(&mutex, NULL);
    if (r) {
        printf("Eroare la initializarea mutex-ului\n");
        exit(-1);
    }

    for (int id = 0; id < MASTER_NUM_THREADS; id++) {
        arguments[id].id = id;
        arguments[id].input = input_filename;
        r = pthread_create(&master_threads[id], NULL, master_thread_function, (void *) &arguments[id]);
        if (r) {
            printf("Eroare la crearea thread-ului %d\n", id);
            exit(-1);
        }
    }

    for (int id = 0; id < MASTER_NUM_THREADS; id++) {
        r = pthread_join(master_threads[id], &status);
        if (r) {
            printf("Eroare la asteptarea thread-ului %d\n", id);
            exit(-1);
        }
    }

    r = pthread_mutex_destroy(&mutex);
    if (r) {
        printf("Eroare la distrugerea mutex-ului\n");
        exit(-1);
    }
}

std::string horror(std::string input) {
	std::string output;
	for (int i = 0; i < input.size(); ++i) {
		char c = input[i];
        output += c;
		if (strchr("qwrtypsdfghjklzxcvbnmQWRTYPSDFGHJKLZXCVBNM", c) != NULL) {
            output += tolower(c);
		}
	}

    return output;
}

std::string comedy(std::string input) {
	std::string output;
    int k = 1;
	for (int i = 0; i < input.size(); ++i) {
		char c = input[i];
        if (k % 2 == 0) {
            output += toupper(c);
        } else {
            output += c;
        }
        if (c == ' ') {
            k = 1;
        } else {
            k++;
        }
	}
    
    return output;
}

std::string fantasy(std::string input) {
	std::string output;
	for (int i = 0; i < input.size(); ++i) {
		char c = input[i];
        if (i == 0 || input[i - 1] == ' ') {
            output += toupper(c);
        } else {
            output += c;
        }
	}
    
    return output;
}

std::string sf(std::string input) {
	std::string output;
	int pos = 0;
    int k = 0;
    std::string token;
    while ((pos = input.find(" ")) != std::string::npos) {
        k++;
        token = input.substr(0, pos);
        if (k % 7 == 0) {
            std::reverse(token.begin(), token.end());
        }
        output += token;
        output += " ";
        input.erase(0, pos + 1);
    }
    k++;
    if (k % 7 == 0) {
        std::reverse(input.begin(), input.end());
    }
    output += input;
    
    return output;
}

void *worker_thread_gateway(void *arg) {
    int para_size, para_count;
    MPI_Status status;
    char buff[2000];

    // recieve all paragraphs that need to be processed
    MPI_Recv(&para_count, 1, MPI_INT, MASTER, 0, MPI_COMM_WORLD, &status);
    while (para_count != -1) {
        para_struct new_job;

        // get new paragraph
        std::vector<std::string> para;
        MPI_Recv(&para_size, 1, MPI_INT, MASTER, 0, MPI_COMM_WORLD, &status);
        for (int i = 0; i < para_size; ++i) {
            MPI_Probe(MASTER, 0, MPI_COMM_WORLD, &status);
            int line_size;
            MPI_Get_count(&status, MPI_CHAR, &line_size);
            MPI_Recv(buff, line_size, MPI_CHAR, MASTER, 0, MPI_COMM_WORLD, &status);
            std::string line(buff);
            para.push_back(line);
        }

        // add paragraph to job queue
        new_job.text = para;
        new_job.n_lines = para_size;
        new_job.pos = para_count;
        job_queue.push_back(new_job);


        // if para_size = -1 then no more jobs are coming
        MPI_Recv(&para_count, 1, MPI_INT, MASTER, 0, MPI_COMM_WORLD, &status);
    }

    pthread_barrier_wait(&barrier);
    
    pthread_barrier_wait(&barrier);

    // send processed paragraphs back to master
    int n_jobs = job_queue.size();
    for (int i = 0; i < n_jobs; ++i) {
        para_struct job = job_queue[i];

        MPI_Send(&job.pos, 1, MPI_INT, MASTER, 0, MPI_COMM_WORLD);
        MPI_Send(&job.n_lines, 1, MPI_INT, MASTER, 0, MPI_COMM_WORLD);
        for (int j = 0; j < job.n_lines; ++j) {
            MPI_Send(job.text[j].c_str(), job.text[j].size() + 1, MPI_CHAR, MASTER, 0, MPI_COMM_WORLD);
        }
    }

    pthread_exit(NULL);
}

void *worker_thread_process(void *arg) {
    worker_arg_struct arguments = *(worker_arg_struct*) arg;
    int rank = arguments.rank;
    int id = arguments.id - 1;
    pthread_barrier_wait(&barrier);

    int n_jobs = job_queue.size();
    for (int job_id = 0; job_id < n_jobs; ++job_id) {
        int lines = job_queue[job_id].n_lines;
        for (int step = 0; step < lines; step += (worker_n_threads - 1) * LINES_PER_WOKER) {
            int start = id * LINES_PER_WOKER + step;
            int finish = std::min((id + 1) * LINES_PER_WOKER + step, lines);

            if (start < lines) {
                for (int i = start; i < finish; ++i) {
                    if (rank == 1) {
                        job_queue[job_id].text[i] = horror(job_queue[job_id].text[i]);
                    } else if (rank == 2) {
                        job_queue[job_id].text[i] = comedy(job_queue[job_id].text[i]);
                    } else if (rank == 3) {
                        job_queue[job_id].text[i] = fantasy(job_queue[job_id].text[i]);
                    } else {
                        job_queue[job_id].text[i] = sf(job_queue[job_id].text[i]);
                    }
                }
            }
        }
    }
    
    pthread_barrier_wait(&barrier);

    pthread_exit(NULL);
}

void worker(int rank) {
    worker_n_threads = get_nprocs();
    pthread_t worker_threads[worker_n_threads];
    worker_arg_struct arguments[worker_n_threads];
    int r;
    void *status;

    pthread_mutex_init(&mutex, NULL);

    r = pthread_barrier_init(&barrier, NULL, worker_n_threads);
    if (r) {
        printf("Eroare la crearea barierei\n");
        exit(-1);
    }

    arguments[0].id = 0;
    arguments[0].rank = rank;
    r = pthread_create(&worker_threads[0], NULL, worker_thread_gateway, (void *) &arguments[0]);
    if (r) {
        printf("Eroare la crearea thread-ului %d\n", 0);
        exit(-1);
    }

    for (int id = 1; id < worker_n_threads; ++id) {
        arguments[id].id = id;
        arguments[id].rank = rank;

        r = pthread_create(&worker_threads[id], NULL, worker_thread_process, (void *) &arguments[id]);
        if (r) {
            printf("Eroare la crearea thread-ului %d\n", id);
            exit(-1);
        }
    }

    for (int id = 0; id < worker_n_threads; id++) {
        r = pthread_join(worker_threads[id], &status);
        if (r) {
            printf("Eroare la asteptarea thread-ului %d\n", id);
            exit(-1);
        }
    }

    pthread_barrier_destroy(&barrier);
    if (r) {
        printf("Eroare la distrugerea barierei\n");
        exit(-1);
    }

    pthread_mutex_destroy(&mutex);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Numar de argumente incorect!\n");
        exit(-1);
    }
    char *input_filename = argv[1];
    global_turn = 1;

    int provided, rank, r;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == MASTER) {
        master(input_filename);
    } else {
        worker(rank);
    }

    MPI_Finalize();
    return 0;
}