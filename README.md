# Virginia Tech ECE5544: Compiler Optimization

## Project Overview

This project is part of the Virginia Tech ECE5544 course on Compiler Optimization. We have developed a custom LLVM pass specifically for Conditional Constant Folding. This pass aims to optimize specific conditional constant scenarios within the code, enhancing execution efficiency.

## Implemented Features

- **Custom LLVM Pass:** A specialized Conditional Constant Folding pass designed to optimize conditional constants more effectively within compiled programs.

- **Benchmarking:** The implementation has been benchmarked using custom microbenchmarks to demonstrate the effectiveness and impact of the Conditional Constant Folding pass on code optimization.

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes.

### Prerequisites

Ensure you have LLVM installed on your system along with the necessary development tools like `make` and `clang`.

### Installation

1. **Clone the Repository**

   Start by cloning this repository on your local machine:

   ```bash
   git clone https://github.com/AnantaSrikar/ConstantFolding
   cd ConstantFolding
   ```

2. **Compile the LLVM Pass and Benchmarks**

   Compile the custom LLVM pass along with the necessary benchmarks using:

   ```bash
   make
   ```

   This command builds all required components of the project.

### Usage

After compiling the components, you can run the benchmarks to see the effects of the Conditional Constant Folding pass:

1. **Run the Benchmark**

   Execute the benchmarks using the provided script:

   ```bash
   ./run.sh
   ```

   This script runs the benchmark suite against the LLVM pass, applying the optimizations and outputting the results.

2. **Analyzing the Output**

   Review the output from the benchmark runs to understand the performance and optimization improvements provided by the custom LLVM pass.

### Cleaning Up

To clean up your directory and remove all compiled outputs:

```bash
make clean
```

This command will clean the project directory, ensuring that all build artifacts are removed.

## Contributing

Contributions are welcome from students and faculty. Please fork the repository, make your changes, and submit a pull request for review.

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE) file for details.

## Acknowledgments

- Thanks to Virginia Tech's faculty and ECE5544 course staff and the TA for guidance and support throughout the project!
```
