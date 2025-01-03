Contributing
============

Contributions are welcome! We appreciate your interest in improving the **Modbus Master/Slave Library in C**. To ensure a smooth and efficient collaboration process, please follow the guidelines outlined below.

## How to Contribute

Follow these steps to contribute to the project:

### 1. Fork the Repository

Click the "Fork" button on the [repository page](https://github.com/yourusername/modbus-library) to create a personal copy of the project in your GitHub account.

### 2. Clone Your Fork

Clone your forked repository to your local machine using Git:

```bash
git clone https://github.com/yourusername/modbus-library.git
cd modbus-library

3. Create a New Branch

Create a new branch for your feature or bugfix. Replace feature/YourFeatureName with a descriptive name for your branch:

git checkout -b feature/YourFeatureName

4. Make Your Changes

Implement your feature or fix the bug. Ensure that your code adheres to the project's coding standards and includes appropriate documentation. Remember to:

    Write clear and concise code.
    Follow the existing code style and naming conventions.
    Add or update comments and documentation as needed.

5. Run Tests

Before committing your changes, make sure all existing tests pass and add new tests for your changes if applicable:

cmake --preset release
cmake --build --preset release
# Run your test commands here

6. Commit Your Changes

Commit your changes with a meaningful commit message:

git add .
git commit -m "Add feature: Your Feature Description"

7. Push to Your Fork

Push your changes to your forked repository:

git push origin feature/YourFeatureName

8. Create a Pull Request

Navigate to the original repository on GitHub and click the "Compare & pull request" button. Provide a clear description of your changes, the problem they solve, and any other relevant information.
Contribution Guidelines

To maintain the quality and consistency of the project, please adhere to the following guidelines:
Coding Standards

    Language: C99 or later.
    Style: Follow the existing code style, including indentation, naming conventions, and file organization.
    Documentation: Use Doxygen comments for all public functions, structures, and enumerations.

Writing Documentation

    Update or add documentation for any new features or changes.
    Ensure that all documentation is clear, concise, and free of errors.
    Follow the project's documentation structure and guidelines.

Testing

    Write unit tests for new features or bug fixes.
    Ensure that all tests pass before submitting your contribution.
    Follow the project's testing conventions and structure.

Code Review

    Be open to feedback and willing to make necessary changes.
    Address all comments and suggestions provided during the review process.
    Maintain a respectful and collaborative attitude throughout the review.

Issue Tracking

    Before submitting a pull request, check the issue tracker to see if your issue or feature request has already been reported.
    If you find an existing issue that matches your contribution, reference it in your pull request.
    If your contribution addresses multiple issues, consider splitting them into separate pull requests.

Reporting Bugs and Requesting Features

If you encounter a bug or have a feature request, please open an issue on the issue tracker. Provide as much detail as possible, including:

    A clear and descriptive title.
    Steps to reproduce the bug.
    Expected and actual behavior.
    Any relevant code snippets or screenshots.

License

By contributing to this project, you agree that your contributions will be licensed under the MIT License.
Acknowledgements

We appreciate all contributors for their time and effort in improving this project. Your contributions help make the Modbus Master/Slave Library in C better for everyone.