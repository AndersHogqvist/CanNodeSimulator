# GitHub Copilot Instructions

## Project Overview
This project is a C++ application that simulates a CAN node based on an eds file. It is designed to be modular and extensible, allowing for easy integration of new features and functionality. The project follows modern C++ best practices and is developed using C++20.

## Code Style
The code style for this project is defined in `.clang-format` and is enforced using clang-format. Documentation comments should be written in Doxygen style. For private fields, use the underscore (_) suffix. For example:

```cpp
class Example {
public:
  void PublicMethod();
private:
  int _private_field;
};
```

The project also uses clang-tidy for static code analysis, and the configuration for clang-tidy is defined in `.clang-tidy`.

**ALWAYS** use the VS Code clangd extension for clang-format and clang-tidy on your code before committing to ensure that it adheres to the project's coding standards.

Some specific guidelines for this project:
- Use two spaces for indentation and avoid using tabs.
- Split the code into multiple files based on functionality.
- Use `.h` for header files, `.cpp` for implementation files and `.hpp` for template classes and header files that contain both declarations and definitions.
- Remove any unused imports and code to keep the codebase clean and maintainable.
- When creating classes, ensure that they have a clear responsibility and follow the Single Responsibility Principle.
- Avoid magic numbers in the code. Instead, use named constants or enums to improve readability and maintainability.
- Use descriptive variable names that clearly indicate the purpose of the function or variable.
- **NEVER** use `NOLINTBEGIN` and `NOLINTEND` to suppress linting errors. Instead, address the underlying issues that are causing the linting errors.
- **ALWAYS** document your code, especially public interfaces, to ensure that other developers can easily understand and use the code. Use Doxygen comments to provide clear explanations of the purpose and functionality of classes, functions, and variables.

## Testing
Unit tests are written using Google Test and are located in the `tests` directory. The tests can be run using CMake and the CTest framework.

**Important**: The tests should be comprehensive and cover all critical functionality of the application. They should also be well-documented to explain the purpose of each test case.

**ALWAYS** write unit tests for any new functionality that you add to the project. This helps ensure that your code works as expected and helps prevent regressions in the future.

## Important Notes
- The code should be well-documented, especially the public interfaces, to ensure that other developers can easily understand and use the code.
- The project should follow best practices for C++ development, including proper memory management and error handling.
- **ALWAYS** prefer standard library features and modern C++ idioms over Boost and other third-party libraries, unless there is a compelling reason to use them.
- **ALWAYS** develop code that is memory efficient and optimized for performance, especially since this application handles real-time CAN communication.
- **ALWAYS** ensure that your code is thread-safe, as the application may involve concurrent operations.
- **NEVER** modify code or create files in the Git sub-modules under `external`.