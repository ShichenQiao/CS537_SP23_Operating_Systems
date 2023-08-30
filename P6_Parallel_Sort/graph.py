import subprocess
import time
import matplotlib.pyplot as plt

def run_command(command):
    start_time = time.time()

    # run the command using subprocess
    process = subprocess.Popen(command, shell=True)
    process.wait()

    end_time = time.time()
    execution_time = end_time - start_time

    #with open("time.log", "r") as timelog:
    #    line = timelog.readlines()
    #execution_time = float(line[0])

    print(f"Execution Time: {execution_time:.5f} seconds")
    return execution_time

def main():
    input_file_name = "test.img"
    num_runs = 10

    commands = [
        "a.out " + input_file_name + " test.out 1",
        "a.out " + input_file_name + " test.out 2",
        "a.out " + input_file_name + " test.out 4",
        "a.out " + input_file_name + " test.out 8",
        "a.out " + input_file_name + " test.out 12",
        "a.out " + input_file_name + " test.out 16",
    ]

    execution_times = []

    for command in commands:
        command_times = []
        for i in range(num_runs):
            command_times.append(run_command(command))
        avg_time = sum(command_times) / num_runs
        execution_times.append(avg_time)

    # plot the execution times as a line graph
    plt.plot(["1", "2", "4", "8", "12", "16"], execution_times, '-o')
    plt.xlabel("Number of Threads")
    plt.ylabel("Average Execution Time (seconds)")
    plt.title("Input File Size = 10^9 B, Iteration = " + str(num_runs))
    plt.show()

if __name__ == '__main__':
    main()
