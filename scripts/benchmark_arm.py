import subprocess
import os
import statistics

c_code = '''
#include <stdio.h>
#include <stdint.h>

// Function to read the Virtual Count Register (cntvct_el0) on ARMv8
static inline uint64_t get_cycle_count() {
    uint64_t cntvct;
    // 'mrs' reads the system register cntvct_el0 into the variable cntvct
    asm volatile("mrs %0, cntvct_el0" : "=r" (cntvct));
    return cntvct;
}

int sum(int size, int* arr) {
    int total = 0;
    for(int i = 0; i < size; i++) {
        total += arr[i];
    }
    return total;
}

int main() {
    int arr[5] = {40, 0, 7, -500, 3000};
    uint64_t start, end;
    int result;

    start = get_cycle_count();
    result = sum(5, arr);
    end = get_cycle_count();

    printf("Result: %d\\n", result);
    printf("Cycles: %llu\\n", (unsigned long long)(end - start));

    return 0;
}
'''

# Save the code to a file
with open('sum.c', 'w') as f:
    f.write(c_code)

optimization_levels = ['-O0', '-Os', '-O3']
num_runs = 10000

results = []

for opt_level in optimization_levels:
    exe_name = 'sum' + opt_level.replace('-', '')  # e.g., sumO0, sumOs, sumO3
    compile_cmd = ['gcc', opt_level, 'sum.c', '-o', exe_name]
    print(f"Compiling with {opt_level}...")
    subprocess.run(compile_cmd, check=True)

    # Get the size of the executable
    exe_size = os.path.getsize(exe_name)
    print(f"Executable size: {exe_size} bytes")

    # Run the executable multiple times
    cycles_list = []
    for i in range(num_runs):
        proc = subprocess.run(['./' + exe_name], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        output = proc.stdout
        # Extract the cycles value from the output
        for line in output.split('\n'):
            if line.startswith('Cycles:'):
                try:
                    cycles = int(line.split(':')[1].strip())
                    cycles_list.append(cycles)
                except ValueError:
                    print(f"Invalid cycle count on run {i+1} with {opt_level}: '{line}'")
                break

    if cycles_list:
        avg_cycles = sum(cycles_list) / len(cycles_list)
        stdev_cycles = statistics.stdev(cycles_list) if len(cycles_list) > 1 else 0.0
        median_cycles = statistics.median(cycles_list)
        print(f"Average cycles for {opt_level}: {avg_cycles}")
        print(f"Median cycles for {opt_level}: {median_cycles}")
        print(f"Standard deviation: {stdev_cycles}\n")
    else:
        avg_cycles = stdev_cycles = median_cycles = None
        print(f"No valid cycle counts collected for {opt_level}.\n")

    results.append((opt_level, exe_size, avg_cycles, median_cycles, stdev_cycles))

# Print summary
print("\nSummary:")
header = f"{'Optimization Level':<20}| {'Executable Size (bytes)':<24}| {'Average Cycles':<16}| {'Median Cycles':<15}| {'Std Dev':<10}"
print("-" * len(header))
print(header)
print("-" * len(header))
for opt_level, exe_size, avg_cycles, median_cycles, stdev_cycles in results:
    avg_str = f"{avg_cycles:.2f}" if avg_cycles is not None else "N/A"
    median_str = f"{median_cycles:.2f}" if median_cycles is not None else "N/A"
    stdev_str = f"{stdev_cycles:.2f}" if stdev_cycles is not None else "N/A"
    print(f"{opt_level:<20}| {exe_size:<24}| {avg_str:<16}| {median_str:<15}| {stdev_str:<10}")
print("-" * len(header))
