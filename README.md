# MyMake - A Simplified Make Program

`./mymake` is a simplified version of the UNIX `make` utility. It executes commands based on dependencies specified in a makefile, supports macros, target rules, inference rules, and provides control over error handling, debugging, and timeouts.

## Features

- **Support for Makefile Macros:** Uses variables defined in the makefile and environment.
- **Target Rules:** Builds targets based on specified prerequisites.
- **Inference Rules:** Automates target generation using pattern matching and variables like `$@` (target name) and `$<` (first prerequisite).
- **Error Handling:** Options to continue on errors or terminate on failure.
- **Debugging:** Displays detailed information about rule applications and command execution.
- **Signal Handling:** Allows blocking of signals like `SIGINT` (Ctrl-C) to prevent interruption.
- **Timeout Handling:** Allows graceful termination of the program if a command exceeds a specified time limit.

## Prerequisites

1. **Environment Setup:**
    - Ensure remote servers are accessible via SSH.
    - Set up the `MYPATH` environment variable to define custom search paths for commands.

2. **Makefile:**
    - The default makefile used is `mymake3.mk`.

## Getting Started

### Download and Setup

1. **Download the .tar file.**
2. **Extract the files** using the following command:
   ```bash
   $ tar -xf <FileName>.tar
   ```
3. **Compile the program:**
   ```bash
   $ make
   ```

4. **Run MyMake:**
   ```bash
   $ ./mymake [options] [target] [options]
   ```

### Cleaning Files

To clean the generated files (executables, object files, backup files, etc.), run:
```bash
$ make clean
```

## Command Line Options

MyMake supports several options that modify its behavior:

| Option     | Description                                                                                              | Example                                  |
|------------|----------------------------------------------------------------------------------------------------------|------------------------------------------|
| `-f mf`    | Use a specified makefile instead of the default `mymake3.mk`.                                              | `$ ./mymake -f makefile2`                |
| `-p`       | Build the rules database from the makefile and display it without executing commands.                      | `$ ./mymake -p`                          |
| `-k`       | Continue executing commands even if some fail.                                                            | `$ ./mymake -k`                          |
| `-d`       | Print debugging information during execution, including applied rules and executed actions.                | `$ ./mymake -d`                          |
| `-i`       | Block the `SIGINT` signal (Ctrl-C) to prevent interruption.                                               | `$ ./mymake -i`                          |
| `-t num`   | Set a timeout for command execution (in seconds). If the program doesn’t finish within the timeout, it gracefully self-destructs. | `$ ./mymake -t 30`                       |
| `[target]` | Specify a target to build. If no target is provided, the first target in the makefile is built by default. | `$ ./mymake clean`                       |

## Usage Examples

1. **Specify a custom makefile:**
   ```bash
   $ ./mymake -f makefile2
   ```

2. **Build a specific target:**
   ```bash
   $ ./mymake -f makefile2 clean
   ```

3. **Continue on command failure:**
   ```bash
   $ ./mymake -f makefile2 -k
   ```

4. **Build the rules database and output it:**
   ```bash
   $ ./mymake -f makefile2 -p
   ```

5. **Run with timeout and debug mode:**
   ```bash
   $ ./mymake -f makefile2 -t 30 -d
   ```

## Additional Features

- **Comments:** Supports comments in the makefile using `#`.
- **Macro Substitution:** Use macros in commands with `$string` or `$(string)`.
- **Special Symbols:**
  - `$@` – Refers to the target name.
  - `$<` – Refers to the first prerequisite.
- **Command Redirection:**
  - Use `>` to redirect output to a file.
  - Use `<` to redirect input from a file.
- **Sequential Commands:** Use `;` to execute multiple commands sequentially.
- **Pipes:** Commands separated by `|` can be piped, passing output of one command as input to the next.
- **Circular Dependency Detection:** Handles circular dependencies in the makefile and reports errors.
- **Timeout and Signal Handling:** Terminates gracefully on timeout or upon receiving signals like `SIGINT` (Ctrl-C).

## Debugging

Use the `-d` option to print debugging information, including:
- The rule being applied to a target.
- The commands being executed.
- Messages when a target is up-to-date or when dependencies are being rebuilt.

Example:
```bash
$ ./mymake -d
```

## Error Handling

If a command fails, you can either:
- **Continue execution:** Use the `-k` flag to proceed with the next commands even after a failure.
- **Terminate on error:** By default, MyMake will terminate if a command fails, unless `-k` is specified.

## Limitations

- **Unknown or unexpected options** will cause the program to throw an error and terminate.
- **Connection Loss or Fork Failure:** If the program encounters loss of connection or failure to create child processes, errors will be printed to the console.

## License

This project is open-source and is licensed under the MIT License.

---

This `README` outlines the usage, features, and options of `mymake`. It provides guidance on how to set up and run the program, handle errors, and configure the environment for optimal performance.
