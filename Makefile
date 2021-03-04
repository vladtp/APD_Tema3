build:
	mpic++ -std=c++11 main.cpp -o main -lpthread
run:
	mpirun -np 5 --oversubscribe main test.in
clean:
	rm main