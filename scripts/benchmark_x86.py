import subprocess
import os
import statistics

c_code = '''
#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>

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

  start = __rdtsc();
  result = sum(5, arr);
  end = __rdtsc();

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
    exe_name = 'sum' + opt_level
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
                cycles = int(line.split(':')[1].strip())
                cycles_list.append(cycles)
                break

    # Compute statistics
    avg_cycles = sum(cycles_list) / len(cycles_list)
    stdev_cycles = statistics.stdev(cycles_list)
    median_cycles = statistics.median(cycles_list)
    print(f"Average cycles for {opt_level}: {avg_cycles}")
    print(f"Median cycles for {opt_level}: {median_cycles}")
    print(f"Standard deviation: {stdev_cycles}")
    results.append((opt_level, exe_size, avg_cycles, median_cycles, stdev_cycles))

# Print summary
print("\nSummary:")
header = f"{'Optimization Level':<20}| {'Executable Size (bytes)':<24}| {'Average Cycles':<16}| {'Median Cycles':<15}| {'Std Dev':<10}"
print("-" * len(header))
print(header)
print("-" * len(header))
for opt_level, exe_size, avg_cycles, median_cycles, stdev_cycles in results:
    print(f"{opt_level:<20}| {exe_size:<24}| {avg_cycles:<16.2f}| {median_cycles:<15.2f}| {stdev_cycles:<10.2f}")
print("-" * len(header))
